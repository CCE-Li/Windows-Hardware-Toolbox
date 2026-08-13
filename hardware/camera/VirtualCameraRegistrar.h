#pragma once

#include <string>

namespace htb {

class VirtualCameraRegistrar {
public:
    static long registerVirtualCamera(const std::string& friendlyName);
    static long unregisterVirtualCamera(const std::string& friendlyName);
};

} // namespace htb
