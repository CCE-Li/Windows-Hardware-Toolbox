#include <gtest/gtest.h>

#include "hardware/sensors/SensorsProvider.h"

TEST(Sensors, AcpiTenthsKelvinToCelsius) {
    // 3000 tenths Kelvin = 300.0 K = 26.85 C
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(3000), 26.85, 1e-9);
    // 2931 tenths Kelvin = 293.1 K = 19.95 C
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(2931), 19.95, 1e-9);
    // 2731 tenths Kelvin = 273.1 K = -0.05 C
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(2731), -0.05, 1e-9);
    // 0 tenths Kelvin (sensor reported 0 -> handled as unavailable upstream)
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(0), -273.15, 1e-9);
}

TEST(Sensors, AcpiRounding) {
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(3201), 46.95, 1e-9);
    EXPECT_NEAR(htb::acpiTenthsKelvinToCelsius(2999), 26.75, 1e-9);
}