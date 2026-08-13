#include "ui/pages/CameraPage.h"

#include <string>

#include "hardware/camera/CameraProvider.h"
#include "hardware/camera/VideoTransformPipeline.h"

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

        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("名称", nameBuf, sizeof(nameBuf));
        ImGui::SameLine();
        static bool pendingElevation = false;
        static std::string pendingOperation;
        if (ImGui::Button("创建虚拟摄像头")) {
            ctx.service.createVirtualCamera(nameBuf);
            if (!ctx.service.isElevated()) {
                pendingElevation = true;
                pendingOperation = "创建虚拟摄像头";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("移除虚拟摄像头")) {
            ctx.service.removeVirtualCamera();
            if (!ctx.service.isElevated()) {
                pendingElevation = true;
                pendingOperation = "移除虚拟摄像头";
            }
        }
        if (pendingElevation) {
            ctx.service.virtualCamera().pollPendingResult(pendingOperation);
            const auto pending = ctx.service.virtualCamera().lastStatus();
            if (pending && !pending->inProgress && !pending->operation.empty()) {
                pendingElevation = false;
            }
        }
        if (!ctx.service.isElevated()) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                               "点击后将弹出 UAC 提升提示，确认后在新窗口自动完成注册。");
        }

        const auto status = ctx.service.virtualCamera().lastStatus();
        if (status && !status->operation.empty()) {
            if (status->inProgress) {
                ImGui::Text("正在%s...", status->operation.c_str());
            } else if (status->success) {
                ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s成功", status->operation.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s: %s", status->operation.c_str(),
                                   status->message.c_str());
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("实时输出 (真实摄像头 -> 变换 -> 虚拟摄像头)");
    ImGui::Separator();
    {
        static CameraOutputParams params;
        static int selectedCamera = 0;
        const auto output = ctx.service.cameraOutputStatus();
        const bool outputRunning = output && output->running;

        if (cameras->empty()) {
            ImGui::Text("未检测到摄像头，无法启动实时输出");
        } else {
            ImGui::SetNextItemWidth(320.0f);
            if (ImGui::BeginCombo("摄像头", selectedCamera < static_cast<int>(cameras->size())
                                              ? cameras->at(static_cast<size_t>(selectedCamera)).name.c_str()
                                              : "-")) {
                for (size_t i = 0; i < cameras->size(); ++i) {
                    const std::string label = std::to_string(i) + ": " + cameras->at(i).name;
                    if (ImGui::Selectable(label.c_str(), selectedCamera == static_cast<int>(i))) {
                        selectedCamera = static_cast<int>(i);
                    }
                }
                ImGui::EndCombo();
            }

            params.cameraIndex = selectedCamera;
            if (ImGui::SliderFloat("缩放", &params.zoom, 1.0f, 3.0f, "%.2fx")) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::SliderFloat("水平平移", &params.panX, -1.0f, 1.0f, "%.2f")) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::SliderFloat("垂直平移", &params.panY, -1.0f, 1.0f, "%.2f")) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::Checkbox("水平翻转", &params.flipHorizontal)) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("垂直翻转", &params.flipVertical)) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            ImGui::SameLine();
            static bool rot180 = false;
            if (ImGui::Checkbox("旋转 180", &rot180)) {
                params.rotation = rot180 ? 180 : 0;
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::SliderInt("亮度", &params.brightness, -100, 100)) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::SliderInt("对比度", &params.contrast, -100, 100)) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }
            if (ImGui::SliderInt("饱和度", &params.saturation, -100, 100)) {
                if (outputRunning) ctx.service.updateCameraOutput(params);
            }

            if (outputRunning) {
                if (ImGui::Button("停止输出")) ctx.service.stopCameraOutput();
            } else {
                if (ImGui::Button("开始输出")) ctx.service.startCameraOutput(params);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("输出 1280x720 NV12 @ 源帧率");

            if (output) {
                if (output->running) {
                    ImGui::Text("状态: %s | 源 %ux%u | %.1f fps | 已发送 %llu 帧", output->message.c_str(),
                                output->sourceWidth, output->sourceHeight, output->fps,
                                static_cast<unsigned long long>(output->framesSent));
                } else if (!output->message.empty()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", output->message.c_str());
                }
            }
            ImGui::TextWrapped("提示: 先创建虚拟摄像头，再点击“开始输出”，然后在其他应用中选择该虚拟摄像头。"
                               "缩放/平移/颜色调整实时生效；无输出时虚拟摄像头显示测试图案。");
        }
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
    }
    ImGui::EndChild();
}

} // namespace htb
