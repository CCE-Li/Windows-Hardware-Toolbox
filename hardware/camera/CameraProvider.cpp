#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/camera/CameraProvider.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace htb {

struct CameraProvider::Impl {
    bool mfInitialized = false;
};

CameraProvider::CameraProvider() : m_impl(std::make_unique<Impl>()) {}

CameraProvider::~CameraProvider() = default;

void CameraProvider::refresh() {
    auto cameras = std::make_shared<std::vector<CameraInfo>>();

    if (!m_impl->mfInitialized) {
        if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) {
            HTB_WARN("[camera] MFStartup failed; camera enumeration unavailable");
            m_snapshot.store(std::move(cameras));
            return;
        }
        m_impl->mfInitialized = true;
    }

    ComPtr<IMFAttributes> attributes;
    if (FAILED(MFCreateAttributes(&attributes, 2))) {
        HTB_WARN("[camera] MFCreateAttributes failed");
        m_snapshot.store(std::move(cameras));
        return;
    }
    attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    HRESULT hr = MFEnumDeviceSources(attributes.Get(), &devices, &count);
    if (SUCCEEDED(hr) && count > 0) {
        for (UINT32 i = 0; i < count; ++i) {
            CameraInfo camera;
            camera.source = "Media Foundation";

            LPWSTR name = nullptr;
            UINT32 len = 0;
            if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len)) && name) {
                camera.name = toUtf8(name);
                CoTaskMemFree(name);
            }
            LPWSTR link = nullptr;
            if (SUCCEEDED(devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                                                         &link, &len)) &&
                link) {
                camera.symbolicLink = toUtf8(link);
                CoTaskMemFree(link);
            }
            if (camera.name.empty()) camera.name = camera.symbolicLink;
            cameras->push_back(std::move(camera));
            devices[i]->Release();
        }
        CoTaskMemFree(devices);
    }

    HTB_INFO("[camera] {} camera(s) enumerated by Media Foundation", cameras->size());
    m_snapshot.store(std::move(cameras));
}

} // namespace htb
