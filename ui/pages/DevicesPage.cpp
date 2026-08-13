#include "ui/pages/DevicesPage.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <string>

#include "core/util/Clipboard.h"
#include "hardware/device/DeviceProvider.h"

#include "imgui.h"

namespace htb {

namespace {
struct Category {
    std::vector<size_t> indices;
};

std::string problemText(uint32_t problem) {
    if (problem == 0) return "工作正常";
    char buf[32];
    snprintf(buf, sizeof(buf), "问题代码 0x%X", problem);
    return buf;
}

std::string boolText(bool v) {
    return v ? "是" : "否";
}

void detailRow(const char* label, const std::string& value, bool emphasized = false) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    if (emphasized) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", value.c_str());
    } else {
        ImGui::TextWrapped("%s", value.c_str());
    }
}
} // namespace

void DevicesPage::draw(UiContext& ctx) {
    ImGui::Text("设备");
    ImGui::Separator();
    ImGui::Spacing();

    auto devices = ctx.service.device().snapshot();
    if (!devices) {
        ctx.service.requestDeviceRefresh();
        ImGui::Text("正在枚举设备...");
        return;
    }

    const auto last = ctx.service.device().lastEnumTime();
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - last).count();
    if (ImGui::Button("刷新")) ctx.service.requestDeviceRefresh();
    ImGui::SameLine();
    if (ImGui::Button("打开设备管理器")) ctx.service.launchSystemTool("devmgmt");
    ImGui::SameLine();
    ImGui::TextDisabled("%zu 台设备 | %s前枚举 | 来源: SetupAPI", devices->size(),
                        std::to_string(static_cast<int>(secs)).c_str());

    static std::string selectedInstance;
    int selected = -1;
    for (size_t i = 0; i < devices->size(); ++i) {
        if ((*devices)[i].instanceId == selectedInstance) {
            selected = static_cast<int>(i);
            break;
        }
    }

    std::map<std::string, Category> categories;
    for (size_t i = 0; i < devices->size(); ++i) {
        const std::string& cls = (*devices)[i].className;
        categories[cls.empty() ? "(未知)" : cls].indices.push_back(i);
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("dev_tree", ImVec2(340.0f, avail.y), ImGuiChildFlags_Borders);
    for (auto& [cls, cat] : categories) {
        const std::string label = cls + " (" + std::to_string(cat.indices.size()) + ")";
        if (ImGui::TreeNode(label.c_str())) {
            for (const size_t idx : cat.indices) {
                const DeviceInfo& d = (*devices)[idx];
                const std::string display = d.name.empty() ? d.instanceId : d.name;
                const bool isSelected = (selected == static_cast<int>(idx));
                if (ImGui::Selectable(display.c_str(), isSelected)) {
                    selectedInstance = d.instanceId;
                    selected = static_cast<int>(idx);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("dev_detail", ImVec2(0, avail.y), ImGuiChildFlags_Borders);
    if (selected >= 0 && selected < static_cast<int>(devices->size())) {
        const DeviceInfo& d = (*devices)[static_cast<size_t>(selected)];
        ImGui::TextColored(ImVec4(0.86f, 0.60f, 0.15f, 1.0f), "%s", d.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", d.className.c_str());

        std::string hwIdsJoined;
        for (const auto& id : d.hardwareIds) {
            hwIdsJoined += id;
            hwIdsJoined += "\n";
        }
        std::string compIdsJoined;
        for (const auto& id : d.compatibleIds) {
            compIdsJoined += id;
            compIdsJoined += "\n";
        }
        if (ImGui::Button("复制实例 ID")) htb::copyToClipboard(d.instanceId);
        ImGui::SameLine();
        if (ImGui::Button("复制硬件 ID")) htb::copyToClipboard(hwIdsJoined);
        ImGui::SameLine();
        if (ImGui::Button("复制兼容 ID")) htb::copyToClipboard(compIdsJoined);
        ImGui::SameLine();
        if (ImGui::Button("复制全部信息")) {
            std::string report = "设备: " + d.name + "\n类别: " + d.className + "\n制造商: " + d.manufacturer +
                                 "\n驱动版本: " + d.driverVersion + "\n实例 ID: " + d.instanceId + "\n\n硬件 ID:\n" +
                                 hwIdsJoined + "\n兼容 ID:\n" + compIdsJoined;
            htb::copyToClipboard(report);
        }
        ImGui::Separator();
        if (ImGui::BeginTable("dev_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("值");
            ImGui::TableHeadersRow();
            detailRow("类别", d.className);
            detailRow("类别 GUID", d.classGuid);
            detailRow("制造商", d.manufacturer);
            detailRow("总线枚举器", d.enumerator);
            detailRow("驱动服务", d.service);
            detailRow("驱动键", d.driverKey);
            detailRow("驱动版本", d.driverVersion);
            detailRow("驱动日期", d.driverDate);
            detailRow("驱动提供商", d.driverProvider);
            detailRow("总线号", std::to_string(d.busNumber));
            detailRow("状态", problemText(d.problem), d.problem != 0);
            detailRow("已启动", boolText(d.started));
            detailRow("已禁用", boolText(d.disabled));
            detailRow("实例 ID", d.instanceId);
            detailRow("硬件 ID", hwIdsJoined);
            detailRow("兼容 ID", compIdsJoined);
            detailRow("位置路径", d.locationPaths);
            ImGui::EndTable();
        }
    } else {
        ImGui::TextDisabled("请在左侧选择一台设备");
    }
    ImGui::EndChild();
}

} // namespace htb
