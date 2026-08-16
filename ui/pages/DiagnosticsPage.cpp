#include "ui/pages/DiagnosticsPage.h"

#include <cstdio>
#include <string>

#include "core/util/Clipboard.h"
#include "hardware/device/DeviceProvider.h"
#include "hardware/storage/StorageProvider.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

void DiagnosticsPage::draw(UiContext& ctx) {
    ImGui::Text("诊断");
    ImGui::Separator();
    ImGui::Spacing();

    auto devices = ctx.service.device().snapshot();
    if (!devices) {
        ctx.service.requestDeviceRefresh();
        ImGui::Text("正在收集设备信息...");
        return;
    }
    auto disks = ctx.service.storage().snapshot();

    size_t total = devices->size();
    size_t problems = 0;
    size_t disabled = 0;
    for (const DeviceInfo& d : *devices) {
        if (d.problem != 0) ++problems;
        if (d.disabled) ++disabled;
    }

    ImGui::BeginChild("diag_body", ImVec2(0, 0));
    if (ImGui::Button("复制诊断报告")) {
        std::string report = "设备总数: " + std::to_string(total) + "\n";
        report += "问题设备: " + std::to_string(problems) + "\n";
        report += "禁用设备: " + std::to_string(disabled) + "\n";
        if (disks) {
            for (const StorageDisk& disk : *disks) {
                report += "磁盘 " + disk.name + ": ";
                report += disk.healthAvailability == Availability::Available ? disk.healthStatus : "N/A";
                report += "\n";
            }
        }
        if (problems > 0) {
            report += "\n问题设备列表:\n";
            for (const DeviceInfo& d : *devices) {
                if (d.problem != 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "0x%X", d.problem);
                    report += "  " + d.name + " [" + d.className + "] " + buf + " " + d.instanceId + "\n";
                }
            }
        }
        htb::copyToClipboard(report);
    }
    if (ImGui::BeginTable("diag_summary", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("指标", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("值");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("设备总数");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu", total);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("存在问题的设备");
        ImGui::TableSetColumnIndex(1);
        if (problems > 0) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%zu", problems);
        } else {
            ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "无");
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("已禁用的设备");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu", disabled);
        ImGui::EndTable();
    }

    if (disks) {
        ImGui::Spacing();
        ImGui::Text("磁盘健康");
        ImGui::Separator();
        if (disks->empty()) {
            ImGui::Text("未检测到磁盘");
        } else {
            for (const StorageDisk& disk : *disks) {
                const char* name = disk.name.empty() ? "-" : disk.name.c_str();
                if (disk.healthAvailability != Availability::Available) {
                    ImGui::BulletText("%s: %s", name, availabilityLabel(disk.healthAvailability).c_str());
                } else if (disk.healthStatus == "健康") {
                    ImGui::BulletText("%s: 健康", name);
                } else if (disk.healthStatus == "警告") {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.30f, 1.0f), "%s: 警告", name);
                } else {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s: %s", name,
                                       disk.healthStatus.c_str());
                }
            }
        }
    }

    if (problems > 0) {
        ImGui::Spacing();
        ImGui::Text("需要关注的设备");
        ImGui::Separator();
        if (ImGui::BeginTable("diag_problems", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("类别", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("问题", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("实例 ID", ImGuiTableColumnFlags_WidthFixed, 380.0f);
            ImGui::TableHeadersRow();
            for (const DeviceInfo& d : *devices) {
                if (d.problem == 0) continue;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(d.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(d.className.c_str());
                ImGui::TableSetColumnIndex(2);
                char buf[32];
                snprintf(buf, sizeof(buf), "0x%X", d.problem);
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", buf);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(d.instanceId.c_str());
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "所有设备工作正常");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("系统工具");
    ImGui::Separator();
    if (ImGui::Button("设备管理器")) ctx.service.launchSystemTool("devmgmt");
    ImGui::SameLine();
    if (ImGui::Button("磁盘管理")) ctx.service.launchSystemTool("diskmgmt");
    ImGui::SameLine();
    if (ImGui::Button("系统信息")) ctx.service.launchSystemTool("msinfo32");
    ImGui::SameLine();
    if (ImGui::Button("网络连接")) ctx.service.launchSystemTool("ncpa");
    ImGui::SameLine();
    if (ImGui::Button("任务管理器")) ctx.service.launchSystemTool("taskmgr");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("启动设置");
    ImGui::Separator();
    if (ctx.service.autoStartEnabled()) {
        if (ImGui::Button("取消开机自启")) ctx.service.setAutoStart(false);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "已启用开机自启（最小化启动）");
    } else {
        if (ImGui::Button("开启开机自启（最小化启动）")) ctx.service.setAutoStart(true);
        ImGui::SameLine();
        ImGui::TextDisabled("注册表 HKCU Run，无需管理员");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("设备操作权限");
    ImGui::Separator();
    if (ctx.service.isElevated()) {
        ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "当前以管理员权限运行，可在“设备”页启用/禁用/卸载设备。");
    } else {
        ImGui::TextWrapped("启用/禁用/卸载设备等系统级操作需要管理员权限。"
                           "点击下方按钮以管理员身份重新启动本程序 (UAC 提示)。");
        if (ImGui::Button("以管理员身份重新启动")) ctx.service.relaunchAsAdmin();
    }
    ImGui::TextDisabled("所有设备操作均记录到日志，卸载操作需在弹窗中确认。");

    ImGui::EndChild();
}

} // namespace htb
