#include "ui/pages/CameraPage.h"

#include <string>

#include "hardware/camera/CameraProvider.h"

#include "imgui.h"

namespace htb {

void CameraPage::draw(UiContext& ctx) {
    ImGui::Text("摄像头");
    ImGui::Separator();
    ImGui::Spacing();

    auto cameras = ctx.service.camera().snapshot();
    if (!cameras) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("camera_body", ImVec2(0, 0));
    if (cameras->empty()) {
        ImGui::Text("未检测到摄像头");
    } else {
        if (ImGui::BeginTable("camera_table", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("符号链接", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (const CameraInfo& c : *cameras) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(c.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(c.symbolicLink.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("来源: %s", cameras->front().source.c_str());
        ImGui::TextWrapped("预览、分辨率与帧率控制、虚拟摄像头属于后续阶段 "
                           "(Media Foundation 视频管线 + D3D11 纹理)。");
    }
    ImGui::EndChild();
}

} // namespace htb
