#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/device/DeviceProvider.h"

#include <windows.h>
#include <cfg.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <setupapi.h>

#include <cwchar>

#ifndef DN_DISABLED
#define DN_DISABLED 0x00000010
#endif

namespace htb {

namespace {
constexpr std::chrono::steady_clock::duration kMaxAge = std::chrono::seconds(60);

std::vector<std::string> readStringList(HDEVINFO devs, PSP_DEVINFO_DATA spdi, const DEVPROPKEY& key) {
    std::vector<std::string> out;
    DWORD size = 0;
    SetupDiGetDevicePropertyW(devs, spdi, &key, nullptr, nullptr, 0, &size, 0);
    if (size == 0) return out;
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1);
    DEVPROPTYPE type = 0;
    if (!SetupDiGetDevicePropertyW(devs, spdi, &key, &type, reinterpret_cast<PBYTE>(buf.data()), size, nullptr, 0))
        return out;
    const wchar_t* p = buf.data();
    while (*p) {
        out.emplace_back(toUtf8(p));
        p += wcslen(p) + 1;
    }
    return out;
}

bool readString(HDEVINFO devs, PSP_DEVINFO_DATA spdi, const DEVPROPKEY& key, std::string& out) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    DEVPROPTYPE type = 0;
    if (SetupDiGetDevicePropertyW(devs, spdi, &key, &type, reinterpret_cast<PBYTE>(buf), size, nullptr, 0) &&
        type == DEVPROP_TYPE_STRING) {
        out = toUtf8(buf);
        return true;
    }
    return false;
}

bool readUlong(HDEVINFO devs, PSP_DEVINFO_DATA spdi, const DEVPROPKEY& key, uint32_t& out) {
    ULONG value = 0;
    DEVPROPTYPE type = 0;
    if (!SetupDiGetDevicePropertyW(devs, spdi, &key, &type, reinterpret_cast<PBYTE>(&value), sizeof(value), nullptr, 0) ||
        type != DEVPROP_TYPE_UINT32)
        return false;
    out = static_cast<uint32_t>(value);
    return true;
}

void readDriverInfo(DeviceInfo& info) {
    if (info.driverKey.empty()) return;
    const std::wstring path = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + toWide(info.driverKey);
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    auto read = [&](const wchar_t* name, std::string& out) {
        wchar_t buf[512];
        DWORD size = sizeof(buf);
        if (RegGetValueW(hKey, nullptr, name, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
            out = toUtf8(buf);
    };
    read(L"DriverVersion", info.driverVersion);
    read(L"DriverDate", info.driverDate);
    read(L"ProviderName", info.driverProvider);
    RegCloseKey(hKey);
}
} // namespace

struct DeviceProvider::Impl {};

DeviceProvider::DeviceProvider() = default;

DeviceProvider::~DeviceProvider() = default;

void DeviceProvider::requestRefresh() {
    m_force.store(true);
}

void DeviceProvider::refresh() {
    const auto now = std::chrono::steady_clock::now();
    const auto last = m_lastEnum.load();
    if (!m_force.exchange(false) && last != std::chrono::steady_clock::time_point{} && (now - last) < kMaxAge) {
        return;
    }

    auto devices = std::make_shared<std::vector<DeviceInfo>>();
    HDEVINFO devs = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) {
        HTB_WARN("[device] SetupDiGetClassDevsW failed: {}", GetLastError());
        m_snapshot.store(std::move(devices));
        m_lastEnum.store(now);
        return;
    }

    SP_DEVINFO_DATA spdi{};
    spdi.cbSize = sizeof(spdi);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &spdi); ++i) {
        DeviceInfo info;
        wchar_t instance[MAX_DEVICE_ID_LEN];
        if (SetupDiGetDeviceInstanceIdW(devs, &spdi, instance, MAX_DEVICE_ID_LEN, nullptr))
            info.instanceId = toUtf8(instance);
        readString(devs, &spdi, DEVPKEY_Device_Parent, info.parentId);
        if (!readString(devs, &spdi, DEVPKEY_Device_FriendlyName, info.name))
            readString(devs, &spdi, DEVPKEY_Device_DeviceDesc, info.name);
        readString(devs, &spdi, DEVPKEY_Device_DeviceDesc, info.description);
        readString(devs, &spdi, DEVPKEY_Device_Manufacturer, info.manufacturer);
        readString(devs, &spdi, DEVPKEY_Device_Class, info.className);
        readString(devs, &spdi, DEVPKEY_Device_ClassGuid, info.classGuid);
        readString(devs, &spdi, DEVPKEY_Device_EnumeratorName, info.enumerator);
        readString(devs, &spdi, DEVPKEY_Device_Service, info.service);
        readString(devs, &spdi, DEVPKEY_Device_Driver, info.driverKey);
        readString(devs, &spdi, DEVPKEY_Device_LocationPaths, info.locationPaths);
        uint32_t bus = 0;
        if (readUlong(devs, &spdi, DEVPKEY_Device_BusNumber, bus)) info.busNumber = bus;
        readUlong(devs, &spdi, DEVPKEY_Device_ProblemCode, info.problem);
        info.hardwareIds = readStringList(devs, &spdi, DEVPKEY_Device_HardwareIds);
        info.compatibleIds = readStringList(devs, &spdi, DEVPKEY_Device_CompatibleIds);
        readDriverInfo(info);

        DEVINST devInst = 0;
        if (CM_Locate_DevNodeW(&devInst, instance, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS) {
            ULONG status = 0;
            ULONG problem = 0;
            if (CM_Get_DevNode_Status(&status, &problem, devInst, 0) == CR_SUCCESS) {
                info.started = (status & DN_STARTED) != 0;
                info.disabled = (status & DN_DISABLED) != 0;
            }
        }
        devices->push_back(std::move(info));
    }
    SetupDiDestroyDeviceInfoList(devs);

    HTB_INFO("[device] enumerated {} present devices", devices->size());
    m_snapshot.store(std::move(devices));
    m_lastEnum.store(now);
}

} // namespace htb
