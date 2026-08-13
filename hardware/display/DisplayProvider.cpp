#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/display/DisplayProvider.h"

#include <windows.h>

#include <cmath>
#include <cwchar>
#include <cstring>
#include <string>
#include <vector>

namespace htb {

namespace {
std::string orientationName(DWORD orientation) {
    switch (orientation) {
        case DMDO_DEFAULT: return "横向";
        case DMDO_90: return "纵向 (90)";
        case DMDO_180: return "横向翻转 (180)";
        case DMDO_270: return "纵向翻转 (270)";
        default: return "未知";
    }
}

std::wstring monitorIdFromDeviceId(const std::wstring& deviceId) {
    const size_t first = deviceId.find(L'\\');
    if (first == std::wstring::npos) return {};
    const size_t second = deviceId.find(L'\\', first + 1);
    if (second == std::wstring::npos) return {};
    return deviceId.substr(first + 1, second - first - 1);
}

struct EdidData {
    std::string manufacturer;
    std::string serial;
    uint32_t week = 0;
    uint32_t year = 0;
    double sizeInches = 0.0;
};

bool parseEdid(const std::vector<uint8_t>& edid, EdidData& out) {
    if (edid.size() < 128) return false;
    static const uint8_t header[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    if (memcmp(edid.data(), header, 8) != 0) return false;

    const char m1 = static_cast<char>('A' + ((edid[8] >> 2) & 0x1F) - 1);
    const char m2 = static_cast<char>('A' + (((edid[8] & 0x03) << 3) | ((edid[9] >> 5) & 0x07)) - 1);
    const char m3 = static_cast<char>('A' + (edid[9] & 0x1F) - 1);
    if (m1 >= 'A' && m1 <= 'Z' && m2 >= 'A' && m2 <= 'Z' && m3 >= 'A' && m3 <= 'Z') {
        out.manufacturer = std::string() + m1 + m2 + m3;
    }
    out.week = edid[10];
    out.year = static_cast<uint32_t>(edid[11]) + 1990;
    if (edid[21] > 0 && edid[22] > 0) {
        const double h = edid[21] / 2.54;
        const double w = edid[22] / 2.54;
        if (h > 0.0 && w > 0.0) out.sizeInches = std::sqrt(h * h + w * w);
    }
    for (int block = 0; block < 4; ++block) {
        const uint8_t* b = edid.data() + 54 + block * 18;
        if (b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0xFF) {
            std::string serial(reinterpret_cast<const char*>(b + 5), 13);
            while (!serial.empty() && (serial.back() == ' ' || serial.back() == '\n' || serial.back() == 0)) {
                serial.pop_back();
            }
            out.serial = serial;
        }
    }
    return true;
}

EdidData readEdidForMonitor(const std::wstring& monitorId) {
    EdidData out;
    HKEY hEnum = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY", 0, KEY_READ, &hEnum) !=
        ERROR_SUCCESS)
        return out;

    for (DWORD i = 0;; ++i) {
        wchar_t keyName[128]{};
        if (RegEnumKeyW(hEnum, i, keyName, 128) != ERROR_SUCCESS) break;
        if (monitorId != L"" && _wcsicmp(keyName, monitorId.c_str()) != 0) continue;

        std::wstring sub = L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY\\";
        sub += keyName;
        HKEY hInstance = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub.c_str(), 0, KEY_READ, &hInstance) != ERROR_SUCCESS) continue;

        for (DWORD j = 0;; ++j) {
            wchar_t instName[128]{};
            if (RegEnumKeyW(hInstance, j, instName, 128) != ERROR_SUCCESS) break;
            std::wstring instPath = sub + L"\\" + instName + L"\\Device Parameters";
            HKEY hParams = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, instPath.c_str(), 0, KEY_READ, &hParams) != ERROR_SUCCESS) continue;
            std::vector<uint8_t> edid(512);
            DWORD size = static_cast<DWORD>(edid.size());
            if (RegGetValueW(hParams, nullptr, L"EDID", RRF_RT_REG_BINARY, nullptr, edid.data(), &size) ==
                    ERROR_SUCCESS &&
                size > 0) {
                edid.resize(size);
                if (parseEdid(edid, out)) {
                    RegCloseKey(hParams);
                    RegCloseKey(hInstance);
                    RegCloseKey(hEnum);
                    return out;
                }
            }
            RegCloseKey(hParams);
        }
        RegCloseKey(hInstance);
        if (!out.manufacturer.empty()) break;
    }
    RegCloseKey(hEnum);
    return out;
}
} // namespace

