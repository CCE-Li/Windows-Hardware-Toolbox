#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "monitoring/Metric.h"

namespace htb {

inline std::string formatBytes(uint64_t bytes) {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 5) {
        v /= 1024.0;
        ++u;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
    return buf;
}

inline std::string formatMhz(uint32_t mhz) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GHz", mhz / 1000.0);
    return buf;
}

inline std::string availabilityLabel(Availability a) {
    return std::string(toString(a));
}

} // namespace htb
