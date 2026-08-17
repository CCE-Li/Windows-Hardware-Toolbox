#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/process/ProcessProvider.h"

#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <psapi.h>
#include <shellapi.h>
#include <winternl.h>

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include <vector>

namespace htb {

namespace {

extern "C" NTSTATUS NTAPI NtSuspendProcess(HANDLE ProcessHandle);
extern "C" NTSTATUS NTAPI NtResumeProcess(HANDLE ProcessHandle);

constexpr DWORD kProcessCommandLineInfo = 60;

#if defined(_WIN64)
struct SysProcInfo {
    ULONG NextEntryOffset;              // 0
    ULONG NumberOfThreads;              // 4
    LARGE_INTEGER WorkingSetPrivateSize; // 8
    ULONG HardFaultCount;               // 16
    ULONG NumberOfThreadsHighWatermark; // 20
    ULONGLONG CycleTime;                // 24
    LARGE_INTEGER CreateTime;           // 32
    LARGE_INTEGER UserTime;             // 40
    LARGE_INTEGER KernelTime;           // 48
    UNICODE_STRING ImageName;           // 56
    LONG BasePriority;                  // 72
    ULONG Reserved;                     // 76 (alignment padding)
    ULONG_PTR UniqueProcessId;          // 80
    ULONG_PTR InheritedFromUniqueProcessId; // 88
    ULONG HandleCount;                  // 96
    ULONG SessionId;                    // 100
};
static_assert(sizeof(SysProcInfo) == 104, "unexpected SYSTEM_PROCESS_INFORMATION prefix size");
#else
struct SysProcInfo {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    ULONG UniqueProcessId;
    ULONG InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
};
#endif

struct RawEntry {
    uint32_t pid = 0;
    uint32_t ppid = 0;
    uint32_t sessionId = 0;
    uint32_t threads = 0;
    uint32_t handles = 0;
    int basePriority = 0;
    std::wstring name;
    uint64_t kernelTime = 0;
    uint64_t userTime = 0;
};

uint64_t quadTicks(const LARGE_INTEGER& v) {
    return v.QuadPart > 0 ? static_cast<uint64_t>(v.QuadPart) : 0;
}

bool enumerateAll(std::vector<RawEntry>& out, uint64_t& totalTicks) {
#if defined(_WIN64)
    ULONG size = 0x200000;
    std::vector<BYTE> buffer;
    NTSTATUS status = 0;
    do {
        buffer.resize(size);
        ULONG returned = 0;
        status = NtQuerySystemInformation(SystemProcessInformation, buffer.data(), size, &returned);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            size = returned != 0 ? returned : size * 2;
        }
    } while (status == STATUS_INFO_LENGTH_MISMATCH);
    if (status != 0) return false;

