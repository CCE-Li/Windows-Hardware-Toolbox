#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct SensorReading {
    std::string id;
    std::string name;
    std::string category;
    std::string unit;
    double value = 0.0;
    std::string source;
    Availability availability = Availability::Unavailable;
};

struct SensorsSnapshot {
    std::vector<SensorReading> sensors;
    std::chrono::steady_clock::time_point timestamp;
    std::string source = "WMI ACPI";
};

// MSAcpi_ThermalZoneTemperature.CurrentTemperature is in tenths of degrees Kelvin.
double acpiTenthsKelvinToCelsius(uint32_t tenthsKelvin);

class SensorsProvider final : public HardwareProvider {
public:
    SensorsProvider();
    ~SensorsProvider() override;

    std::string_view name() const override { return "sensors"; }
    void refresh() override;

    std::shared_ptr<const SensorsSnapshot> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const SensorsSnapshot>> m_snapshot;
};

} // namespace htb