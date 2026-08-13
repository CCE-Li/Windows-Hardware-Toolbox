#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/VirtualCameraController.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfvirtualcamera.h>
#include <windows.h>
#include <wrl/client.h>

#include <functional>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace htb {

namespace {
constexpr GUID KSCATEGORY_VIDEO_CAMERA = {0xE5323777, 0xF976, 0x4f5b,
                                          {0x9B, 0x55, 0xB9, 0x46, 0x99, 0xC4, 0x6E, 0x44}};
constexpr GUID KSCATEGORY_VIDEO = {0x6994AD05, 0x93EF, 0x11D0,
                                   {0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
constexpr GUID KSCATEGORY_CAPTURE = {0x65E8773D, 0x8F56, 0x11D0,
                                     {0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};

std::string hrText(HRESULT hr) {
    char buf[64];
    snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(hr));
    return buf;
}

std::string moduleDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    return toUtf8(path);
}
} // namespace

struct VirtualCameraController::Impl {
    HMODULE dll = nullptr;
    ComPtr<IMFVirtualCamera> camera;
    std::string friendlyName;
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

VirtualCameraController::VirtualCameraController() : m_impl(std::make_unique<Impl>()) {}

VirtualCameraController::~VirtualCameraController() {
    if (m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
    if (m_impl->dll) FreeLibrary(m_impl->dll);
}

std::string VirtualCameraController::clsidString() {
    return "{7D8E9F3A-2C4B-4E5F-9A1C-3D2B6E8F4A51}";
}

void VirtualCameraController::run(const std::string& operation, const std::string& friendlyName,
                                  const std::function<long(void*)>& task) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, friendlyName, task] {
        auto status = std::make_shared<VirtualCameraStatus>();
        status->operation = operation;
        status->friendlyName = friendlyName;
        status->inProgress = true;
        m_status.store(status);

        if (!m_impl->dll) {
            const std::string dllPath = moduleDirectory() + "\\virtualcamera.dll";
            m_impl->dll = LoadLibraryW(toWide(dllPath).c_str());
            if (!m_impl->dll) {
                status->message = "无法加载 virtualcamera.dll (" + dllPath + ")";
                status->inProgress = false;
                m_status.store(status);
                m_impl->opRunning.store(false);
                return;
            }
        }
        auto registerServer = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(m_impl->dll, "DllRegisterServer"));
        if (registerServer) {
            const HRESULT regHr = registerServer();
            if (FAILED(regHr)) {
                HTB_WARN("[camera] DllRegisterServer failed: {:#x}", static_cast<unsigned>(regHr));
            }
        }

        ComPtr<IMFVirtualCamera> camera;
        const long hr = task(static_cast<void*>(camera.GetAddressOf()));
        status->success = SUCCEEDED(hr);
        status->message = SUCCEEDED(hr) ? "成功" : ("失败 (" + hrText(hr) + ")");
        if (SUCCEEDED(hr)) m_impl->camera = camera;
        status->inProgress = false;
        m_status.store(status);
        if (SUCCEEDED(hr)) {
            HTB_INFO("[camera] {} succeeded: {}", operation, friendlyName);
        } else {
            HTB_ERROR("[camera] {} failed: {:#x}", operation, static_cast<unsigned>(hr));
        }
        m_impl->opRunning.store(false);
    });
}

void VirtualCameraController::create(const std::string& friendlyName) {
    m_impl->friendlyName = friendlyName;
    run("创建虚拟摄像头", friendlyName, [friendlyName](void* outRaw) -> long {
        auto** out = static_cast<IMFVirtualCamera**>(outRaw);
        const std::wstring name = toWide(friendlyName);
        const std::wstring clsid = toWide(clsidString());
        const GUID categories[] = {KSCATEGORY_VIDEO_CAMERA, KSCATEGORY_VIDEO, KSCATEGORY_CAPTURE};
        const HRESULT createHr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource,
                                                       MFVirtualCameraLifetime_System,
                                                       MFVirtualCameraAccess_CurrentUser, name.c_str(), clsid.c_str(),
                                                       categories, 3, out);
        if (FAILED(createHr)) {
            HTB_ERROR("[camera] MFCreateVirtualCamera failed: {:#x}", static_cast<unsigned>(createHr));
            return createHr;
        }
        const HRESULT startHr = (*out)->Start(nullptr);
        if (FAILED(startHr)) {
            HTB_ERROR("[camera] IMFVirtualCamera::Start failed: {:#x}", static_cast<unsigned>(startHr));
        }
        return startHr;
    });
}

void VirtualCameraController::remove() {
    const std::string name = m_impl->friendlyName.empty() ? "Hardware Toolbox 虚拟摄像头" : m_impl->friendlyName;
    run("移除虚拟摄像头", name, [this, name](void* outRaw) -> long {
        auto** out = static_cast<IMFVirtualCamera**>(outRaw);
        const std::wstring wname = toWide(name);
        const std::wstring clsid = toWide(clsidString());
        const HRESULT hr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource,
                                                 MFVirtualCameraLifetime_System, MFVirtualCameraAccess_CurrentUser,
                                                 wname.c_str(), clsid.c_str(), nullptr, 0, out);
        if (FAILED(hr)) return hr;
        m_impl->camera.Reset();
        return (*out)->Remove();
    });
}

} // namespace htb
