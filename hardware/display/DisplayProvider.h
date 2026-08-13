#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"

namespace htb {

struct DisplayInfo {
    std::string deviceName;
    std::string gpuName;
    std::string monitorName;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshHz = 0;
    uint32_t bitsPerPixel = 0;
    std::string orientation;
    bool attached = false;
    bool primary = false;
    std::string source;

    std::string edidManufacturer;
    std::string edidSerial;
    uint32_t edidWeek = 0;
    uint32_t edidYear = 0;
    double sizeInches = 0.0;
};

class DisplayProvider final : public HardwareProvider {
public:
    DisplayProvider();

    std::string_view name() const override { return "display"; }
    void refresh() override;

    std::shared_ptr<const std::vector<DisplayInfo>> snapshot() const { return m_snapshot.load(); }

private:
    std::atomic<std::shared_ptr<const std::vector<DisplayInfo>>> m_snapshot;
};

} // namespace htb
