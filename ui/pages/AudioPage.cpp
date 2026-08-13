#include "ui/pages/AudioPage.h"

#include <cmath>
#include <string>

#include "hardware/audio/AudioProvider.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

void AudioPage::draw(UiContext& ctx) {
    ImGui::Text("音频");
    ImGui::Separator();
    ImGui::Spacing();

    auto endpoints = ctx.service.audio().snapshot();
    if (!endpoints) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("音量控制 (默认播放设备)");
    ImGui::Separator();
    const auto volume = ctx.service.audio().volume();
    if (!volume || volume->availability != Availability::Available) {
        ImGui::Text("系统音量 %s", availabilityLabel(volume ? volume->availability : Availability::Unavailable).c_str());
    } else {
        static float lastSet = -1.0f;
        float level = volume->level;
        if (ImGui::SliderFloat("音量", &level, 0.0f, 1.0f, "%.0f%%")) {
            if (lastSet < 0.0f || std::abs(level - lastSet) > 0.01f) {
                ctx.service.setVolumeAsync(level, false);
                lastSet = level;
            }
        }
        bool muted = volume->muted;
        if (ImGui::Checkbox("静音", &muted)) {
            ctx.service.setVolumeAsync(volume->level, muted);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("来源: %s", volume->source.c_str());
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("音频端点");
    ImGui::BeginChild("audio_body", ImVec2(0, 0));
    if (endpoints->empty()) {
        ImGui::Text("未检测到音频端点");
    } else {
        if (ImGui::BeginTable("audio_table", 8,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("方向", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("描述", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("默认", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("采样率", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("声道", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("格式", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();
            for (const AudioEndpoint& e : *endpoints) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(e.direction.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(e.name.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(e.description.c_str());
                ImGui::TableSetColumnIndex(3);
                if (e.state == "活动") {
                    ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s", e.state.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", e.state.c_str());
                }
                ImGui::TableSetColumnIndex(4);
                if (e.isDefault) {
                    ImGui::TextColored(ImVec4(0.86f, 0.60f, 0.15f, 1.0f), "是");
                } else {
                    ImGui::TextDisabled("-");
                }
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%u", e.sampleRate);
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%u", e.channels);
                ImGui::TableSetColumnIndex(7);
                ImGui::TextUnformatted(e.format.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("来源: %s", endpoints->front().source.c_str());
    }
    ImGui::EndChild();
}

} // namespace htb
