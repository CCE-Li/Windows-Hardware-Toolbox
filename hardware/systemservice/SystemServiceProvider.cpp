#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/systemservice/SystemServiceProvider.h"

#include <windows.h>

#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

namespace htb {

namespace {

constexpr std::chrono::steady_clock::duration kConfigMaxAge = std::chrono::seconds(60);

struct ServiceConfig {
    std::string description;
    std::string startType;
    std::string binaryPath;
    std::chrono::steady_clock::time_point cachedAt;
};

std::string stateName(DWORD state) {
    switch (state) {
        case SERVICE_STOPPED: return "已停止";
        case SERVICE_START_PENDING: return "启动中";
        case SERVICE_STOP_PENDING: return "停止中";
        case SERVICE_RUNNING: return "运行中";
        case SERVICE_CONTINUE_PENDING: return "继续中";
        case SERVICE_PAUSE_PENDING: return "暂停中";
        case SERVICE_PAUSED: return "已暂停";
        default: return "未知";
    }
}

std::string startTypeName(DWORD startType) {
    switch (startType) {
        case SERVICE_BOOT_START: return "引导启动";
        case SERVICE_SYSTEM_START: return "系统启动";
        case SERVICE_AUTO_START: return "自动";
        case SERVICE_DEMAND_START: return "手动";
        case SERVICE_DISABLED: return "禁用";
        default: return "未知";
    }
}

std::string errorText() {
    wchar_t buf[256]{};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                   GetLastError(), 0, buf, 256, nullptr);
    return n > 0 ? toUtf8(std::wstring_view(buf, n)) : "未知错误";
}

std::pair<bool, std::string> doStart(const std::wstring& name) {
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return {false, "无法连接服务控制管理器: " + errorText()};
    const SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_START);
    if (!svc) {
        const std::string err = errorText();
        CloseServiceHandle(scm);
        return {false, "无法打开服务 (权限不足?): " + err};
    }
    const BOOL ok = StartServiceW(svc, 0, nullptr);
    const DWORD code = GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (ok || code == ERROR_SERVICE_ALREADY_RUNNING) return {true, {}};
    return {false, "启动服务失败: " + errorText() + " (错误码 " + std::to_string(code) + ")"};
}

std::pair<bool, std::string> doStop(const std::wstring& name) {
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return {false, "无法连接服务控制管理器: " + errorText()};
    const SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_STOP);
    if (!svc) {
        const std::string err = errorText();
        CloseServiceHandle(scm);
        return {false, "无法打开服务 (权限不足?): " + err};
    }
    SERVICE_STATUS status{};
    const BOOL ok = ControlService(svc, SERVICE_CONTROL_STOP, &status);
    const DWORD code = GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (ok) return {true, {}};
    if (code == ERROR_SERVICE_NOT_ACTIVE) return {true, {}};
    return {false, "停止服务失败: " + errorText() + " (错误码 " + std::to_string(code) + ")"};
}

std::pair<bool, std::string> doRestart(const std::wstring& name) {
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return {false, "无法连接服务控制管理器"};
    const SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        const std::string err = errorText();
        CloseServiceHandle(scm);
        return {false, "无法打开服务 (权限不足?): " + err};
    }
    SERVICE_STATUS status{};
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    for (int i = 0; i < 50; ++i) {
        QueryServiceStatus(svc, &status);
        if (status.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(100);
    }
    const BOOL ok = StartServiceW(svc, 0, nullptr);
    const DWORD code = GetLastError();
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    if (ok || code == ERROR_SERVICE_ALREADY_RUNNING) return {true, {}};
    return {false, "重启服务失败: " + errorText() + " (错误码 " + std::to_string(code) + ")"};
}