struct DisplayProvider::Impl {
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

DisplayProvider::DisplayProvider() = default;

DisplayProvider::~DisplayProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

void DisplayProvider::loadModesAsync(const std::string& deviceName) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, deviceName] {
        auto result = std::make_shared<DisplayModeResult>();
        result->deviceName = deviceName;
        const std::wstring dev = toWide(deviceName);
        for (DWORD i = 0;; ++i) {
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (!EnumDisplaySettingsW(dev.c_str(), i, &dm)) break;
            if (dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0) continue;
            DisplayMode mode;
            mode.width = dm.dmPelsWidth;
            mode.height = dm.dmPelsHeight;
            mode.refreshHz = dm.dmDisplayFrequency;
            result->modes.push_back(mode);
        }
        m_modesResult.store(std::move(result));
        m_impl->opRunning.store(false);
        HTB_INFO("[display] {} modes loaded for {}", result->modes.size(), deviceName);
    });
}

void DisplayProvider::applyModeAsync(const std::string& deviceName, uint32_t width, uint32_t height,
                                     uint32_t refreshHz) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, deviceName, width, height, refreshHz] {
        auto result = std::make_shared<DisplayApplyResult>();
        result->deviceName = deviceName;

        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        dm.dmPelsWidth = width;
        dm.dmPelsHeight = height;
        dm.dmDisplayFrequency = refreshHz;
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;

        LONG rc = ChangeDisplaySettingsExW(toWide(deviceName).c_str(), &dm, nullptr, CDS_TEST, nullptr);
        if (rc != DISP_CHANGE_SUCCESSFUL) {
            result->message = "模式不受支持 (错误码 " + std::to_string(rc) + ")";
            m_applyResult.store(std::move(result));
            m_impl->opRunning.store(false);
            HTB_WARN("[display] mode test failed for {}: {}", deviceName, result->message);
            return;
        }
        rc = ChangeDisplaySettingsExW(toWide(deviceName).c_str(), &dm, nullptr, 0, nullptr);
        result->success = rc == DISP_CHANGE_SUCCESSFUL;
        result->message = result->success ? "已应用" : ("应用失败 (错误码 " + std::to_string(rc) + ")");
        m_applyResult.store(std::move(result));
        m_impl->opRunning.store(false);
        if (result->success) {
            HTB_INFO("[display] applied {}x{}@{} to {}", width, height, refreshHz, deviceName);
        } else {
            HTB_ERROR("[display] apply failed for {}: {}", deviceName, result->message);
        }
    });
}

void DisplayProvider::refresh() {
    auto displays = std::make_shared<std::vector<DisplayInfo>>();

    for (DWORD i = 0;; ++i) {
        DISPLAY_DEVICEW dd{};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesW(nullptr, i, &dd, 0)) break;

        DisplayInfo info;
        info.deviceName = toUtf8(dd.DeviceName);
        info.gpuName = toUtf8(dd.DeviceString);
        info.attached = (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0;
        info.primary = (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;

        DISPLAY_DEVICEW monitor{};
        monitor.cb = sizeof(monitor);
        if (EnumDisplayDevicesW(dd.DeviceName, 0, &monitor, 0)) {
            info.monitorName = toUtf8(monitor.DeviceString);
            const std::wstring monitorId = monitorIdFromDeviceId(monitor.DeviceID);
            const EdidData edid = readEdidForMonitor(monitorId);
            info.edidManufacturer = edid.manufacturer;
            info.edidSerial = edid.serial;
            info.edidWeek = edid.week;
            info.edidYear = edid.year;
            info.sizeInches = edid.sizeInches;
        }

        if (info.attached) {
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (EnumDisplaySettingsW(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                info.width = dm.dmPelsWidth;
                info.height = dm.dmPelsHeight;
                info.refreshHz = dm.dmDisplayFrequency;
                info.bitsPerPixel = dm.dmBitsPerPel;
                info.orientation = orientationName(dm.dmDisplayOrientation);
            }
        }
        info.source = "Win32 Display API (EnumDisplayDevices / EnumDisplaySettings)";
        displays->push_back(std::move(info));
    }

    HTB_INFO("[display] {} display device(s) enumerated", displays->size());
    m_snapshot.store(std::move(displays));
}

} // namespace htb