    totalTicks = 0;
    const BYTE* base = buffer.data();
    const SysProcInfo* p = reinterpret_cast<const SysProcInfo*>(base);
    while (true) {
        RawEntry e;
        e.pid = static_cast<uint32_t>(p->UniqueProcessId);
        e.ppid = static_cast<uint32_t>(p->InheritedFromUniqueProcessId);
        e.sessionId = p->SessionId;
        e.threads = p->NumberOfThreads;
        e.handles = p->HandleCount;
        e.basePriority = p->BasePriority;
        if (p->ImageName.Buffer && p->ImageName.Length > 0 && p->ImageName.Length < 4096) {
            e.name.assign(p->ImageName.Buffer, p->ImageName.Length / sizeof(wchar_t));
        }
        e.kernelTime = quadTicks(p->KernelTime);
        e.userTime = quadTicks(p->UserTime);
        totalTicks += e.kernelTime + e.userTime;
        out.push_back(std::move(e));
        if (p->NextEntryOffset == 0) break;
        p = reinterpret_cast<const SysProcInfo*>(reinterpret_cast<const BYTE*>(p) + p->NextEntryOffset);
    }
    return true;
#else
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    totalTicks = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            RawEntry e;
            e.pid = pe.th32ProcessID;
            e.ppid = pe.th32ParentProcessID;
            e.threads = pe.cntThreads;
            e.basePriority = pe.pcPriClassBase;
            e.name = pe.szExeFile;
            const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.pid);
            if (h) {
                FILETIME created{}, exited{}, kernel{}, user{};
                if (GetProcessTimes(h, &created, &exited, &kernel, &user)) {
                    e.kernelTime = (static_cast<uint64_t>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
                    e.userTime = (static_cast<uint64_t>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
                }
                CloseHandle(h);
            }
            totalTicks += e.kernelTime + e.userTime;
            out.push_back(std::move(e));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return true;
#endif
}

std::string displayName(const RawEntry& e) {
    if (!e.name.empty()) return toUtf8(e.name);
    if (e.pid == 0) return "System Idle Process";
    if (e.pid == 4) return "System";
    return "System Process";
}

std::string priorityClassName(DWORD cls) {
    switch (cls) {
        case IDLE_PRIORITY_CLASS: return "空闲";
        case BELOW_NORMAL_PRIORITY_CLASS: return "低于正常";
        case NORMAL_PRIORITY_CLASS: return "正常";
        case ABOVE_NORMAL_PRIORITY_CLASS: return "高于正常";
        case HIGH_PRIORITY_CLASS: return "高";
        case REALTIME_PRIORITY_CLASS: return "实时";
        default: return "正常";
    }
}

std::string priorityFromBase(int base) {
    if (base <= 4) return "空闲";
    if (base <= 6) return "低于正常";
    if (base <= 8) return "正常";
    if (base <= 10) return "高于正常";
    if (base <= 13) return "高";
    return "实时";
}

std::string errorText() {
    wchar_t buf[256]{};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                   GetLastError(), 0, buf, 256, nullptr);
    return n > 0 ? toUtf8(std::wstring_view(buf, n)) : "未知错误";
}

std::pair<bool, std::string> doEndProcess(uint32_t pid) {
    const HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return {false, "无法打开进程 (权限不足或进程已退出)"};
    const BOOL ok = TerminateProcess(h, 1);
    const DWORD err = GetLastError();
    CloseHandle(h);
    if (ok) return {true, {}};
    return {false, "结束失败: " + errorText() + " (错误码 " + std::to_string(err) + ")"};
}

std::pair<bool, std::string> doEndTree(uint32_t rootPid) {
    std::vector<RawEntry> raw;
    uint64_t totalTicks = 0;
    if (!enumerateAll(raw, totalTicks)) return {false, "枚举进程失败"};
    std::unordered_map<uint32_t, uint32_t> parent;
    parent.reserve(raw.size());
    for (const RawEntry& e : raw) parent[e.pid] = e.ppid;

    std::vector<uint32_t> order;
    std::vector<uint32_t> stack{rootPid};
    while (!stack.empty()) {
        const uint32_t pid = stack.back();
        stack.pop_back();
        order.push_back(pid);
        for (const auto& [child, pp] : parent) {
            if (pp == pid) stack.push_back(child);
        }
    }
    std::reverse(order.begin(), order.end());

    std::string lastError;
    bool ok = true;
    for (const uint32_t pid : order) {
        const auto r = doEndProcess(pid);
        if (!r.first) {
            ok = false;
            lastError = r.second;
        }
    }
    if (!ok) return {false, "部分进程无法结束: " + lastError};
    return {true, {}};
}

std::pair<bool, std::string> doSuspend(uint32_t pid, bool suspend) {
    const HANDLE h = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!h) return {false, "无法打开进程 (权限不足或进程已退出)"};
    const NTSTATUS st = suspend ? NtSuspendProcess(h) : NtResumeProcess(h);
    CloseHandle(h);
    if (st >= 0) return {true, {}};
    return {false, std::string(suspend ? "暂停失败" : "恢复失败") + " (NTSTATUS " + std::to_string(st) + ")"};
}

