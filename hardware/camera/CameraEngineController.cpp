#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/CameraEngineController.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace htb {

namespace {
std::string tempDir() {
    wchar_t buf[MAX_PATH]{};
    GetTempPathW(MAX_PATH, buf);
    return toUtf8(buf);
}

std::string moduleDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    return toUtf8(path);
}

std::string findPython() {
    const std::string candidates[] = {
        "C:\\Users\\Lenovo\\Desktop\\streamlens-virtual-cam\\venv\\Scripts\\python.exe",
        "C:\\Users\\Lenovo\\Desktop\\streamlens-virtual-cam\\venv_broken_314\\Scripts\\python.exe",
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return "python.exe";
}

void parseStatusJson(const std::string& text, CameraEngineStatus& out) {
    auto extract = [&](const char* key) -> std::string {
        const std::string k = std::string("\"") + key + "\"";
        const size_t pos = text.find(k);
        if (pos == std::string::npos) return {};
        const size_t colon = text.find(':', pos + k.size());
        if (colon == std::string::npos) return {};
        size_t start = colon + 1;
        while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
        if (start >= text.size()) return {};
        if (text[start] == '"') {
            const size_t end = text.find('"', start + 1);
            if (end == std::string::npos) return {};
            return text.substr(start + 1, end - start - 1);
        }
        size_t end = start;
        while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != '\n') ++end;
        return text.substr(start, end - start);
    };
    const std::string running = extract("running");
    const std::string fps = extract("fps");
    const std::string frames = extract("frames");
    const std::string source = extract("source");
    out.running = running == "true";
    out.fps = fps.empty() ? 0.0 : std::stod(fps);
    out.frames = frames.empty() ? 0 : std::stoull(frames);
    out.source = source;
    out.error = extract("error");
}
} // namespace

struct CameraEngineController::Impl {
    PROCESS_INFORMATION proc{};
    bool started = false;
    CameraEngineParams lastParams;
};

CameraEngineController::CameraEngineController() : m_impl(std::make_unique<Impl>()) {}

CameraEngineController::~CameraEngineController() {
    stop();
}

std::string CameraEngineController::paramsFilePath() {
    return tempDir() + "htb_cam_params.json";
}

std::string CameraEngineController::statusFilePath() {
    return tempDir() + "htb_cam_status.json";
}

void CameraEngineController::writeParams(const CameraEngineParams& p, bool running) {
    std::ofstream out(paramsFilePath());
    if (!out) return;
    out << "{\"camera_index\":" << p.cameraIndex << ",\"zoom\":" << p.zoom << ",\"pan_x\":" << p.panX
        << ",\"pan_y\":" << p.panY << ",\"flip_h\":" << (p.flipHorizontal ? "true" : "false")
        << ",\"flip_v\":" << (p.flipVertical ? "true" : "false") << ",\"brightness\":" << p.brightness
        << ",\"contrast\":" << p.contrast << ",\"saturation\":" << p.saturation
        << ",\"running\":" << (running ? "true" : "false") << "}";
}

void CameraEngineController::start(const CameraEngineParams& params) {
    if (m_impl->started) return;
    m_impl->lastParams = params;
    writeParams(params, true);
    std::filesystem::remove(statusFilePath());

    const std::string python = findPython();
    const std::string script = moduleDirectory() + "\\camera_engine.py";
    const std::string cmdLine = "\"" + python + "\" \"" + script + "\" --parent-pid " +
                                std::to_string(GetCurrentProcessId());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    const BOOL ok = CreateProcessW(nullptr, const_cast<wchar_t*>(toWide(cmdLine).c_str()), nullptr, nullptr, FALSE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_impl->proc);
    if (!ok) {
        HTB_ERROR("[camera] failed to start python engine: {}", cmdLine);
        return;
    }
    m_impl->started = true;
    HTB_INFO("[camera] python engine started: {}", cmdLine);
}

void CameraEngineController::updateParams(const CameraEngineParams& params) {
    m_impl->lastParams = params;
    writeParams(params, true);
}

void CameraEngineController::stop() {
    if (!m_impl->started) return;
    writeParams(m_impl->lastParams, false);
    if (m_impl->proc.hProcess) {
        WaitForSingleObject(m_impl->proc.hProcess, 3000);
        DWORD code = 0;
        GetExitCodeProcess(m_impl->proc.hProcess, &code);
        if (code == STILL_ACTIVE) {
            TerminateProcess(m_impl->proc.hProcess, 1);
        }
        CloseHandle(m_impl->proc.hProcess);
        CloseHandle(m_impl->proc.hThread);
    }
    m_impl->started = false;
    HTB_INFO("[camera] python engine stopped");
}

void CameraEngineController::poll() {
    auto status = std::make_shared<CameraEngineStatus>();
    status->running = m_impl->started;
    std::ifstream in(statusFilePath());
    if (in) {
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        parseStatusJson(text, *status);
    }
    if (m_impl->started) {
        DWORD code = 0;
        if (!GetExitCodeProcess(m_impl->proc.hProcess, &code) || code != STILL_ACTIVE) {
            m_impl->started = false;
            if (code != 0 && code != STILL_ACTIVE) {
                char buf[64];
                snprintf(buf, sizeof(buf), "引擎已退出 (退出码 %lu)", static_cast<unsigned long>(code));
                status->error = buf;
            } else if (!status->running) {
                status->error = "引擎已退出";
            }
            status->running = false;
            CloseHandle(m_impl->proc.hProcess);
            CloseHandle(m_impl->proc.hThread);
            HTB_WARN("[camera] python engine exited with code {}", static_cast<unsigned long>(code));
        }
    }
    m_status.store(status);
}

} // namespace htb
