#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/display/DisplayProvider.h"

#include <windows.h>

#include <string>

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
} // namespace

DisplayProvider::DisplayProvider() = default;

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