ServiceConfig loadConfig(SC_HANDLE scm, const std::wstring& name) {
    ServiceConfig cfg;
    const SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_CONFIG);
    if (!svc) return cfg;

    DWORD bytes = 0;
    QueryServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytes);
    if (bytes > 0) {
        std::vector<BYTE> buffer(bytes);
        if (QueryServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, buffer.data(), bytes, &bytes)) {
            const auto* desc = reinterpret_cast<const SERVICE_DESCRIPTIONW*>(buffer.data());
            if (desc->lpDescription) cfg.description = toUtf8(desc->lpDescription);
        }
    }

    DWORD cfgBytes = 0;
    QueryServiceConfigW(svc, nullptr, 0, &cfgBytes);
    if (cfgBytes > 0) {
        std::vector<BYTE> buffer(cfgBytes);
        if (QueryServiceConfigW(svc, reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(buffer.data()), cfgBytes, &cfgBytes)) {
            const auto* qsc = reinterpret_cast<const QUERY_SERVICE_CONFIGW*>(buffer.data());
            cfg.startType = startTypeName(qsc->dwStartType);
            if (qsc->lpBinaryPathName) cfg.binaryPath = toUtf8(qsc->lpBinaryPathName);
        }
    }
    CloseServiceHandle(svc);
    cfg.cachedAt = std::chrono::steady_clock::now();
    return cfg;
}

} // namespace

struct SystemServiceProvider::Impl {
    std::unordered_map<std::string, ServiceConfig> configCache;
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

SystemServiceProvider::SystemServiceProvider() : m_impl(std::make_unique<Impl>()) {}

SystemServiceProvider::~SystemServiceProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

void SystemServiceProvider::refresh() {
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) {
        HTB_ERROR("[systemservice] OpenSCManagerW failed: {}", GetLastError());
        m_snapshot.store(std::make_shared<std::vector<ServiceInfo>>());
        return;
    }

    auto services = std::make_shared<std::vector<ServiceInfo>>();
    DWORD bytes = 0;
    DWORD count = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &bytes, &count,
                          nullptr, nullptr);
    if (bytes == 0) {
        CloseServiceHandle(scm);
        m_snapshot.store(std::move(services));
        return;
    }
    std::vector<BYTE> buffer(bytes);
    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buffer.data(),
                               static_cast<DWORD>(buffer.size()), &bytes, &count, nullptr, nullptr)) {
        CloseServiceHandle(scm);
        m_snapshot.store(std::move(services));
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto* entry = reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    services->reserve(count);
    for (DWORD i = 0; i < count; ++i) {
        ServiceInfo info;
        info.name = toUtf8(entry->lpServiceName);
        info.displayName = toUtf8(entry->lpDisplayName);
        info.state = stateName(entry->ServiceStatusProcess.dwCurrentState);
        info.canStop = (entry->ServiceStatusProcess.dwControlsAccepted & SERVICE_ACCEPT_STOP) != 0;
        info.pid = entry->ServiceStatusProcess.dwProcessId;

        auto cached = m_impl->configCache.find(info.name);
        if (cached == m_impl->configCache.end() || (now - cached->second.cachedAt) > kConfigMaxAge) {
            const ServiceConfig cfg = loadConfig(scm, toWide(info.name));
            m_impl->configCache[info.name] = cfg;
            cached = m_impl->configCache.find(info.name);
        }
        info.description = cached->second.description;
        info.startType = cached->second.startType;
        info.binaryPath = cached->second.binaryPath;
        services->push_back(std::move(info));
        ++entry;
    }
    CloseServiceHandle(scm);

    m_snapshot.store(std::move(services));
}

void SystemServiceProvider::runOperation(const std::string& operation, const std::string& name,
                                         const std::function<std::pair<bool, std::string>()>& task) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, name, task] {
        auto result = std::make_shared<ServiceOperationResult>();
        result->operation = operation;
        result->name = name;
        auto outcome = task();
        result->success = outcome.first;
        result->message = outcome.second;
        m_lastOperation.store(result);
        if (result->success) {
            HTB_INFO("[service] {} {} succeeded", operation, name);
        } else {
            HTB_ERROR("[service] {} {} failed: {}", operation, name, result->message);
        }
        m_impl->opRunning.store(false);
    });
}

void SystemServiceProvider::startService(const std::string& name) {
    runOperation("启动服务", name, [name] { return doStart(toWide(name)); });
}

void SystemServiceProvider::stopService(const std::string& name) {
    runOperation("停止服务", name, [name] { return doStop(toWide(name)); });
}

void SystemServiceProvider::restartService(const std::string& name) {
    runOperation("重启服务", name, [name] { return doRestart(toWide(name)); });
}

} // namespace htb