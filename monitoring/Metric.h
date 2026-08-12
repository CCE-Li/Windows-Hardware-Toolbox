#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace htb {

enum class Availability {
    Available,
    Unavailable,
    Unsupported,
    PermissionDenied,
};

inline std::string_view toString(Availability a) {
    switch (a) {
        case Availability::Available: return "Available";
        case Availability::Unavailable: return "N/A";
        case Availability::Unsupported: return "Unsupported";
        case Availability::PermissionDenied: return "Permission denied";
    }
    return "N/A";
}

struct Metric {
    std::string name;
    double value = 0.0;
    std::string unit;
    std::string source;
    Availability availability = Availability::Unavailable;
    std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now();
};

} // namespace htb
