#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/FrameShm.h"
#include "hardware/camera/VideoTransformPipeline.h"

#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace htb {

namespace {

constexpr size_t kMaxSourceFrame = static_cast<size_t>(1920) * 1080 * 3 / 2;
constexpr uint32_t kOutWidth = kFrameShmOutWidth;
constexpr uint32_t kOutHeight = kFrameShmOutHeight;
constexpr size_t kOutFrameSize = static_cast<size_t>(kOutWidth) * kOutHeight * 3 / 2;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float sampleBilinear(const uint8_t* plane, int pw, int ph, float x, float y) {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x0 >= pw - 1) x0 = pw - 2;
    if (y0 >= ph - 1) y0 = ph - 2;
    const float fx = x - x0;
    const float fy = y - y0;
    const uint8_t* p = plane + static_cast<size_t>(y0) * pw + x0;
    const float v00 = p[0];
    const float v10 = p[1];
    const float v01 = p[pw];
    const float v11 = p[pw + 1];
    return (v00 * (1.0f - fx) + v10 * fx) * (1.0f - fy) + (v01 * (1.0f - fx) + v11 * fx) * fy;
}

void transformNv12(const uint8_t* src, int sw, int sh, uint8_t* dst, const CameraOutputParams& p) {
    const int dw = static_cast<int>(kOutWidth);
    const int dh = static_cast<int>(kOutHeight);

    const float scale = std::min(static_cast<float>(dw) / sw, static_cast<float>(dh) / sh);
    const float fitW = sw * scale;
    const float fitH = sh * scale;
    const float ox = (dw - fitW) * 0.5f;
    const float oy = (dh - fitH) * 0.5f;

    const float zoom = clampf(p.zoom, 1.0f, 3.0f);
    const float cw = fitW / zoom;
    const float ch = fitH / zoom;
    const float panX = clampf(p.panX, -1.0f, 1.0f);
    const float panY = clampf(p.panY, -1.0f, 1.0f);
    const float maxDx = std::max(0.0f, (fitW - cw) * 0.5f);
    const float maxDy = std::max(0.0f, (fitH - ch) * 0.5f);
    const float cx = (fitW - cw) * 0.5f + panX * maxDx + ox;
    const float cy = (fitH - ch) * 0.5f + panY * maxDy + oy;

    const float alpha = 1.0f + p.contrast / 100.0f;
    const float brightness = p.brightness * 2.55f;
    const float sat = 1.0f + p.saturation / 100.0f;
    const bool flipH = p.flipHorizontal;
    const bool flipV = p.flipVertical;
    const bool rot180 = (p.rotation % 360) == 180;

    const uint8_t* srcUV = src + static_cast<size_t>(sw) * sh;
    const int suw = sw / 2;
    const int suh = sh / 2;
    const int duw = dw / 2;

    for (int dy = 0; dy < dh; ++dy) {
        uint8_t* row = dst + static_cast<size_t>(dy) * dw;
        for (int dx = 0; dx < dw; ++dx) {
            float yv = 16.0f;
            if (dx >= cx && dx < cx + cw && dy >= cy && dy < cy + ch) {
                float sx = (dx - cx) / cw * sw;
                float sy = (dy - cy) / ch * sh;
                if (rot180) {
                    sx = sw - 1 - sx;
                    sy = sh - 1 - sy;
                }
                if (flipH) sx = sw - 1 - sx;
                if (flipV) sy = sh - 1 - sy;
                yv = sampleBilinear(src, sw, sh, sx, sy);
            }
            yv = (yv - 16.0f) * alpha + 16.0f + brightness;
            if (yv < 0.0f) yv = 0.0f;
            if (yv > 255.0f) yv = 255.0f;
            row[dx] = static_cast<uint8_t>(yv);
        }
    }

    for (int dy = 0; dy < dh / 2; ++dy) {
        uint8_t* row = dst + static_cast<size_t>(dw) * dh + static_cast<size_t>(dy) * duw * 2;
        for (int dx = 0; dx < duw; ++dx) {
            float u = 128.0f;
            float v = 128.0f;
            const float px = dx * 2.0f + 0.5f;
            const float py = dy * 2.0f + 0.5f;
            if (px >= cx && px < cx + cw && py >= cy && py < cy + ch) {
                float sx = (px - cx) / cw * sw;
                float sy = (py - cy) / ch * sh;
                if (rot180) {
                    sx = sw - 1 - sx;
                    sy = sh - 1 - sy;
                }
                if (flipH) sx = sw - 1 - sx;
                if (flipV) sy = sh - 1 - sy;
                const float ux = sx * 0.5f;
                const float uy = sy * 0.5f;
                u = sampleBilinear(srcUV, suw, suh, ux, uy);
                v = sampleBilinear(srcUV + 1, suw, suh, ux, uy);
            }
            u = (u - 128.0f) * sat + 128.0f;
            v = (v - 128.0f) * sat + 128.0f;
            if (u < 0.0f) u = 0.0f;
            if (u > 255.0f) u = 255.0f;
            if (v < 0.0f) v = 0.0f;
            if (v > 255.0f) v = 255.0f;
            row[dx * 2] = static_cast<uint8_t>(u);
            row[dx * 2 + 1] = static_cast<uint8_t>(v);
        }
    }
}

} // namespace

