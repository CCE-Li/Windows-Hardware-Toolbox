#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/startup/StartupProvider.h"

#include <windows.h>
#include <shellapi.h>

#include <utility>
#include <vector>

namespace htb {

namespace {

constexpr wchar_t kDisabledSuffix[] = L" (HTB 已禁用)";
constexpr wchar_t kFileDisabledSuffix[] = L".disabled";

std::string errorText() {
    wchar_t buf[256]{};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                   GetLastError(), 0, buf, 256, nullptr);
    return n > 0 ? toUtf8(std::wstring_view(buf, n)) : "未知错误";
}

bool endsWith(const std::wstring& s, const std::wstring& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void enumRegKey(HKEY root, const std::wstring& rootName, const std::wstring& subKey,
                std::vector<StartupItem>& out) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    const std::wstring suffix(kDisabledSuffix);
    for (DWORD i = 0;; ++i) {
        wchar_t valueName[256]{};
        wchar_t data[4096]{};
        DWORD nlen = 256;
        DWORD dlen = sizeof(data);
        DWORD type = 0;
        const LSTATUS st =
            RegEnumValueW(hKey, i, valueName, &nlen, nullptr, &type, reinterpret_cast<BYTE*>(data), &dlen);
        if (st == ERROR_NO_MORE_ITEMS) break;
        if (st != ERROR_SUCCESS) continue;

        std::wstring name = valueName;
        bool disabled = false;
        if (endsWith(name, suffix)) {
            disabled = true;
            name.resize(name.size() - suffix.size());
        }
        if (name.empty()) continue;

        StartupItem item;
        item.id = toUtf8(rootName + L"\\" + subKey + L"\\" + name);
        item.name = toUtf8(name);
        item.command = toUtf8(data);
        item.location = "注册表: " + toUtf8(rootName + L"\\" + subKey);
        item.locationShort = toUtf8(rootName) + " 注册表";
        item.enabled = !disabled;
        item.status = disabled ? "已禁用" : "已启用";
        item.isRegistry = true;
        item.regRoot = toUtf8(rootName);
        item.regPath = toUtf8(subKey);
        item.valueName = toUtf8(name);
        out.push_back(std::move(item));
    }
    RegCloseKey(hKey);
}

void enumFolder(const std::wstring& dir, const std::string& locationShort, std::vector<StartupItem>& out) {
    if (dir.empty()) return;
    const std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd{};
    const HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    const std::wstring suffix(kFileDisabledSuffix);
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fname = fd.cFileName;
        bool disabled = false;
        if (endsWith(fname, suffix)) {
            disabled = true;
            fname.resize(fname.size() - suffix.size());
        }
        if (fname.empty()) continue;

        StartupItem item;
        item.id = toUtf8(dir + L"\\" + fd.cFileName);
        item.name = toUtf8(fname);
        item.command = toUtf8(fname);
        item.filePath = toUtf8(dir + L"\\" + fd.cFileName);
        item.location = "启动文件夹: " + toUtf8(dir);
        item.locationShort = locationShort;
        item.enabled = !disabled;
        item.status = disabled ? "已禁用" : "已启用";
        item.isRegistry = false;
        item.sizeBytes = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        item.source = "启动文件夹";
        out.push_back(std::move(item));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

std::pair<bool, std::string> doSetEnabled(const StartupItem& item, bool enable) {
    if (item.isRegistry) {
        const HKEY root = (item.regRoot == "HKLM") ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(root, toWide(item.regPath).c_str(), 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            return {false, "无法打开注册表项"};
        const std::wstring origName = toWide(item.valueName);
        const std::wstring suffix(kDisabledSuffix);
        const std::wstring current = enable ? origName + suffix : origName;
        const std::wstring target = enable ? origName : origName + suffix;

        wchar_t data[4096]{};
        DWORD dlen = sizeof(data);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, current.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(data), &dlen) !=
            ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return {false, "找不到注册表值 (可能已被其他程序修改)"};
        }
        const LSTATUS wr = RegSetValueExW(hKey, target.c_str(), 0, type, reinterpret_cast<const BYTE*>(data), dlen);
        if (wr == ERROR_SUCCESS) RegDeleteValueW(hKey, current.c_str());
        const DWORD err = GetLastError();
        RegCloseKey(hKey);
        if (wr != ERROR_SUCCESS) return {false, "写入注册表失败 (错误码 " + std::to_string(err) + ")"};
        return {true, {}};
    }

    std::wstring current = toWide(item.filePath);
    std::wstring target = current;
    const std::wstring suffix(kFileDisabledSuffix);
    if (enable) {
        if (endsWith(current, suffix)) target = current.substr(0, current.size() - suffix.size());
    } else {
        target = current + suffix;
    }
    if (current == target) return {true, {}};
    if (!MoveFileW(current.c_str(), target.c_str())) return {false, "重命名失败: " + errorText()};
    return {true, {}};
}

std::pair<bool, std::string> doRemove(const StartupItem& item) {
    if (item.isRegistry) {
        const HKEY root = (item.regRoot == "HKLM") ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(root, toWide(item.regPath).c_str(), 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            return {false, "无法打开注册表项"};
        std::wstring current = toWide(item.valueName);
        const std::wstring suffix(kDisabledSuffix);
        if (endsWith(current, suffix)) current = current.substr(0, current.size() - suffix.size());
        const LSTATUS st = RegDeleteValueW(hKey, current.c_str());
        RegCloseKey(hKey);
        if (st != ERROR_SUCCESS) return {false, "删除注册表值失败 (错误码 " + std::to_string(st) + ")"};
        return {true, {}};
    }
    if (!DeleteFileW(toWide(item.filePath).c_str())) return {false, "删除文件失败: " + errorText()};
    return {true, {}};
}

std::wstring extractCommandPath(const std::wstring& command) {
    if (command.empty()) return {};
    if (command.front() == L'"') {
        const size_t end = command.find(L'"', 1);
        return end == std::wstring::npos ? command.substr(1) : command.substr(1, end - 1);
    }
    const size_t space = command.find(L' ');
    return space == std::wstring::npos ? command : command.substr(0, space);
}

} // namespace

struct StartupProvider::Impl {
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

StartupProvider::StartupProvider() : m_impl(std::make_unique<Impl>()) {}

StartupProvider::~StartupProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

void StartupProvider::refresh() {
    auto items = std::make_shared<std::vector<StartupItem>>();

    enumRegKey(HKEY_CURRENT_USER, L"HKCU", L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", *items);
    enumRegKey(HKEY_LOCAL_MACHINE, L"HKLM", L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", *items);
    enumRegKey(HKEY_CURRENT_USER, L"HKCU", L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run",
               *items);
    enumRegKey(HKEY_LOCAL_MACHINE, L"HKLM", L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer\\Run",
               *items);

    wchar_t* env = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&env, &len, L"APPDATA") == 0 && env) {
        enumFolder(std::wstring(env) + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup", "用户启动文件夹",
                   *items);
        free(env);
    }
    if (_wdupenv_s(&env, &len, L"PROGRAMDATA") == 0 && env) {
        enumFolder(std::wstring(env) + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup", "公共启动文件夹",
                   *items);
        free(env);
    }

    HTB_INFO("[startup] enumerated {} startup items", items->size());
    m_snapshot.store(std::move(items));
}

void StartupProvider::runOperation(const std::string& operation, const std::string& name,
                                   const std::function<std::pair<bool, std::string>()>& task) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, operation, name, task] {
        auto result = std::make_shared<StartupOperationResult>();
        result->operation = operation;
        result->name = name;
        auto outcome = task();
        result->success = outcome.first;
        result->message = outcome.second;
        m_lastOperation.store(result);
        if (result->success) {
            HTB_INFO("[startup] {} {} succeeded", operation, name);
        } else {
            HTB_ERROR("[startup] {} {} failed: {}", operation, name, result->message);
        }
        m_impl->opRunning.store(false);
    });
}

void StartupProvider::setEnabled(const StartupItem& item, bool enable) {
    const std::string op = enable ? "启用启动项" : "禁用启动项";
    runOperation(op, item.name, [item, enable] { return doSetEnabled(item, enable); });
}

void StartupProvider::removeItem(const StartupItem& item) {
    runOperation("删除启动项", item.name, [item] { return doRemove(item); });
}

void StartupProvider::openLocation(const StartupItem& item) {
    if (item.isRegistry) {
        const std::wstring path = extractCommandPath(toWide(item.command));
        if (path.empty()) return;
        const std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        return;
    }
    const std::wstring args = L"/select,\"" + toWide(item.filePath) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

} // namespace htb