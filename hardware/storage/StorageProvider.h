#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct NvmeHealth {
    Availability availability = Availability::Unavailable;
    std::string source = "IOCTL (NVMe Health Log)";
    std::optional<double> temperatureC;
    std::optional<uint8_t> percentageUsed;
    std::optional<uint64_t> powerOnHours;
    std::optional<uint64_t> powerCycles;
    std::optional<uint64_t> unsafeShutdowns;
    std::optional<uint64_t> mediaErrors;
    std::optional<uint64_t> dataUnitsRead;
    std::optional<uint64_t> dataUnitsWritten;
    std::string criticalWarning;
};

struct StorageDisk {
    std::string name;
    std::string serial;
    std::string firmware;
    std::string interfaceType;
    std::string mediaType;
    std::string busType;
    uint64_t sizeBytes = 0;
    std::string deviceId;
    bool isBoot = false;
    bool isSystem = false;

    std::string healthStatus;
    std::string operationalStatus;
    Availability healthAvailability = Availability::Unavailable;
    std::optional<double> temperatureC;
    std::string source;

    NvmeHealth nvme;
};

struct DiskActivity {
    Availability availability = Availability::Unavailable;
    std::string source = "PDH";
    float diskTimePercent = 0.0f;
    double readBps = 0.0;
    double writeBps = 0.0;
};

class StorageProvider final : public HardwareProvider {
public:
    StorageProvider();
    ~StorageProvider() override;

    std::string_view name() const override { return "storage"; }
    void refresh() override;

    std::shared_ptr<const std::vector<StorageDisk>> snapshot() const { return m_snapshot.load(); }
    std::shared_ptr<const DiskActivity> activity() const { return m_activity.load(); }

private:
    void refreshActivity();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<StorageDisk>>> m_snapshot;
    std::atomic<std::shared_ptr<const DiskActivity>> m_activity;
};

} // namespace htb
