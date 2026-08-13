#include "ui/pages/DisplayPage.h"

#include <string>

#include "hardware/display/DisplayProvider.h"

#include "imgui.h"

namespace htb {

void DisplayPage::draw(UiContext& ctx) {
    ImGui::Text("显示");
    ImGui::Separator();
    ImGui::Spacing();

    auto displays = ctx.service.display().snapshot();
    if (!displays) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("display_body", ImVec2(0, 0));
    if (displays->empty()) {
        ImGui::Text("未检测到显示设备");
    } else {
        if (ImGui::BeginTable("display_table", 11,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("设备名", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("显卡", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("监视器", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("分辨率", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("刷新率", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("位深", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("方向", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("EDID 厂商", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("EDID 序列号", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("生产日期", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn("尺寸", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();
            for (const DisplayInfo& d : *displays) {
                ImGui::TableNextRow();
                int col = 0;
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.deviceName.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.gpuName.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.monitorName.c_str());
                ImGui::TableSetColumnIndex(col++);
                if (d.attached && d.width > 0) {
                    ImGui::Text("%u x %u", d.width, d.height);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableSetColumnIndex(col++);
                if (d.attached && d.refreshHz > 0) {
                    ImGui::Text("%u Hz", d.refreshHz);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableSetColumnIndex(col++);
                if (d.bitsPerPixel > 0) {
                    ImGui::Text("%u", d.bitsPerPixel);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.orientation.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.edidManufacturer.empty() ? "-" : d.edidManufacturer.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(d.edidSerial.empty() ? "-" : d.edidSerial.c_str());
                ImGui::TableSetColumnIndex(col++);
                if (d.edidYear > 0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u 年第 %u 周", d.edidYear, d.edidWeek);
                    ImGui::Text("%s", buf);
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableSetColumnIndex(col++);
                if (d.sizeInches > 0.0) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.1f 英寸", d.sizeInches);
                    ImGui::Text("%s", buf);
                } else {
                    ImGui::TextDisabled("-");
                }
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("来源: %s (EDID: 注册表)", displays->front().source.c_str());
    }
    ImGui::EndChild();
}

} // namespace htb
