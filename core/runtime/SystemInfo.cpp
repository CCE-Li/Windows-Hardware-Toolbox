#include "core/runtime/SystemInfo.h"

#include <cstdint>
#include <format>
#include <string>
#include <windows.h>

#include "core/util/Utf.h"

namespace htb {

namespace {
using RtlGetVersionFn = LONG(WINAPI*)(void*);

struct OsVersionInfoEx {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
};

std::string readRegString(const wchar_t* subKey, const wchar_t* valueName) {
    WCHAR buf[512]{};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, subKey, valueName, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    return toUtf8(buf);
}
} // namespace

SystemInfo querySystemInfo() {
    SystemInfo info;

    auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OsVersionInfoEx vi{};
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (rtlGetVersion && rtlGetVersion(&vi) == 0) {
        info.osVersion = std::format("{}.{}.{}", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
        info.osBuild = std::to_string(vi.dwBuildNumber);
    }

    const wchar_t* ntKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    info.osName = readRegString(ntKey, L"ProductName");
    info.osDisplayVersion = readRegString(ntKey, L"DisplayVersion");

    uint32_t buildNumber = 0;
    try {
        if (!info.osBuild.empty()) buildNumber = static_cast<uint32_t>(std::stoul(info.osBuild));
    } catch (...) {
    }
    if (buildNumber >= 22000 && info.osName.rfind("Windows 10", 0) == 0) {
        info.osName.replace(0, 10, "Windows 11");
    }

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: info.architecture = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: info.architecture = "ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: info.architecture = "x86"; break;
        default: info.architecture = "Unknown"; break;
    }
    return info;
}

} // namespace htb
