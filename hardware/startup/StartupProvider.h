#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hardware/HardwareProvider.h"

namespace htb {

struct StartupItem {
    std::string id;             // unique key used by operations
    std::string name;
    std::string command;
    std::string location;       // 完整来源描述
    std::string locationShort;  // 简短来源 (用于表格列)
    bool enabled = true;
    std::string status;         // 已启用 / 已禁用
    bool isRegistry = true;
    std::string regRoot;        // HKCU / HKLM
    std::string regPath;
    std::string valueName;
    std::string filePath;
    uint64_t sizeBytes = 0;
    std::string source = "Registry / 启动文件夹";
};

struct StartupOperationResult {
    std::string operation;
    std::string name;
    bool success = false;
    std::string message;
};

class StartupProvider final : public HardwareProvider {
public:
    StartupProvider();
    ~StartupProvider() override;

    std::string_view name() const override { return "startup"; }
    void refresh() override;

    std::shared_ptr<const std::vector<StartupItem>> snapshot() const { return m_snapshot.load(); }

    void setEnabled(const StartupItem& item, bool enable);
    void removeItem(const StartupItem& item);
    void openLocation(const StartupItem& item);
    std::shared_ptr<const StartupOperationResult> lastOperation() const { return m_lastOperation.load(); }

private:
    void runOperation(const std::string& operation, const std::string& name,
                      const std::function<std::pair<bool, std::string>()>& task);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<StartupItem>>> m_snapshot;
    std::atomic<std::shared_ptr<const StartupOperationResult>> m_lastOperation;
};

} // namespace htb