DWORD priorityClassValue(ProcessPriority p) {
    switch (p) {
        case ProcessPriority::Idle: return IDLE_PRIORITY_CLASS;
        case ProcessPriority::BelowNormal: return BELOW_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::Normal: return NORMAL_PRIORITY_CLASS;
        case ProcessPriority::AboveNormal: return ABOVE_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::High: return HIGH_PRIORITY_CLASS;
        case ProcessPriority::Realtime: return REALTIME_PRIORITY_CLASS;
    }
    return NORMAL_PRIORITY_CLASS;
}

std::pair<bool, std::string> doSetPriority(uint32_t pid, ProcessPriority priority) {
    const HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return {false, "无法打开进程 (权限不足或进程已退出)"};
    const BOOL ok = SetPriorityClass(h, priorityClassValue(priority));
    const DWORD err = GetLastError();
    CloseHandle(h);
    if (ok) return {true, {}};
    return {false, "设置优先级失败: " + errorText() + " (错误码 " + std::to_string(err) + ")"};
}

std::pair<bool, std::string> doSetAffinity(uint32_t pid, uint64_t mask) {
    const HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return {false, "无法打开进程 (权限不足或进程已退出)"};
    ULONG_PTR systemMask = 0;
    if (!GetProcessAffinityMask(h, &systemMask, &systemMask)) {
        CloseHandle(h);
        return {false, "无法获取进程 CPU 亲和性"};
    }
    ULONG_PTR requested = static_cast<ULONG_PTR>(mask);
    if ((requested & systemMask) == 0) {
        CloseHandle(h);
        return {false, "亲和掩码与可用 CPU 无交集"};
    }
    const BOOL ok = SetProcessAffinityMask(h, requested);
    const DWORD err = GetLastError();
    CloseHandle(h);
    if (ok) return {true, {}};
    return {false, "设置 CPU 亲和性失败: " + errorText() + " (错误码 " + std::to_string(err) + ")"};
}

std::pair<bool, std::string> doRestart(uint32_t pid) {
    const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pid);
    if (!h) return {false, "无法打开进程 (权限不足或进程已退出)"};
    wchar_t path[MAX_PATH]{};
    DWORD len = MAX_PATH;
    const BOOL gotPath = QueryFullProcessImageNameW(h, 0, path, &len);
    if (gotPath) TerminateProcess(h, 1);
    CloseHandle(h);
    if (!gotPath) return {false, "无法获取可执行文件路径"};
    const HINSTANCE r = ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) <= 32) return {false, "重新启动失败"};
    return {true, {}};
}

std::shared_ptr<ProcessDetail> buildDetail(uint32_t pid) {
    auto detail = std::make_shared<ProcessDetail>();
    detail->pid = pid;

    const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        detail->status = "无法打开进程 (权限不足或进程已退出)";
        return detail;
    }

    wchar_t path[MAX_PATH]{};
    DWORD pathLen = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, path, &pathLen)) {
        detail->executablePath = toUtf8(std::wstring_view(path, pathLen));
    }

    ULONG len = 0;
    NTSTATUS st = NtQueryInformationProcess(h, static_cast<PROCESSINFOCLASS>(kProcessCommandLineInfo), nullptr, 0, &len);
    if (st == STATUS_INFO_LENGTH_MISMATCH && len > 0) {
        std::vector<BYTE> buffer(len);
        st = NtQueryInformationProcess(h, static_cast<PROCESSINFOCLASS>(kProcessCommandLineInfo), buffer.data(), len,
                                       nullptr);
        if (st >= 0) {
            const auto& us = *reinterpret_cast<const UNICODE_STRING*>(buffer.data());
            if (us.Buffer && us.Length > 0) {
                detail->commandLine = toUtf8(std::wstring_view(us.Buffer, us.Length / sizeof(wchar_t)));
            }
        }
    }

    HANDLE token = nullptr;
    if (OpenProcessToken(h, TOKEN_QUERY, &token)) {
        DWORD tlen = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &tlen);
        if (tlen > 0) {
            std::vector<BYTE> tbuf(tlen);
            if (GetTokenInformation(token, TokenUser, tbuf.data(), tlen, &tlen)) {
                const auto* tu = reinterpret_cast<const TOKEN_USER*>(tbuf.data());
                wchar_t user[256]{};
                wchar_t domain[256]{};
                DWORD un = 256;
                DWORD dn = 256;
                SID_NAME_USE use{};
                if (LookupAccountSidW(nullptr, tu->User.Sid, user, &un, domain, &dn, &use)) {
                    detail->userName = toUtf8(std::wstring(domain) + L"\\" + user);
                }
            }
        }
        CloseHandle(token);
    }

    CloseHandle(h);
    detail->status = "ok";
    return detail;
}

} // namespace

