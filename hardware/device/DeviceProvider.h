#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"

namespace htb {

struct DeviceInfo {
    std::string instanceId;
    std::string parentId;
    std::string name;
    std::string description;
    std::string manufacturer;
    std::string className;
    std::string classGuid;
    std::string enumerator;
    std::string service;
    std::string driverKey;
    std::string driverVersion;
    std::string driverDate;
    std::string driverProvider;
    std::string locationPaths;
    uint32_t busNumber = 0;
    std::vector<std::string> hardwareIds;
    std::vector<std::string> compatibleIds;
    uint32_t problem = 0;
    bool started = false;
    bool disabled = false;
};

class DeviceProvider final : public HardwareProvider {
public:
    DeviceProvider();
    ~DeviceProvider() override;

    std::string_view name() const override { return "device"; }
    void refresh() override;
    void requestRefresh();

    std::shared_ptr<const std::vector<DeviceInfo>> snapshot() const { return m_snapshot.load(); }
    std::chrono::steady_clock::time_point lastEnumTime() const { return m_lastEnum.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<DeviceInfo>>> m_snapshot;
    std::atomic<std::chrono::steady_clock::time_point> m_lastEnum{};
    std::atomic<bool> m_force{false};
};

} // namespace htb
