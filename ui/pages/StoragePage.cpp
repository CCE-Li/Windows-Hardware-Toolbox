#include "ui/pages/StoragePage.h"

#include <string>

#include "hardware/storage/StorageProvider.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {
ImVec4 healthColor(const StorageDisk& disk) {
    if (disk.healthAvailability != Availability::Available) return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    if (disk.healthStatus == "健康") return ImVec4(0.40f, 0.78f, 0.45f, 1.0f);
    if (disk.healthStatus == "警告") return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
    return ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
}

void cell(const char* text) {
    ImGui::TableNextColumn();
    if (text && *text) {
        ImGui::TextUnformatted(text);
    } else {
        ImGui::TextDisabled("-");
    }
}
} // namespace

void StoragePage::draw(UiContext& ctx) {
    ImGui::Text("存储");
    ImGui::Separator();
    ImGui::Spacing();

    auto disks = ctx.service.storage().snapshot();
    if (!disks) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("storage_body", ImVec2(0, 0));
    if (disks->empty()) {
        ImGui::Text("未检测到磁盘");
    } else {
        if (ImGui::BeginTable("disk_table", 9,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("接口", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("介质", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("容量", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("序列号", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("固件", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("健康", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("温度", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("启动盘", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();

            for (const StorageDisk& disk : *disks) {
                ImGui::TableNextRow();
                cell(disk.name.c_str());
                cell(disk.busType.empty() ? disk.interfaceType.c_str() : disk.busType.c_str());
                cell(disk.mediaType.empty() ? "-" : disk.mediaType.c_str());
                const std::string size = formatBytes(disk.sizeBytes);
                cell(size.c_str());
                cell(disk.serial.c_str());
                cell(disk.firmware.c_str());
                ImGui::TableNextColumn();
                if (disk.healthAvailability == Availability::Available) {
                    ImGui::TextColored(healthColor(disk), "%s", disk.healthStatus.c_str());
                } else {
                    ImGui::TextDisabled("%s", availabilityLabel(disk.healthAvailability).c_str());
                }
                ImGui::TableNextColumn();
                if (disk.temperatureC) {
                    ImGui::Text("%.0f C", *disk.temperatureC);
                } else {
                    ImGui::TextDisabled("-");
                }
                cell(disk.isBoot ? "是" : "-");
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("来源: %s", disks->front().source.c_str());
        ImGui::TextDisabled("健康状态与温度来自 Storage Management WMI (MSFT_PhysicalDisk)。");
    }
    ImGui::EndChild();
}

} // namespace htb
