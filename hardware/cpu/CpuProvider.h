#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct CpuInfo {
    std::string name;
    std::string vendor;
    std::string architecture;
    uint32_t physicalCores = 0;
    uint32_t logicalCores = 0;
    std::optional<uint32_t> baseFrequencyMHz;
    std::string staticSource = "Win32 API / Registry";

    float totalUsage = 0.0f;
    std::vector<float> perCoreUsage;
    Availability usageAvailability = Availability::Unavailable;
    std::string usageSource;
    std::chrono::steady_clock::time_point usageTimestamp;
    std::optional<float> currentFrequencyMHz;
};

class CpuProvider final : public HardwareProvider {
public:
    CpuProvider();
    ~CpuProvider() override;

    std::string_view name() const override { return "cpu"; }
    void refresh() override;

    std::shared_ptr<const CpuInfo> snapshot() const { return m_snapshot.load(); }

private:
    void refreshStatic();
    void refreshUsage();
    void initPdh();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const CpuInfo>> m_snapshot;
};

} // namespace htb