struct VideoTransformPipeline::Impl {
    std::unique_ptr<std::thread> thread;
    std::atomic<bool> running{false};
    std::mutex shmMutex;
    HANDLE shmMap = nullptr;
    uint8_t* shmView = nullptr;
    uint32_t generation = 0;
    uint64_t frameIndex = 0;
};

VideoTransformPipeline::VideoTransformPipeline() : m_impl(std::make_unique<Impl>()) {}

VideoTransformPipeline::~VideoTransformPipeline() {
    stop();
}

void VideoTransformPipeline::start(const CameraOutputParams& params) {
    if (m_impl->running.exchange(true)) return;
    m_params.store(params);
    m_impl->thread = std::make_unique<std::thread>([this] { loop(); });
}

void VideoTransformPipeline::updateParams(const CameraOutputParams& params) {
    m_params.store(params);
}

void VideoTransformPipeline::stop() {
    if (!m_impl->running.exchange(false)) return;
    if (m_impl->thread && m_impl->thread->joinable()) m_impl->thread->join();
    std::lock_guard lock(m_impl->shmMutex);
    if (m_impl->shmView) UnmapViewOfFile(m_impl->shmView);
    if (m_impl->shmMap) CloseHandle(m_impl->shmMap);
    m_impl->shmMap = nullptr;
    m_impl->shmView = nullptr;
}

void VideoTransformPipeline::writeSharedMemory(const uint8_t* frame, size_t size) {
    std::lock_guard lock(m_impl->shmMutex);
    if (!m_impl->shmMap) {
        m_impl->shmMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                            static_cast<DWORD>(kFrameShmTotalSize), kFrameShmName);
        if (m_impl->shmMap) {
            m_impl->shmView = static_cast<uint8_t*>(MapViewOfFile(m_impl->shmMap, FILE_MAP_WRITE, 0, 0, 0));
        }
    }
    if (!m_impl->shmMap || !m_impl->shmView) return;
    auto* header = reinterpret_cast<FrameShmHeader*>(m_impl->shmView);
    memcpy(m_impl->shmView + sizeof(FrameShmHeader), frame, size);
    header->magic = kFrameShmMagic;
    header->width = kOutWidth;
    header->height = kOutHeight;
    header->dataSize = static_cast<uint32_t>(size);
    header->frameIndex = ++m_impl->frameIndex;
    header->timestamp100ns = std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
    header->sourceActive = 1;
    std::atomic_thread_fence(std::memory_order_release);
    header->generation = ++m_impl->generation;
}

