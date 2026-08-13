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

    float engineUsagePercent = 0.0f;
    Availability engineAvailability = Availability::Unavailable;
    std::string engineSource;

    std::vector<std::string> outputs;
};

class GpuProvider final : public HardwareProvider {
public:
    GpuProvider();
    ~GpuProvider() override;

    std::string_view name() const override { return "gpu"; }
    void refresh() override;

    std::shared_ptr<const std::vector<GpuInfo>> snapshot() const { return m_snapshot.load(); }

private:
    void refreshEngineUsage();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<GpuInfo>>> m_snapshot;
    float m_engineUsage = 0.0f;
};

} // namespace htb
