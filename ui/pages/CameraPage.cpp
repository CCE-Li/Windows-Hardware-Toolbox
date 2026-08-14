#include "ui/pages/CameraPage.h"

#include <algorithm>
#include <string>

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include "hardware/camera/CameraEngineController.h"
#include "hardware/camera/CameraProvider.h"
#include "hardware/camera/FrameShm.h"
#include "ui/widgets/PreviewCanvas.h"

#include "imgui.h"

namespace htb {

namespace {
struct PreviewState {
    HANDLE map = nullptr;
    uint8_t* view = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    uint32_t lastGen = 0;
};

ID3D11ShaderResourceView* updatePreviewTexture(UiContext& ctx, PreviewState& ps) {
    if (!ps.map) {
        ps.map = OpenFileMappingW(FILE_MAP_READ, FALSE, kPreviewShmName);
        if (ps.map) ps.view = static_cast<uint8_t*>(MapViewOfFile(ps.map, FILE_MAP_READ, 0, 0, 0));
    }
    if (!ps.map || !ps.view || !ctx.d3dDevice || !ctx.d3dContext) return nullptr;
    const auto* header = reinterpret_cast<const PreviewShmHeader*>(ps.view);
    if (header->magic != kPreviewShmMagic) return nullptr;
    if (ps.lastGen != header->generation) {
        ps.lastGen = header->generation;
        const uint8_t* bgr = ps.view + kPreviewShmHeaderSize;

        if (!ps.texture) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = kPreviewShmWidth;
            desc.Height = kPreviewShmHeight;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            ctx.d3dDevice->CreateTexture2D(&desc, nullptr, &ps.texture);
            ctx.d3dDevice->CreateShaderResourceView(ps.texture.Get(), nullptr, &ps.srv);
        }
        if (ps.texture && ps.srv) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(ctx.d3dContext->Map(ps.texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                const size_t w = kPreviewShmWidth;
                const size_t h = kPreviewShmHeight;
                for (size_t row = 0; row < h; ++row) {
                    const uint8_t* s = bgr + row * w * 3;
                    uint8_t* d = static_cast<uint8_t*>(mapped.pData) + row * mapped.RowPitch;
                    for (size_t col = 0; col < w; ++col) {
                        d[col * 4 + 0] = s[col * 3 + 0];
                        d[col * 4 + 1] = s[col * 3 + 1];
                        d[col * 4 + 2] = s[col * 3 + 2];
                        d[col * 4 + 3] = 255;
                    }
                }
                ctx.d3dContext->Unmap(ps.texture.Get(), 0);
            }
        }
    }
    return ps.srv.Get();
}
} // namespace

void CameraPage::draw(UiContext& ctx) {
    ImGui::Text("摄像头");
    ImGui::Separator();
    ImGui::Spacing();

    const auto pendingAction = ctx.service.pendingVcameraAction();
    if (pendingAction != HardwareService::VcameraAction::None) {
        const std::string name = ctx.service.pendingVcameraName();
        if (ctx.service.isElevated()) {
            if (pendingAction == HardwareService::VcameraAction::Register) {
                ctx.service.createVirtualCamera(name);
            } else {
                ctx.service.removeVirtualCamera();
            }
        }
        ctx.service.clearPendingVcameraAction();
    }

    ctx.service.pollPythonEngine();
    auto cameras = ctx.service.camera().snapshot();
    if (!cameras) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("实时输出 (Python 引擎 -> OBS Virtual Camera)");
    ImGui::Separator();
    {
        static CameraEngineParams params;
        static int selectedCamera = 0;
        const auto status = ctx.service.pythonEngineStatus();
        const bool running = status && status->running;

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
            ImGui::TextDisabled("输出: OBS Virtual Camera (1280x720, 无需管理员)");

            params.cameraIndex = selectedCamera;
            if (ImGui::SliderFloat("缩放", &params.zoom, 1.0f, 3.0f, "%.2fx")) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::SliderFloat("水平平移", &params.panX, -1.0f, 1.0f, "%.2f")) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::SliderFloat("垂直平移", &params.panY, -1.0f, 1.0f, "%.2f")) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::Checkbox("水平翻转", &params.flipHorizontal)) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("垂直翻转", &params.flipVertical)) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::SliderInt("亮度", &params.brightness, -100, 100)) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::SliderInt("对比度", &params.contrast, -100, 100)) {
                if (running) ctx.service.updatePythonEngine(params);
            }
            if (ImGui::SliderInt("饱和度", &params.saturation, -100, 100)) {
                if (running) ctx.service.updatePythonEngine(params);
            }

            if (running) {
                if (ImGui::Button("停止输出")) ctx.service.stopPythonEngine();
            } else {
                if (ImGui::Button("开始输出")) ctx.service.startPythonEngine(params);
            }

            if (status) {
                if (status->running) {
                    ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f),
                                       "运行中 | 源 %s | %.1f fps | 已发送 %llu 帧", status->source.c_str(),
                                       status->fps, static_cast<unsigned long long>(status->frames));
                } else if (!status->error.empty()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", status->error.c_str());
                }
            }
            ImGui::TextWrapped("提示: 引擎为独立 Python 进程 (OpenCV + pyvirtualcam)。"
                               "开始前请关闭微信/QQ 等占用摄像头的应用；输出目标为 OBS Virtual Camera，"
                               "在其他应用中即可选择。");

            ImGui::Spacing();
            ImGui::Text("预览 (拖拽平移 / 滚轮缩放 / 双击重置)");
            ImGui::Separator();
            static PreviewState previewState;
            static PreviewCanvas previewCanvas;
            ID3D11ShaderResourceView* frameSrv = updatePreviewTexture(ctx, previewState);
            if (!frameSrv) {
                ImGui::TextDisabled("预览不可用（引擎未运行或尚无数据）");
            } else {
                previewCanvas.draw(frameSrv, static_cast<float>(kPreviewShmWidth),
                                   static_cast<float>(kPreviewShmHeight), params.zoom, params.panX, params.panY,
                                   [&](float newZoom, float newPanX, float newPanY) {
                                       params.zoom = newZoom;
                                       params.panX = newPanX;
                                       params.panY = newPanY;
                                       if (running) ctx.service.updatePythonEngine(params);
                                   });
            }
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