void VideoTransformPipeline::loop() {
    auto status = std::make_shared<CameraOutputStatus>();
    status->running = true;
    m_status.store(status);

    auto cameraParams = m_params.load();

    HRESULT hr = S_OK;
    ComPtr<IMFMediaSource> source;
    {
        ComPtr<IMFAttributes> attrs;
        hr = MFCreateAttributes(&attrs, 2);
        if (SUCCEEDED(hr)) {
            attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        }
        IMFActivate** devices = nullptr;
        UINT32 count = 0;
        if (SUCCEEDED(hr) && SUCCEEDED(MFEnumDeviceSources(attrs.Get(), &devices, &count))) {
            if (cameraParams.cameraIndex < 0 || static_cast<UINT32>(cameraParams.cameraIndex) >= count) {
                status->message = "摄像头索引无效";
            } else {
                hr = devices[cameraParams.cameraIndex]->ActivateObject(IID_PPV_ARGS(&source));
            }
            for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
            CoTaskMemFree(devices);
        } else {
            status->message = "未检测到摄像头";
        }
    }

    ComPtr<IMFSourceReader> reader;
    if (SUCCEEDED(hr)) {
        hr = MFCreateSourceReaderFromMediaSource(source.Get(), nullptr, &reader);
    }

    UINT32 sourceWidth = 0;
    UINT32 sourceHeight = 0;
    if (SUCCEEDED(hr)) {
        ComPtr<IMFMediaType> nv12;
        hr = MFCreateMediaType(&nv12);
        if (SUCCEEDED(hr)) {
            nv12->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            nv12->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            hr = reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr,
                                             nv12.Get());
        }
        ComPtr<IMFMediaType> actual;
        if (SUCCEEDED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                                  &actual))) {
            MFGetAttributeSize(actual.Get(), MF_MT_FRAME_SIZE, &sourceWidth, &sourceHeight);
        }
    }

    if (FAILED(hr) || !reader) {
        if (status->message.empty()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "无法打开摄像头 (0x%08X)", static_cast<unsigned>(hr));
            status->message = buf;
        }
        status->running = false;
        m_status.store(status);
        return;
    }

    status->capturing = true;
    status->sourceWidth = sourceWidth;
    status->sourceHeight = sourceHeight;
    status->message = "输出中";
    m_status.store(status);
    HTB_INFO("[camera] output pipeline started: {}x{} source", sourceWidth, sourceHeight);

    std::vector<uint8_t> srcFrame(kMaxSourceFrame);
    std::vector<uint8_t> outFrame(kOutFrameSize);

    uint64_t framesSent = 0;
    const auto lastFpsUpdate = std::chrono::steady_clock::now();
    uint64_t windowFrames = 0;

    while (m_impl->running.load()) {
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* sample = nullptr;
        hr = reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, &streamIndex, &flags,
                                &timestamp, &sample);
        if (FAILED(hr)) {
            status->message = "读取帧失败";
            break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            status->message = "视频流结束";
            break;
        }
        if (!sample) {
            continue;
        }

        ComPtr<IMFMediaBuffer> buffer;
        if (SUCCEEDED(sample->GetBufferByIndex(0, &buffer))) {
            BYTE* data = nullptr;
            DWORD maxLen = 0;
            DWORD curLen = 0;
            if (SUCCEEDED(buffer->Lock(&data, &maxLen, &curLen))) {
                const size_t frameSize = static_cast<size_t>(sourceWidth) * sourceHeight * 3 / 2;
                if (curLen >= frameSize && frameSize <= srcFrame.size()) {
                    memcpy(srcFrame.data(), data, frameSize);
                    transformNv12(srcFrame.data(), static_cast<int>(sourceWidth), static_cast<int>(sourceHeight),
                                  outFrame.data(), m_params.load());
                    writeSharedMemory(outFrame.data(), outFrame.size());
                    ++framesSent;
                    ++windowFrames;
                }
                buffer->Unlock();
            }
        }
        sample->Release();

        const auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - lastFpsUpdate).count();
        if (elapsed >= 2.0) {
            auto updated = std::make_shared<CameraOutputStatus>(*status);
            updated->fps = windowFrames / elapsed;
            updated->framesSent = framesSent;
            windowFrames = 0;
            m_status.store(updated);
        }
    }

    auto finalStatus = std::make_shared<CameraOutputStatus>(*status);
    finalStatus->framesSent = framesSent;
    finalStatus->capturing = false;
    finalStatus->running = false;
    if (finalStatus->message.empty()) finalStatus->message = "已停止";
    m_status.store(finalStatus);
    HTB_INFO("[camera] output pipeline stopped: {}", finalStatus->message);
}

} // namespace htb
