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
#include <string_view>

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

struct DeviceProvider::Impl {
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

DeviceProvider::DeviceProvider() = default;

DeviceProvider::~DeviceProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

namespace {
std::string setupErrorMessage() {
    wchar_t buf[256]{};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                   GetLastError(), 0, buf, 256, nullptr);
    return n > 0 ? toUtf8(std::wstring_view(buf, n)) : "未知错误";
}

std::pair<bool, std::string> doSetEnabled(const std::string& instanceId, bool enable) {
    HDEVINFO devs = SetupDiGetClassDevsW(nullptr, toWide(instanceId).c_str(), nullptr,
                                         DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) return {false, "无法打开设备信息集"};
    SP_DEVINFO_DATA spdi{};
    spdi.cbSize = sizeof(spdi);
    if (!SetupDiEnumDeviceInfo(devs, 0, &spdi)) {
        SetupDiDestroyDeviceInfoList(devs);
        return {false, "设备未找到"};
    }
    SP_PROPCHANGE_PARAMS params{};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    params.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
    params.Scope = DICS_FLAG_GLOBAL;
    params.HwProfile = 0;
    bool ok = SetupDiSetClassInstallParamsW(devs, &spdi, &params.ClassInstallHeader, sizeof(params));
    if (ok) ok = SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devs, &spdi);
    const std::string err = ok ? std::string() : setupErrorMessage();
    SetupDiDestroyDeviceInfoList(devs);
    return ok ? std::pair<bool, std::string>(true, {}) : std::pair<bool, std::string>(false, err);
}

std::pair<bool, std::string> doRemove(const std::string& instanceId) {
    HDEVINFO devs = SetupDiGetClassDevsW(nullptr, toWide(instanceId).c_str(), nullptr,
                                         DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) return {false, "无法打开设备信息集"};
    SP_DEVINFO_DATA spdi{};
    spdi.cbSize = sizeof(spdi);
    if (!SetupDiEnumDeviceInfo(devs, 0, &spdi)) {
        SetupDiDestroyDeviceInfoList(devs);
        return {false, "设备未找到"};
    }
    SP_REMOVEDEVICE_PARAMS params{};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_REMOVE;
    params.Scope = DI_REMOVEDEVICE_GLOBAL;
    params.HwProfile = 0;
    bool ok = SetupDiSetClassInstallParamsW(devs, &spdi, &params.ClassInstallHeader, sizeof(params));
    if (ok) ok = SetupDiCallClassInstaller(DIF_REMOVE, devs, &spdi);
    const std::string err = ok ? std::string() : setupErrorMessage();
    SetupDiDestroyDeviceInfoList(devs);
    return ok ? std::pair<bool, std::string>(true, {}) : std::pair<bool, std::string>(false, err);
}

std::pair<bool, std::string> doRescan() {
    DEVINST root = 0;
    if (CM_Locate_DevNodeW(&root, nullptr, 0) != CR_SUCCESS) return {false, "无法定位根设备节点"};
    const CONFIGRET cr = CM_Reenumerate_DevNode(root, CM_REENUMERATE_SYNCHRONOUS);
    return cr == CR_SUCCESS ? std::pair<bool, std::string>(true, {})
                            : std::pair<bool, std::string>(false, "重新扫描失败");
}
} // namespace

void DeviceProvider::runOperation(const std::string& operation, const std::string& instanceId,
                                  const std::function<std::pair<bool, std::string>()>& task) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, instanceId, task] {
        auto result = std::make_shared<DeviceOperationResult>();
        result->operation = operation;
        result->instanceId = instanceId;
        auto outcome = task();
        result->success = outcome.first;
        result->message = outcome.second;
        m_lastOperation.store(result);
        if (result->success) {
            HTB_INFO("[device] {} succeeded: {} {}", operation, instanceId,
                     result->message.empty() ? "" : result->message);
            m_force.store(true);
        } else {
            HTB_ERROR("[device] {} failed for {}: {}", operation, instanceId, result->message);
        }
        m_impl->opRunning.store(false);
    });
}

void DeviceProvider::setDeviceEnabledAsync(const std::string& instanceId, bool enable) {
    runOperation(enable ? "启用设备" : "禁用设备", instanceId, [instanceId, enable] {
        return doSetEnabled(instanceId, enable);
    });
}

void DeviceProvider::removeDeviceAsync(const std::string& instanceId) {
    runOperation("卸载设备", instanceId, [instanceId] { return doRemove(instanceId); });
}

void DeviceProvider::rescanDevicesAsync() {
    runOperation("扫描硬件更改", "", [] { return doRescan(); });
}

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
