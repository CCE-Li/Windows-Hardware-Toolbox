#include "ui/pages/StoragePage.h"

#include <string>

#include "core/util/Clipboard.h"
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
        if (ImGui::Button("打开磁盘管理")) ctx.service.launchSystemTool("diskmgmt");
        ImGui::SameLine();
        if (ImGui::Button("复制磁盘信息")) {
            std::string report;
            for (const StorageDisk& disk : *disks) {
                report += "磁盘: " + disk.name + "\n";
                report += "  接口: " + (disk.busType.empty() ? disk.interfaceType : disk.busType) + "\n";
                report += "  介质: " + disk.mediaType + "\n";
                report += "  容量: " + formatBytes(disk.sizeBytes) + "\n";
                report += "  序列号: " + disk.serial + "\n";
                report += "  固件: " + disk.firmware + "\n";
                if (disk.healthAvailability == Availability::Available) {
                    report += "  健康: " + disk.healthStatus + "\n";
                }
                if (disk.nvme.percentageUsed) {
                    report += "  磨损: " + std::to_string(*disk.nvme.percentageUsed) + "%\n";
                }
                report += "\n";
            }
            htb::copyToClipboard(report);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("共 %zu 块磁盘", disks->size());
        if (ImGui::BeginTable("disk_table", 11,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("接口", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("介质", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("容量", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("序列号", ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn("固件", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("健康", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("温度", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("磨损", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("通电时间", ImGuiTableColumnFlags_WidthFixed, 100.0f);
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
                } else if (disk.nvme.temperatureC) {
                    ImGui::Text("%.0f C", *disk.nvme.temperatureC);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableNextColumn();
                if (disk.nvme.percentageUsed) {
                    ImGui::Text("%u%%", *disk.nvme.percentageUsed);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableNextColumn();
                if (disk.nvme.powerOnHours) {
                    ImGui::Text("%llu h", static_cast<unsigned long long>(*disk.nvme.powerOnHours));
                } else {
                    ImGui::TextDisabled("-");
                }
                cell(disk.isBoot ? "是" : "-");
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("来源: %s", disks->front().source.c_str());
        if (disks->front().nvme.availability == Availability::Available) {
            ImGui::TextDisabled("NVMe 健康信息来源: %s", disks->front().nvme.source.c_str());
        }
        ImGui::TextDisabled("健康状态与温度来自 Storage Management WMI (MSFT_PhysicalDisk)。");
        ImGui::TextDisabled("磨损 / 通电时间 / 温度来自 NVMe 健康日志，需以管理员身份运行时可用。");
    }
    ImGui::EndChild();
}

} // namespace htb
