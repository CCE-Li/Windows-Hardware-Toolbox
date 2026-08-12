#include "ui/pages/DiagnosticsPage.h"

#include <cstdio>
#include <string>

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

    size_t total = devices->size();
    size_t problems = 0;
    size_t disabled = 0;
    for (const DeviceInfo& d : *devices) {
        if (d.problem != 0) ++problems;
        if (d.disabled) ++disabled;
    }

    ImGui::BeginChild("diag_body", ImVec2(0, 0));
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

    auto disks = ctx.service.storage().snapshot();
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
    ImGui::EndChild();
}

} // namespace htb
