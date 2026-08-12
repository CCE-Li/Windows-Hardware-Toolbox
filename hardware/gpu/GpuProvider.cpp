#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/gpu/GpuProvider.h"

#include <cwctype>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace htb {

namespace {
std::wstring pciIdString(uint32_t vid, uint32_t did) {
    wchar_t buf[32];
    swprintf_s(buf, 32, L"VEN_%04X&DEV_%04X", vid, did);
    return buf;
}

void queryDriverInfo(GpuInfo& gpu, uint32_t vid, uint32_t did) {
    const wchar_t* classKey = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
    HKEY hClass = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, classKey, 0, KEY_READ, &hClass) != ERROR_SUCCESS) return;

    const std::wstring want = pciIdString(vid, did);
    for (DWORD i = 0;; ++i) {
        wchar_t name[64]{};
        if (RegEnumKeyW(hClass, i, name, 64) != ERROR_SUCCESS) break;
        const std::wstring sub = std::wstring(classKey) + L"\\" + name;

        HKEY hSub = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub.c_str(), 0, KEY_READ, &hSub) != ERROR_SUCCESS) continue;

        auto readString = [&](const wchar_t* valueName, wchar_t* out, DWORD outBytes) {
            DWORD size = outBytes;
            return RegGetValueW(hSub, nullptr, valueName, RRF_RT_REG_SZ, nullptr, out, &size) == ERROR_SUCCESS;
        };

        bool matched = false;
        wchar_t match[512]{};
        if (readString(L"MatchingDeviceId", match, sizeof(match))) {
            std::wstring ms(match);
            for (auto& c : ms) c = static_cast<wchar_t>(towupper(c));
            matched = ms.find(want) != std::wstring::npos;
        }
        if (matched) {
            wchar_t version[128]{};
            wchar_t date[128]{};
            if (readString(L"DriverVersion", version, sizeof(version))) gpu.driverVersion = toUtf8(version);
            if (readString(L"DriverDate", date, sizeof(date))) gpu.driverDate = toUtf8(date);
            gpu.driverSource = "Registry (Display class key)";
            gpu.driverAvailability = (gpu.driverVersion.empty() && gpu.driverDate.empty())
                                         ? Availability::Unavailable
                                         : Availability::Available;
            RegCloseKey(hSub);
            break;
        }
        RegCloseKey(hSub);
    }
    RegCloseKey(hClass);
}
} // namespace

GpuProvider::GpuProvider() = default;

void GpuProvider::refresh() {
    auto adapters = std::make_shared<std::vector<GpuInfo>>();

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        HTB_WARN("[gpu] CreateDXGIFactory1 failed; GPU info unavailable");
        m_snapshot.store(std::move(adapters));
        return;
    }

    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapters1(i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr)) {
            HTB_WARN("[gpu] EnumAdapters1({}) failed: {:#x}", i, static_cast<unsigned>(hr));
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) continue;

        GpuInfo gpu;
        gpu.name = toUtf8(desc.Description);
        gpu.vendor = vendorFromPciId(desc.VendorId);
        gpu.dedicatedVramBytes = desc.DedicatedVideoMemory;
        gpu.dedicatedSystemMemoryBytes = desc.DedicatedSystemMemory;
        gpu.sharedSystemMemoryBytes = desc.SharedSystemMemory;
        queryDriverInfo(gpu, desc.VendorId, desc.DeviceId);

        ComPtr<IDXGIAdapter3> adapter3;
        if (SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3)))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
            if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)) &&
                memInfo.Budget > 0) {
                gpu.usageBytes = memInfo.CurrentUsage;
                gpu.usagePercent = static_cast<float>(
                    static_cast<double>(memInfo.CurrentUsage) * 100.0 / static_cast<double>(memInfo.Budget));
                if (gpu.usagePercent > 100.0f) gpu.usagePercent = 100.0f;
                gpu.usageAvailability = Availability::Available;
                gpu.usageSource = "DXGI";
            }
        }

        for (UINT o = 0;; ++o) {
            ComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(o, &output) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_OUTPUT_DESC od{};
            if (SUCCEEDED(output->GetDesc(&od))) gpu.outputs.push_back(toUtf8(od.DeviceName));
        }

        HTB_INFO("[gpu] {} ({}) VRAM {} bytes, driver {}", gpu.name, vendorName(gpu.vendor),
                 gpu.dedicatedVramBytes, gpu.driverVersion.empty() ? "N/A" : gpu.driverVersion);
        adapters->push_back(std::move(gpu));
    }
    m_snapshot.store(std::move(adapters));
}

} // namespace htb
