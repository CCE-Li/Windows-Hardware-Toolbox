#include "core/logging/Logger.h"
#include "hardware/memory/MemoryProvider.h"
#include "hardware/wmi/WmiSession.h"

#include <windows.h>

namespace htb {

namespace {
std::string memoryTypeName(int smbiosType) {
    switch (smbiosType) {
        case 20: return "DDR";
        case 21: return "DDR2";
        case 22: return "DDR2 FB-DIMM";
        case 24: return "DDR3";
        case 25: return "LPDDR3";
        case 26: return "DDR4";
        case 27: return "DDR5";
        case 28: return "LPDDR4";
        case 29: return "LPDDR5";
        case 30: return "LPDDR5x";
        default: return {};
    }
}
} // namespace

struct MemoryProvider::Impl {
    std::unique_ptr<WmiSession> wmi;
};

MemoryProvider::MemoryProvider() : m_impl(std::make_unique<Impl>()) {}

MemoryProvider::~MemoryProvider() = default;

void MemoryProvider::refresh() {
    auto info = std::make_shared<MemoryInfo>();

    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) {
        info->totalBytes = st.ullTotalPhys;
        info->availableBytes = st.ullAvailPhys;
        info->usedBytes = info->totalBytes - info->availableBytes;
        info->loadPercent = static_cast<float>(st.dwMemoryLoad);
        info->source = "Win32 API";
    } else {
        HTB_WARN("[memory] GlobalMemoryStatusEx failed; totals marked Unavailable");
    }

    refreshDimmInfo(*info);
    m_snapshot.store(std::move(info));
}

void MemoryProvider::refreshDimmInfo(MemoryInfo& info) {
    if (!m_impl->wmi) m_impl->wmi = std::make_unique<WmiSession>();
    if (!m_impl->wmi->connect()) return;

    const bool ok = m_impl->wmi->query(
        L"SELECT Manufacturer,PartNumber,Capacity,Speed,SMBIOSMemoryType,DeviceLocator,BankLabel "
        L"FROM Win32_PhysicalMemory",
        [&](IWbemClassObject* obj) {
            DimInfo dim;
            readWmiString(obj, L"Manufacturer", dim.manufacturer);
            readWmiString(obj, L"PartNumber", dim.partNumber);
            readWmiString(obj, L"DeviceLocator", dim.deviceLocator);
            readWmiString(obj, L"BankLabel", dim.bankLabel);
            readWmiUint64(obj, L"Capacity", dim.capacityBytes);
            uint64_t speed = 0;
            if (readWmiUint64(obj, L"Speed", speed) && speed > 0) dim.speed = std::to_string(speed) + " MHz";
            uint64_t smbiosType = 0;
            if (readWmiUint64(obj, L"SMBIOSMemoryType", smbiosType))
                dim.memoryType = memoryTypeName(static_cast<int>(smbiosType));
            info.dimms.push_back(std::move(dim));
            return true;
        });
    info.dimmAvailability = ok ? Availability::Available : Availability::Unavailable;
    if (ok) {
        HTB_DEBUG("[memory] {} DIMM module(s) reported by WMI", info.dimms.size());
    } else {
        HTB_WARN("[memory] WMI DIMM query failed; modules marked Unavailable");
    }
}

} // namespace htb
