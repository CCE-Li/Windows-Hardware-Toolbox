#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/cpu/CpuProvider.h"

#include <intrin.h>
#include <pdh.h>
#include <windows.h>

#include <array>
#include <cstring>

namespace htb {

namespace {
std::string readRegString(const wchar_t* subKey, const wchar_t* valueName) {
    WCHAR buf[512]{};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, subKey, valueName, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    return toUtf8(buf);
}

uint32_t readRegDword(const wchar_t* subKey, const wchar_t* valueName) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, subKey, valueName, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS)
        return 0;
    return value;
}

std::string cpuVendor() {
    std::array<int, 4> regs{};
    __cpuid(regs.data(), 0);
    char brand[13]{};
    std::memcpy(brand, &regs[1], 4);
    std::memcpy(brand + 4, &regs[3], 4);
    std::memcpy(brand + 8, &regs[2], 4);
    return std::string(brand, 12);
}

uint32_t physicalCoreCount() {
    DWORD size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return 0;
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX> buffer(size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) + 2);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, buffer.data(), &size)) return 0;
    uint32_t count = 0;
    const char* p = reinterpret_cast<const char*>(buffer.data());
    const char* end = p + size;
    while (p < end) {
        const auto* entry = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(p);
        if (entry->Relationship == RelationProcessorCore) ++count;
        p += entry->Size;
    }
    return count;
}

std::string architectureName(DWORD arch) {
    switch (arch) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        default: return "Unknown";
    }
}
} // namespace

struct CpuProvider::Impl {
    PDH_HQUERY query = nullptr;
    std::vector<PDH_HCOUNTER> counters;
    PDH_HCOUNTER performanceCounter = nullptr;
    bool pdhReady = false;
    bool pdhSeeded = false;
    bool staticDone = false;
};

CpuProvider::CpuProvider() : m_impl(std::make_unique<Impl>()) {}

CpuProvider::~CpuProvider() {
    if (m_impl->query) PdhCloseQuery(m_impl->query);
}

void CpuProvider::refresh() {
    if (!m_impl->staticDone) refreshStatic();
    refreshUsage();
}

void CpuProvider::refreshStatic() {
    m_impl->staticDone = true;
    auto info = std::make_shared<CpuInfo>();

    info->name = readRegString(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    const std::string brand = cpuVendor();
    if (brand.find("GenuineIntel") != std::string::npos) {
        info->vendor = "Intel";
    } else if (brand.find("AuthenticAMD") != std::string::npos || brand.find("HygonGenuine") != std::string::npos) {
        info->vendor = "AMD";
    } else if (brand.find("Qualcomm") != std::string::npos) {
        info->vendor = "Qualcomm";
    } else {
        info->vendor = brand;
    }

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    info->architecture = architectureName(si.wProcessorArchitecture);
    info->logicalCores = si.dwNumberOfProcessors;
    info->physicalCores = physicalCoreCount();
    if (info->physicalCores == 0) info->physicalCores = info->logicalCores;

    const uint32_t mhz = readRegDword(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"~MHz");
    if (mhz > 0) info->baseFrequencyMHz = mhz;

    HTB_INFO("CPU: {} [{}] {}P/{}L, base {} MHz", info->name, info->vendor, info->physicalCores,
             info->logicalCores, info->baseFrequencyMHz.value_or(0));
    m_snapshot.store(std::move(info));
}

void CpuProvider::initPdh() {
    if (PdhOpenQueryW(nullptr, 0, &m_impl->query) != ERROR_SUCCESS) {
        HTB_WARN("[cpu] PDH open query failed; usage will be N/A");
        return;
    }
    const auto info = m_snapshot.load();
    const uint32_t cores = info ? info->logicalCores : 1;

    std::vector<PDH_HCOUNTER> counters;
    bool ok = true;
    auto add = [&](const wchar_t* path, PDH_HCOUNTER& counter) {
        const PDH_STATUS st = PdhAddEnglishCounterW(m_impl->query, path, 0, &counter);
        if (st != ERROR_SUCCESS) {
            HTB_WARN("[cpu] PDH add counter failed ({:#x}) for {}", static_cast<unsigned>(st), toUtf8(path));
            return false;
        }
        return true;
    };

    PDH_HCOUNTER total{};
    ok = add(L"\\Processor Information(_Total)\\% Processor Time", total);
    PDH_HCOUNTER perf{};
    if (ok) {
        const PDH_STATUS perfSt = PdhAddEnglishCounterW(
            m_impl->query, L"\\Processor Information(_Total)\\% Processor Performance", 0, &perf);
        if (perfSt == ERROR_SUCCESS) m_impl->performanceCounter = perf;
    }
    for (uint32_t i = 0; ok && i < cores; ++i) {
        const std::wstring path = std::format(L"\\Processor Information({})\\% Processor Time", i);
        PDH_HCOUNTER c{};
        if (add(path.c_str(), c)) counters.push_back(c);
    }
    if (!ok) {
        HTB_WARN("[cpu] PDH unavailable; usage marked Unavailable");
        PdhCloseQuery(m_impl->query);
        m_impl->query = nullptr;
        return;
    }
    counters.insert(counters.begin(), total);
    m_impl->counters = std::move(counters);
    m_impl->pdhReady = true;
}

void CpuProvider::refreshUsage() {
    auto info = m_snapshot.load();
    if (!info) return;
    auto updated = std::make_shared<CpuInfo>(*info);

    if (!m_impl->pdhReady) initPdh();
    if (!m_impl->pdhReady) {
        updated->usageAvailability = Availability::Unavailable;
        updated->usageSource = "PDH";
        m_snapshot.store(std::move(updated));
        return;
    }
    if (!m_impl->pdhSeeded) {
        m_impl->pdhSeeded = true;
        PdhCollectQueryData(m_impl->query);
        updated->usageAvailability = Availability::Unavailable;
        updated->usageSource = "PDH";
        m_snapshot.store(std::move(updated));
        return;
    }
    if (PdhCollectQueryData(m_impl->query) != ERROR_SUCCESS) return;

    updated->perCoreUsage.clear();
    updated->perCoreUsage.reserve(m_impl->counters.size());
    for (const PDH_HCOUNTER counter : m_impl->counters) {
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS) return;
        float pct = static_cast<float>(value.doubleValue);
        pct = pct < 0.0f ? 0.0f : (pct > 100.0f ? 100.0f : pct);
        updated->perCoreUsage.push_back(pct);
    }
    if (updated->perCoreUsage.empty()) return;
    updated->totalUsage = updated->perCoreUsage.front();

    if (m_impl->performanceCounter) {
        PDH_FMT_COUNTERVALUE perfValue{};
        if (PdhGetFormattedCounterValue(m_impl->performanceCounter, PDH_FMT_DOUBLE, nullptr, &perfValue) ==
                ERROR_SUCCESS &&
            perfValue.doubleValue > 0.0 && info->baseFrequencyMHz) {
            updated->currentFrequencyMHz =
                static_cast<float>(info->baseFrequencyMHz.value() * perfValue.doubleValue / 100.0);
        }
    }

    updated->usageAvailability = Availability::Available;
    updated->usageSource = "PDH";
    updated->usageTimestamp = std::chrono::steady_clock::now();
    m_snapshot.store(std::move(updated));
}

} // namespace htb
