#include "ui/pages/DisplayPage.h"

#include <string>
#include <vector>

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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("切换分辨率 / 刷新率");
    ImGui::Separator();

    static std::string selectedDevice;
    static int selectedMode = -1;
    const DisplayInfo* attached = nullptr;
    for (const DisplayInfo& d : *displays) {
        if (d.attached && (selectedDevice.empty() || selectedDevice == d.deviceName)) {
            selectedDevice = d.deviceName;
            attached = &d;
            break;
        }
    }

    if (!attached) {
        ImGui::Text("没有已连接的显示器");
    } else {
        ImGui::Text("显示器: %s", attached->deviceName.c_str());
        ImGui::SameLine();
        if (ImGui::Button("加载可用模式")) ctx.service.loadDisplayModesAsync(attached->deviceName);

        const auto modes = ctx.service.display().modesResult();
        if (modes && modes->deviceName == selectedDevice) {
            ImGui::SetNextItemWidth(320.0f);
            if (ImGui::BeginCombo("模式", "选择模式")) {
                for (size_t i = 0; i < modes->modes.size(); ++i) {
                    const DisplayMode& m = modes->modes[i];
                    const std::string label = std::to_string(m.width) + " x " + std::to_string(m.height) +
                                              " @ " + std::to_string(m.refreshHz) + " Hz";
                    if (ImGui::Selectable(label.c_str(), selectedMode == static_cast<int>(i))) {
                        selectedMode = static_cast<int>(i);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (selectedMode >= 0 && selectedMode < static_cast<int>(modes->modes.size())) {
                const DisplayMode& m = modes->modes[static_cast<size_t>(selectedMode)];
                if (ImGui::Button("应用")) {
                    ctx.service.applyDisplayModeAsync(selectedDevice, m.width, m.height, m.refreshHz);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("共 %zu 种模式", modes->modes.size());
        } else {
            ImGui::TextDisabled("点击“加载可用模式”以枚举该显示器的全部模式 (分辨率/刷新率组合)");
        }

        const auto apply = ctx.service.display().applyResult();
        if (apply && !apply->deviceName.empty()) {
            if (apply->success) {
                ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "已应用: %s", apply->message.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", apply->message.c_str());
            }
        }
        ImGui::TextDisabled("注意: 分辨率切换仅作用于当前会话，不写入注册表。");
    }
    ImGui::EndChild();
}

} // namespace htb
