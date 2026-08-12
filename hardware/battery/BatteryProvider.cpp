#include "core/logging/Logger.h"
#include "hardware/battery/BatteryProvider.h"
#include "hardware/wmi/WmiSession.h"

#include <windows.h>

namespace htb {

namespace {
std::string batteryStatusName(uint32_t status) {
    switch (status) {
        case 1: return "放电中";
        case 2: return "交流供电";
        case 3: return "已充满";
        case 4: return "电量低";
        case 5: return "电量严重不足";
        case 6: return "充电中";
        case 7: return "充电中 (高)";
        case 8: return "充电中 (低)";
        case 9: return "充电中 (严重不足)";
        case 10: return "未连接";
        default: return "未知";
    }
}
} // namespace

struct BatteryProvider::Impl {
    std::unique_ptr<WmiSession> wmi;
};

BatteryProvider::BatteryProvider() : m_impl(std::make_unique<Impl>()) {}

BatteryProvider::~BatteryProvider() = default;

void BatteryProvider::refresh() {
    auto info = std::make_shared<BatteryInfo>();
    if (!m_impl->wmi) m_impl->wmi = std::make_unique<WmiSession>();
    if (!m_impl->wmi->connect()) {
        m_snapshot.store(std::move(info));
        return;
    }
    bool found = false;
    m_impl->wmi->query(L"SELECT Name,EstimatedChargeRemaining,BatteryStatus FROM Win32_Battery",
                       [&](IWbemClassObject* obj) {
                           found = true;
                           readWmiString(obj, L"Name", info->name);
                           uint64_t percent = 0;
                           if (readWmiUint64(obj, L"EstimatedChargeRemaining", percent))
                               info->chargePercent = static_cast<uint32_t>(percent);
                           uint64_t status = 0;
                           if (readWmiUint64(obj, L"BatteryStatus", status))
                               info->status = batteryStatusName(static_cast<uint32_t>(status));
                           return true;
                       });
    info->availability = found ? Availability::Available : Availability::Unavailable;
    if (found) {
        HTB_DEBUG("[battery] {} at {}% ({})", info->name, info->chargePercent, info->status);
    } else {
        HTB_DEBUG("[battery] no battery reported");
    }
    m_snapshot.store(std::move(info));
}

} // namespace htb
