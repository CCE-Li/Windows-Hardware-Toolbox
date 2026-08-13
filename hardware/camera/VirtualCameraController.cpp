#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/VirtualCameraController.h"

#include <windows.h>
#include <shellapi.h>

#include <functional>
#include <string>
#include <thread>

namespace htb {

namespace {
std::string hrText(long hr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
    return buf;
}

std::string modulePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return toUtf8(path);
}
} // namespace

struct VirtualCameraController::Impl {
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
    std::string friendlyName;
};

VirtualCameraController::VirtualCameraController() : m_impl(std::make_unique<Impl>()) {}

VirtualCameraController::~VirtualCameraController() {
    if (m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

std::string VirtualCameraController::clsidString() {
    return "{7D8E9F3A-2C4B-4E5F-9A1C-3D2B6E8F4A51}";
}

void VirtualCameraController::run(const std::string& operation, const std::string& friendlyName) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, friendlyName] {
        auto status = std::make_shared<VirtualCameraStatus>();
        status->operation = operation;
        status->friendlyName = friendlyName;
        status->inProgress = true;
        m_status.store(status);

        const std::wstring exe = toWide(modulePath());
        std::wstring args = operation == "创建虚拟摄像头" ? L"--register-vcamera" : L"--unregister-vcamera";
        args += L" \"" + toWide(friendlyName) + L"\" --console";

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
        sei.lpVerb = L"open";
        sei.lpFile = exe.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_HIDE;

        const BOOL launched = ShellExecuteExW(&sei);
        if (!launched || !sei.hProcess) {
            status->message = "无法启动注册进程";
            status->inProgress = false;
            m_status.store(status);
            m_impl->opRunning.store(false);
            return;
        }

        const DWORD wait = WaitForSingleObject(sei.hProcess, 60000);
        DWORD exitCode = 1;
        GetExitCodeProcess(sei.hProcess, &exitCode);
        CloseHandle(sei.hProcess);

        if (wait == WAIT_TIMEOUT) {
            status->message = "注册进程超时";
        } else if (exitCode == 0) {
            status->success = true;
            status->message = "成功";
        } else {
            status->message = "失败 (子进程退出码 " + std::to_string(exitCode) + "，详情见日志)";
        }
        status->inProgress = false;
        m_status.store(status);
        if (status->success) {
            HTB_INFO("[camera] {} succeeded: {}", operation, friendlyName);
        } else {
            HTB_ERROR("[camera] {} failed: {}", operation, status->message);
        }
        m_impl->opRunning.store(false);
    });
}

void VirtualCameraController::create(const std::string& friendlyName) {
    m_impl->friendlyName = friendlyName;
    run("创建虚拟摄像头", friendlyName);
}

void VirtualCameraController::remove() {
    const std::string name = m_impl->friendlyName.empty() ? "Hardware Toolbox 虚拟摄像头" : m_impl->friendlyName;
    run("移除虚拟摄像头", name);
}

} // namespace htb
