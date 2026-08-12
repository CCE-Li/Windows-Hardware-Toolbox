#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct BatteryInfo {
    std::string name;
    uint32_t chargePercent = 0;
    std::string status;
    Availability availability = Availability::Unavailable;
    std::string source = "WMI";
};

class BatteryProvider final : public HardwareProvider {
public:
    BatteryProvider();
    ~BatteryProvider() override;

    std::string_view name() const override { return "battery"; }
    void refresh() override;

    std::shared_ptr<const BatteryInfo> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const BatteryInfo>> m_snapshot;
};

} // namespace htb
