#pragma once

#include <cstdint>
#include <string_view>

namespace htb {

enum class Vendor { Unknown, Intel, Amd, Nvidia, Qualcomm, Microsoft, Broadcom, Realtek };

inline constexpr std::string_view vendorName(Vendor v) {
    switch (v) {
        case Vendor::Intel: return "Intel";
        case Vendor::Amd: return "AMD";
        case Vendor::Nvidia: return "NVIDIA";
        case Vendor::Qualcomm: return "Qualcomm";
        case Vendor::Microsoft: return "Microsoft";
        case Vendor::Broadcom: return "Broadcom";
        case Vendor::Realtek: return "Realtek";
        default: return "Unknown";
    }
}

inline Vendor vendorFromPciId(uint32_t vid) {
    switch (vid) {
        case 0x8086: return Vendor::Intel;
        case 0x10DE: return Vendor::Nvidia;
        case 0x1002:
        case 0x1022: return Vendor::Amd;
        case 0x5143: return Vendor::Qualcomm;
        case 0x1414: return Vendor::Microsoft;
        case 0x14E4: return Vendor::Broadcom;
        case 0x10EC: return Vendor::Realtek;
        default: return Vendor::Unknown;
    }
}

} // namespace htb