std::string processPriorityDisplayName(ProcessPriority priority) {
    switch (priority) {
        case ProcessPriority::Idle: return "空闲";
        case ProcessPriority::BelowNormal: return "低于正常";
        case ProcessPriority::Normal: return "正常";
        case ProcessPriority::AboveNormal: return "高于正常";
        case ProcessPriority::High: return "高";
        case ProcessPriority::Realtime: return "实时";
    }
    return "正常";
}

double computeProcessCpuPercent(uint64_t procDeltaTicks, uint64_t totalDeltaTicks) {
    if (totalDeltaTicks == 0) return 0.0;
    return std::min(100.0, static_cast<double>(procDeltaTicks) * 100.0 / static_cast<double>(totalDeltaTicks));
}

struct ProcessProvider::Impl {
    uint32_t coreCount = 0;
    struct Prev {
        uint64_t kernelUser = 0;
        uint64_t readBytes = 0;
        uint64_t writeBytes = 0;
    };
    std::unordered_map<uint32_t, Prev> prev;
    uint64_t prevTotalTicks = 0;
    std::chrono::steady_clock::time_point prevTime;
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

ProcessProvider::ProcessProvider() : m_impl(std::make_unique<Impl>()) {
    m_impl->coreCount = std::max(1u, static_cast<uint32_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
}

ProcessProvider::~ProcessProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

void ProcessProvider::refresh() {
    auto snap = std::make_shared<ProcessSnapshot>();
    snap->coreCount = m_impl->coreCount;

    std::vector<RawEntry> raw;
    uint64_t totalTicks = 0;
    if (!enumerateAll(raw, totalTicks)) {
        snap->availability = Availability::Unavailable;
        m_snapshot.store(std::move(snap));
        return;
    }

    MEMORYSTATUSEX msx{};
    msx.dwLength = sizeof(msx);
    if (GlobalMemoryStatusEx(&msx)) {
        snap->totalRamBytes = msx.ullTotalPhys;
        snap->busyRamBytes = msx.ullTotalPhys - msx.ullAvailPhys;
    }

    const auto now = std::chrono::steady_clock::now();
    const double dt = (m_impl->prevTime == std::chrono::steady_clock::time_point{})
                          ? 0.0
                          : std::chrono::duration<double>(now - m_impl->prevTime).count();
    const uint64_t prevTotal = m_impl->prevTotalTicks;
    const bool haveBaseline = prevTotal > 0 && totalTicks > prevTotal;
    const double dTotal = haveBaseline ? static_cast<double>(totalTicks - prevTotal) : 0.0;

    snap->timestamp = now;
    snap->processes.reserve(raw.size());

    std::unordered_map<uint32_t, Impl::Prev> nextPrev;
    nextPrev.reserve(raw.size());
    uint64_t idleDelta = 0;

    for (RawEntry& e : raw) {
        ProcessInfo info;
        info.pid = e.pid;
        info.ppid = e.ppid;
        info.name = displayName(e);
        info.sessionId = e.sessionId;
        info.threads = e.threads;
        info.handles = e.handles;
        info.basePriority = e.basePriority;
        info.kernelTime100ns = e.kernelTime;
        info.userTime100ns = e.userTime;

        const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.pid);
        if (h) {
            PROCESS_MEMORY_COUNTERS_EX pmc{};
            if (GetProcessMemoryInfo(h, reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc))) {
                info.workingSetBytes = pmc.WorkingSetSize;
                info.privateBytes = pmc.PrivateUsage;
                info.commitBytes = pmc.PagefileUsage;
            }
            IO_COUNTERS io{};
            if (GetProcessIoCounters(h, &io)) {
                info.readBytes = static_cast<uint64_t>(io.ReadTransferCount);
                info.writeBytes = static_cast<uint64_t>(io.WriteTransferCount);
            }
            const DWORD cls = GetPriorityClass(h);
            if (cls != 0) info.priority = priorityClassName(cls);
            CloseHandle(h);
        }
        if (info.priority.empty()) info.priority = priorityFromBase(e.basePriority);

        const uint64_t ku = e.kernelTime + e.userTime;
        const auto prevIt = m_impl->prev.find(e.pid);
        if (haveBaseline && prevIt != m_impl->prev.end()) {
            const uint64_t dProc = ku > prevIt->second.kernelUser ? ku - prevIt->second.kernelUser : 0;
            info.cpuPercent = computeProcessCpuPercent(dProc, totalTicks - prevTotal);
            if (dt > 0.0) {
                if (info.readBytes >= prevIt->second.readBytes)
                    info.readRateBps = static_cast<double>(info.readBytes - prevIt->second.readBytes) / dt;
                if (info.writeBytes >= prevIt->second.writeBytes)
                    info.writeRateBps = static_cast<double>(info.writeBytes - prevIt->second.writeBytes) / dt;
            }
            if (e.pid == 0) idleDelta = dProc;
        }
        nextPrev[e.pid] = {ku, info.readBytes, info.writeBytes};
        snap->processes.push_back(std::move(info));
    }

