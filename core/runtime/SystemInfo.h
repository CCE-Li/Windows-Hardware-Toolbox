#pragma once

#include <string>

namespace htb {

struct SystemInfo {
    std::string osName;
    std::string osDisplayVersion;
    std::string osVersion;
    std::string osBuild;
    std::string architecture;
};

SystemInfo querySystemInfo();

} // namespace htb
