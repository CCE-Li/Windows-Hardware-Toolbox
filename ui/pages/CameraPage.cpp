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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("虚拟摄像头");
    ImGui::Separator();
    {
        static char nameBuf[128]{};
        if (nameBuf[0] == '\0') {
            snprintf(nameBuf, sizeof(nameBuf), "%s", "Hardware Toolbox 虚拟摄像头");
        }
        const bool elevated = ctx.service.isElevated();

        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("名称", nameBuf, sizeof(nameBuf));
        ImGui::SameLine();
        if (ImGui::Button("创建虚拟摄像头")) {
            ctx.service.createVirtualCamera(nameBuf);
        }
        ImGui::SameLine();
        if (ImGui::Button("移除虚拟摄像头")) {
            ctx.service.removeVirtualCamera();
        }
        if (!elevated) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                               "创建/移除虚拟摄像头需要管理员权限，请在“诊断”页以管理员身份重新启动。");
        }

        const auto status = ctx.service.virtualCamera().lastStatus();
        if (status && !status->operation.empty()) {
            if (status->inProgress) {
                ImGui::Text("正在%s...", status->operation.c_str());
            } else if (status->success) {
                ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s成功", status->operation.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s%s", status->operation.c_str(),
                                   status->message.c_str());
            }
        }
        ImGui::TextWrapped("虚拟摄像头输出 640x480 @ 30fps NV12 测试图案（彩条 + 帧计数），"
                           "创建后可在其他应用（相机、微信、Teams 等）中选择使用。");
        ImGui::TextDisabled("技术方案: Media Foundation Virtual Camera (Windows 11 22H2+)，用户态实现，无内核驱动。");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("已检测的摄像头");
    ImGui::Separator();

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
        ImGui::TextWrapped("预览、分辨率与帧率控制属于后续阶段 (Media Foundation 视频管线 + D3D11 纹理)。");
    }
    ImGui::EndChild();
}

} // namespace htb
