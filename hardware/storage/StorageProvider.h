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
};

class StorageProvider final : public HardwareProvider {
public:
    StorageProvider();
    ~StorageProvider() override;

    std::string_view name() const override { return "storage"; }
    void refresh() override;

    std::shared_ptr<const std::vector<StorageDisk>> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<StorageDisk>>> m_snapshot;
};

} // namespace htb
