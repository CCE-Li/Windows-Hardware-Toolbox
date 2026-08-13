#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
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

struct DisplayMode {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t refreshHz = 0;
};

struct DisplayModeResult {
    std::string deviceName;
    std::vector<DisplayMode> modes;
};

struct DisplayApplyResult {
    std::string deviceName;
    bool success = false;
    std::string message;
};

class DisplayProvider final : public HardwareProvider {
public:
    DisplayProvider();
    ~DisplayProvider() override;

    std::string_view name() const override { return "display"; }
    void refresh() override;

    std::shared_ptr<const std::vector<DisplayInfo>> snapshot() const { return m_snapshot.load(); }

    void loadModesAsync(const std::string& deviceName);
    std::shared_ptr<const DisplayModeResult> modesResult() const { return m_modesResult.load(); }
    void applyModeAsync(const std::string& deviceName, uint32_t width, uint32_t height, uint32_t refreshHz);
    std::shared_ptr<const DisplayApplyResult> applyResult() const { return m_applyResult.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<DisplayInfo>>> m_snapshot;
    std::atomic<std::shared_ptr<const DisplayModeResult>> m_modesResult;
    std::atomic<std::shared_ptr<const DisplayApplyResult>> m_applyResult;
};

} // namespace htb
