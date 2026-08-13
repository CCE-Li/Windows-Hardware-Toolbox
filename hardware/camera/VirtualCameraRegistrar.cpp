#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/VirtualCameraController.h"
#include "hardware/camera/VirtualCameraRegistrar.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfvirtualcamera.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace htb {

namespace {
constexpr GUID KSCATEGORY_VIDEO_CAMERA = {0xE5323777, 0xF976, 0x4f5b,
                                          {0x9B, 0x55, 0xB9, 0x46, 0x99, 0xC4, 0x6E, 0x44}};
constexpr GUID KSCATEGORY_VIDEO = {0x6994AD05, 0x93EF, 0x11D0,
                                   {0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};
constexpr GUID KSCATEGORY_CAPTURE = {0x65E8773D, 0x8F56, 0x11D0,
                                     {0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96}};

HMODULE ensureProviderDll() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *slash = L'\0';
    std::wstring dllPath = std::wstring(path) + L"\\virtualcamera.dll";
    HMODULE dll = LoadLibraryW(dllPath.c_str());
    if (!dll) {
        HTB_ERROR("[vcam] failed to load virtualcamera.dll");
        return nullptr;
    }
    auto registerServer = reinterpret_cast<HRESULT(WINAPI*)()>(GetProcAddress(dll, "DllRegisterServer"));
    if (registerServer) {
        const HRESULT regHr = registerServer();
        if (FAILED(regHr)) {
            HTB_WARN("[vcam] DllRegisterServer failed: {:#x}", static_cast<unsigned>(regHr));
        }
    }
    return dll;
}
} // namespace

long VirtualCameraRegistrar::registerVirtualCamera(const std::string& friendlyName) {
    HMODULE dll = ensureProviderDll();
    if (!dll) return E_FAIL;
    FreeLibrary(dll);

    const std::wstring name = toWide(friendlyName);
    const std::wstring clsid = toWide(VirtualCameraController::clsidString());

    ComPtr<IMFVirtualCamera> camera;
    long hr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource, MFVirtualCameraLifetime_System,
                                    MFVirtualCameraAccess_AllUsers, name.c_str(), clsid.c_str(), nullptr, 0,
                                    &camera);
    if (FAILED(hr)) {
        HTB_ERROR("[vcam] MFCreateVirtualCamera failed: {:#x}", static_cast<unsigned>(hr));
        return hr;
    }
    hr = camera->Start(nullptr);
    if (FAILED(hr)) {
        HTB_ERROR("[vcam] IMFVirtualCamera::Start failed: {:#x}", static_cast<unsigned>(hr));
    } else {
        HTB_INFO("[vcam] virtual camera registered: {}", friendlyName);
    }
    return hr;
}

long VirtualCameraRegistrar::unregisterVirtualCamera(const std::string& friendlyName) {
    const std::wstring name = toWide(friendlyName);
    const std::wstring clsid = toWide(VirtualCameraController::clsidString());

    ComPtr<IMFVirtualCamera> camera;
    long hr = MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource, MFVirtualCameraLifetime_System,
                                    MFVirtualCameraAccess_AllUsers, name.c_str(), clsid.c_str(), nullptr, 0,
                                    &camera);
    if (FAILED(hr)) {
        HTB_ERROR("[vcam] MFCreateVirtualCamera (remove) failed: {:#x}", static_cast<unsigned>(hr));
        return hr;
    }
    hr = camera->Remove();
    if (FAILED(hr)) {
        HTB_ERROR("[vcam] Remove failed: {:#x}", static_cast<unsigned>(hr));
    } else {
        HTB_INFO("[vcam] virtual camera removed: {}", friendlyName);
    }
    return hr;
}

} // namespace htb
