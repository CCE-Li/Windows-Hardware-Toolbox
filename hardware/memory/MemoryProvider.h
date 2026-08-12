#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct DimInfo {
    std::string manufacturer;
    std::string partNumber;
    uint64_t capacityBytes = 0;
    std::string memoryType;
    std::string speed;
    std::string deviceLocator;
    std::string bankLabel;
};

struct MemoryInfo {
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t availableBytes = 0;
    float loadPercent = 0.0f;
    std::string source = "Win32 API";

    std::vector<DimInfo> dimms;
    Availability dimmAvailability = Availability::Unavailable;
    std::string dimmSource = "WMI";
};

class MemoryProvider final : public HardwareProvider {
public:
    MemoryProvider();
    ~MemoryProvider() override;

    std::string_view name() const override { return "memory"; }
    void refresh() override;

    std::shared_ptr<const MemoryInfo> snapshot() const { return m_snapshot.load(); }

private:
    void refreshDimmInfo(MemoryInfo& info);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const MemoryInfo>> m_snapshot;
};

} // namespace htb
