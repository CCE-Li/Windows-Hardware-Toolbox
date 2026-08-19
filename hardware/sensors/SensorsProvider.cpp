#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/sensors/SensorsProvider.h"
#include "hardware/wmi/WmiSession.h"

#include <exception>

namespace htb {

double acpiTenthsKelvinToCelsius(uint32_t tenthsKelvin) {
    return static_cast<double>(tenthsKelvin) / 10.0 - 273.15;
}

struct SensorsProvider::Impl {
    WmiSession wmi;
    bool wmiReady = false;
};

SensorsProvider::SensorsProvider() : m_impl(std::make_unique<Impl>()) {}

SensorsProvider::~SensorsProvider() = default;

void SensorsProvider::refresh() {
    auto snap = std::make_shared<SensorsSnapshot>();
    snap->timestamp = std::chrono::steady_clock::now();

    if (!m_impl->wmiReady) {
        m_impl->wmiReady = m_impl->wmi.connect();
        if (!m_impl->wmiReady) HTB_WARN("[sensors] WMI unavailable; thermal zones will be N/A");
    }

    if (m_impl->wmiReady) {
        try {
            uint32_t index = 0;
            m_impl->wmi.query(L"SELECT * FROM MSAcpi_ThermalZoneTemperature",
                              [&](IWbemClassObject* obj) {
                                  uint64_t raw = 0;
                                  std::string instance;
                                  readWmiUint64(obj, L"CurrentTemperature", raw);
                                  readWmiString(obj, L"InstanceName", instance);

                                  SensorReading r;
                                  r.id = "acpi_tz_" + std::to_string(index);
                                  r.name = "ACPI 热区 " + std::to_string(index);
                                  r.category = "温度";
                                  r.unit = "°C";
                                  r.value = acpiTenthsKelvinToCelsius(static_cast<uint32_t>(raw));
                                  r.source = "WMI (MSAcpi_ThermalZoneTemperature)";
                                  r.availability =
                                      raw == 0 ? Availability::Unavailable : Availability::Available;
                                  if (!instance.empty()) r.name += " (" + instance + ")";
                                  snap->sensors.push_back(std::move(r));
                                  ++index;
                                  return true;
                              });
        } catch (const std::exception& e) {
            HTB_ERROR("[sensors] thermal zone query failed: {}", e.what());
        } catch (...) {
            HTB_ERROR("[sensors] thermal zone query failed: unknown exception");
        }
    }

    if (snap->sensors.empty()) {
        SensorReading placeholder;
        placeholder.id = "thermal_zones";
        placeholder.name = "ACPI 热区";
        placeholder.category = "温度";
        placeholder.unit = "°C";
        placeholder.availability = Availability::Unavailable;
        placeholder.source = "WMI (MSAcpi_ThermalZoneTemperature)";
        snap->sensors.push_back(std::move(placeholder));
        HTB_INFO("[sensors] no ACPI thermal zones reported");
    } else {
        HTB_INFO("[sensors] {} thermal zone(s) reported", snap->sensors.size());
    }

    m_snapshot.store(std::move(snap));
}

} // namespace htb