import ctypes
import json
import os
import sys
import time

import cv2
import numpy as np
import pyvirtualcam

PARAMS_PATH = os.path.join(os.environ.get("TEMP", "."), "htb_cam_params.json")
STATUS_PATH = os.path.join(os.environ.get("TEMP", "."), "htb_cam_status.json")

DEFAULT_PARAMS = {
    "camera_index": 0,
    "zoom": 1.0,
    "pan_x": 0.0,
    "pan_y": 0.0,
    "flip_h": False,
    "flip_v": False,
    "brightness": 0,
    "contrast": 0,
    "saturation": 0,
    "running": True,
}

OUT_WIDTH = 1280
OUT_HEIGHT = 720
OUT_FPS = 30


def load_params():
    try:
        with open(PARAMS_PATH, "r", encoding="utf-8") as f:
            p = json.load(f)
        for k, v in DEFAULT_PARAMS.items():
            p.setdefault(k, v)
        return p
    except Exception:
        return dict(DEFAULT_PARAMS)


def write_status(status):
    try:
        with open(STATUS_PATH, "w", encoding="utf-8") as f:
            json.dump(status, f)
    except Exception:
        pass


def parent_alive(pid):
    try:
        PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
        h = ctypes.windll.kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
        if not h:
            return False
        code = ctypes.c_ulong()
        ctypes.windll.kernel32.GetExitCodeProcess(h, ctypes.byref(code))
        ctypes.windll.kernel32.CloseHandle(h)
        return code.value == 259
    except Exception:
        return True


def compute_crop(w, h, zoom, pan_x, pan_y):
    if abs(zoom - 1.0) <= 0.01:
        return 0, 0, w, h
    new_w = max(1, min(int(w / zoom), w))
    new_h = max(1, min(int(h / zoom), h))
    pan_x = max(-1.0, min(1.0, pan_x))
    pan_y = max(-1.0, min(1.0, pan_y))
    max_dx = max(0, (w - new_w) / 2.0)
    max_dy = max(0, (h - new_h) / 2.0)
    x = int(round((w - new_w) / 2.0 + pan_x * max_dx))
    y = int(round((h - new_h) / 2.0 + pan_y * max_dy))
    x = max(0, min(x, w - new_w))
    y = max(0, min(y, h - new_h))
    return x, y, new_w, new_h


def fit_to_size(frame, out_w, out_h):
    h, w = frame.shape[:2]
    if w <= 0 or h <= 0:
        return frame
    scale = min(out_w / w, out_h / h)
    new_w = max(1, int(round(w * scale)))
    new_h = max(1, int(round(h * scale)))
    if (new_w, new_h) != (w, h):
        frame = cv2.resize(frame, (new_w, new_h), interpolation=cv2.INTER_AREA)
    if (new_w, new_h) == (out_w, out_h):
        return frame
    canvas = np.zeros((out_h, out_w, 3), dtype=np.uint8)
    x0 = (out_w - new_w) // 2
    y0 = (out_h - new_h) // 2
    canvas[y0:y0 + new_h, x0:x0 + new_w] = frame
    return canvas


def transform(frame, p):
    if p["flip_h"] and p["flip_v"]:
        frame = cv2.flip(frame, -1)
    elif p["flip_h"]:
        frame = cv2.flip(frame, 1)
    elif p["flip_v"]:
        frame = cv2.flip(frame, 0)

    if p["brightness"] != 0 or p["contrast"] != 0:
        alpha = 1.0 + p["contrast"] / 100.0
        frame = cv2.convertScaleAbs(frame, alpha=alpha, beta=float(p["brightness"]))

    if p["saturation"] != 0:
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        lut = np.clip(np.arange(256) * (1.0 + p["saturation"] / 100.0), 0, 255).astype(np.uint8)
        hsv[:, :, 1] = cv2.LUT(hsv[:, :, 1], lut)
        frame = cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

    h, w = frame.shape[:2]
    x, y, cw, ch = compute_crop(w, h, p["zoom"], p["pan_x"], p["pan_y"])
    if (cw, ch) != (w, h):
        frame = frame[y:y + ch, x:x + cw]

    return fit_to_size(frame, OUT_WIDTH, OUT_HEIGHT)


def main():
    parent_pid = None
    args = sys.argv
    for i, a in enumerate(args):
        if a == "--parent-pid" and i + 1 < len(args):
            parent_pid = int(args[i + 1])

    params = load_params()
    if not params.get("running", True):
        return 0

    cam = None
    cap = None
    frames = 0
    last = time.time()
    fps_win = 0.0

    try:
        cam = pyvirtualcam.Camera(width=OUT_WIDTH, height=OUT_HEIGHT, fps=OUT_FPS)
    except Exception as e:
        write_status({"running": False, "fps": 0, "frames": 0,
                      "error": "无法打开 OBS Virtual Camera: %s" % e, "source": ""})
        return 1

    try:
        cap = cv2.VideoCapture(params["camera_index"], cv2.CAP_DSHOW)
        if not cap.isOpened():
            write_status({"running": False, "fps": 0, "frames": 0,
                          "error": "无法打开摄像头 %d（可能被其他应用占用）" % params["camera_index"], "source": ""})
            return 1
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        for _ in range(10):
            cap.grab()
        sw = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        sh = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    except Exception as e:
        write_status({"running": False, "fps": 0, "frames": 0,
                      "error": "摄像头初始化失败: %s" % e, "source": ""})
        return 1

    write_status({"running": True, "fps": 0, "frames": 0, "error": "", "source": "%dx%d" % (sw, sh)})

    while parent_alive(parent_pid) if parent_pid else True:
        params = load_params()
        if not params.get("running", True):
            break

        ok, frame = cap.read()
        if not ok:
            write_status({"running": True, "fps": fps_win, "frames": frames,
                          "error": "读取帧失败，正在重试...", "source": "%dx%d" % (sw, sh)})
            time.sleep(0.5)
            continue

        try:
            out = transform(frame, params)
            cam.send(out)
            cam.sleep_until_next_frame()
            frames += 1
        except Exception:
            pass

        now = time.time()
        if now - last >= 2.0:
            fps_win = frames / (now - last) if now > last else 0.0
            write_status({"running": True, "fps": round(fps_win, 1), "frames": frames,
                          "error": "", "source": "%dx%d" % (sw, sh)})
            last = now

    if cap is not None:
        cap.release()
    write_status({"running": False, "fps": 0, "frames": frames, "error": "", "source": ""})
    return 0


if __name__ == "__main__":
    sys.exit(main())