    if (haveBaseline && dTotal > 0.0) {
        const double busy = dTotal - static_cast<double>(idleDelta);
        snap->totalCpuPercent = std::clamp(busy / dTotal * 100.0, 0.0, 100.0);
    }

    m_impl->prev = std::move(nextPrev);
    m_impl->prevTotalTicks = totalTicks;
    m_impl->prevTime = now;

    const uint32_t inspectPid = m_inspectPid.exchange(0);
    if (inspectPid != 0) m_detail.store(buildDetail(inspectPid));

    m_snapshot.store(std::move(snap));
}

void ProcessProvider::runOperation(const std::string& operation, uint32_t pid,
                                   const std::function<std::pair<bool, std::string>()>& task) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, pid, task] {
        auto result = std::make_shared<ProcessOperationResult>();
        result->operation = operation;
        result->pid = pid;
        auto outcome = task();
        result->success = outcome.first;
        result->message = outcome.second;
        m_lastOperation.store(result);
        if (result->success) {
            HTB_INFO("[process] {} PID {} succeeded", operation, pid);
        } else {
            HTB_ERROR("[process] {} PID {} failed: {}", operation, pid, result->message);
        }
        m_impl->opRunning.store(false);
    });
}

void ProcessProvider::endProcess(uint32_t pid) {
    runOperation("结束任务", pid, [pid] { return doEndProcess(pid); });
}

void ProcessProvider::endProcessTree(uint32_t pid) {
    runOperation("结束进程树", pid, [pid] { return doEndTree(pid); });
}

void ProcessProvider::suspendProcess(uint32_t pid) {
    runOperation("暂停进程", pid, [pid] { return doSuspend(pid, true); });
}

void ProcessProvider::resumeProcess(uint32_t pid) {
    runOperation("恢复进程", pid, [pid] { return doSuspend(pid, false); });
}

void ProcessProvider::setPriority(uint32_t pid, ProcessPriority priority) {
    runOperation("设置优先级", pid, [pid, priority] { return doSetPriority(pid, priority); });
}

void ProcessProvider::setAffinity(uint32_t pid, uint64_t mask) {
    runOperation("设置 CPU 亲和性", pid, [pid, mask] { return doSetAffinity(pid, mask); });
}

void ProcessProvider::restartProcess(uint32_t pid) {
    runOperation("重启进程", pid, [pid] { return doRestart(pid); });
}

void ProcessProvider::inspectProcess(uint32_t pid) {
    m_inspectPid.store(pid);
}

} // namespace htb