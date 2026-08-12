#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "hardware/HardwareTypes.h"
#include "monitoring/Metric.h"

namespace htb {

struct GpuInfo {
    std::string name;
    Vendor vendor = Vendor::Unknown;
    uint64_t dedicatedVramBytes = 0;
    uint64_t dedicatedSystemMemoryBytes = 0;
    uint64_t sharedSystemMemoryBytes = 0;

    std::string driverVersion;
    std::string driverDate;
    std::string driverSource;
    Availability driverAvailability = Availability::Unavailable;

    float usagePercent = 0.0f;
    uint64_t usageBytes = 0;
    Availability usageAvailability = Availability::Unavailable;
    std::string usageSource;

    std::vector<std::string> outputs;
};

class GpuProvider final : public HardwareProvider {
public:
    GpuProvider();

    std::string_view name() const override { return "gpu"; }
    void refresh() override;

    std::shared_ptr<const std::vector<GpuInfo>> snapshot() const { return m_snapshot.load(); }

private:
    std::atomic<std::shared_ptr<const std::vector<GpuInfo>>> m_snapshot;
};

} // namespace htb
