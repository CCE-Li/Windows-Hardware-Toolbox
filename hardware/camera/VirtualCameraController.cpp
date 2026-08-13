#include "core/logging/Logger.h"
#include "core/runtime/Privileges.h"
#include "core/util/Utf.h"
#include "hardware/camera/VirtualCameraController.h"
#include "hardware/camera/VirtualCameraRegistrar.h"

#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace htb {

namespace {
std::string modulePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return toUtf8(path);
}

std::string tempDir() {
    wchar_t buf[MAX_PATH]{};
    GetTempPathW(MAX_PATH, buf);
    return toUtf8(buf);
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

std::string VirtualCameraController::resultFilePath() {
    return tempDir() + "htb_vcam_result.txt";
}

void VirtualCameraController::checkResultFile(const std::string& operation) {
    std::ifstream in(resultFilePath());
    if (!in) return;
    std::string line;
    std::getline(in, line);
    std::string message;
    std::getline(in, message);
    if (line.empty()) return;

    auto status = std::make_shared<VirtualCameraStatus>();
    status->operation = operation;
    status->friendlyName = m_impl->friendlyName;
    status->success = line == "success";
    status->message = message.empty() ? (status->success ? "成功" : "失败") : message;
    m_status.store(status);
    std::filesystem::remove(resultFilePath());
}

void VirtualCameraController::pollPendingResult(const std::string& operation) {
    checkResultFile(operation);
}

void VirtualCameraController::run(const std::string& operation, const std::string& friendlyName) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, friendlyName] {
        auto status = std::make_shared<VirtualCameraStatus>();
        status->operation = operation;
        status->friendlyName = friendlyName;
        status->inProgress = true;
        m_status.store(status);

        if (!htb::isElevated()) {
            const std::wstring exe = toWide(modulePath());
            std::wstring args = operation == "创建虚拟摄像头" ? L"--auto-register-vcamera"
                                                             : L"--auto-unregister-vcamera";
            args += L" \"" + toWide(friendlyName) + L"\" --elevated --page=camera";

            SHELLEXECUTEINFOW sei{};
            sei.cbSize = sizeof(sei);
            sei.fMask = SEE_MASK_NOCLOSEPROCESS;
            sei.lpVerb = L"runas";
            sei.lpFile = exe.c_str();
            sei.lpParameters = args.c_str();
            sei.nShow = SW_SHOWNORMAL;
            const BOOL launched = ShellExecuteExW(&sei);
            if (launched && sei.hProcess) CloseHandle(sei.hProcess);
            status->message = launched ? "UAC 确认后将在新窗口自动完成注册" : "无法启动提升进程";
            status->inProgress = false;
            m_status.store(status);
            m_impl->opRunning.store(false);
            return;
        }

        const long hr = operation == "创建虚拟摄像头"
                            ? VirtualCameraRegistrar::registerVirtualCamera(friendlyName)
                            : VirtualCameraRegistrar::unregisterVirtualCamera(friendlyName);
        status->success = SUCCEEDED(hr);
        if (status->success) {
            status->message = "成功";
        } else {
            char buf[16];
            snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
            status->message = std::string("失败 (") + buf + ")";
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
