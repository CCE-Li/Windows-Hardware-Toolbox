#include "ui/widgets/PreviewCanvas.h"

#include <algorithm>

#include "imgui_internal.h"

namespace htb {

namespace {
constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 3.0f;
}

void PreviewCanvas::draw(ID3D11ShaderResourceView* frame, float frameW, float frameH, float zoom, float panX,
                         float panY, const OnChangeFn& onChange) {
    (void)frameW;
    (void)frameH;
    const float canvasW = std::max(200.0f, ImGui::GetContentRegionAvail().x - 16.0f);
    const float canvasH = canvasW * 9.0f / 16.0f;
    ImGui::BeginChild("##preview_canvas", ImVec2(canvasW, canvasH), ImGuiChildFlags_Borders);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    m_canvasPos = pos;
    m_canvasSize = ImGui::GetContentRegionAvail();
    m_canvasSize.y = std::min(m_canvasSize.y, canvasH);

    m_frameSize = ImVec2(m_canvasSize.x * zoom, m_canvasSize.y * zoom);
    const float panOffsetX = (m_frameSize.x - m_canvasSize.x) * panX * 0.5f;
    const float panOffsetY = (m_frameSize.y - m_canvasSize.y) * panY * 0.5f;
    m_framePos = ImVec2(m_canvasPos.x + (m_canvasSize.x - m_frameSize.x) * 0.5f + panOffsetX,
                        m_canvasPos.y + (m_canvasSize.y - m_frameSize.y) * 0.5f + panOffsetY);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImGui::InvisibleButton("##canvas_interact", m_canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    if (ImGui::IsItemHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f) {
        const float newZoom = std::clamp(zoom * (ImGui::GetIO().MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f), kMinZoom, kMaxZoom);
        if (newZoom != zoom && onChange) {
            const float nfW = m_canvasSize.x * newZoom;
            const float nfH = m_canvasSize.y * newZoom;
            const float relX = (mouse.x - m_framePos.x) / m_frameSize.x;
            const float relY = (mouse.y - m_framePos.y) / m_frameSize.y;
            const float newFrameX = mouse.x - relX * nfW;
            const float newFrameY = mouse.y - relY * nfH;
            const float maxOffsetX = std::max(0.0f, (nfW - m_canvasSize.x) * 0.5f);
            const float maxOffsetY = std::max(0.0f, (nfH - m_canvasSize.y) * 0.5f);
            const float centerX = m_canvasPos.x + m_canvasSize.x * 0.5f;
            const float centerY = m_canvasPos.y + m_canvasSize.y * 0.5f;
            const float offX = (newFrameX + nfW * 0.5f) - centerX;
            const float offY = (newFrameY + nfH * 0.5f) - centerY;
            const float nPanX = maxOffsetX > 0.0f ? std::clamp(offX / maxOffsetX, -1.0f, 1.0f) : 0.0f;
            const float nPanY = maxOffsetY > 0.0f ? std::clamp(offY / maxOffsetY, -1.0f, 1.0f) : 0.0f;
            onChange(newZoom, nPanX, nPanY);
        }
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (onChange && (zoom != kMinZoom || panX != 0.0f || panY != 0.0f)) {
            onChange(kMinZoom, 0.0f, 0.0f);
        }
    }

    const bool pressed = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                         ImGui::IsMousePosValid(&mouse);
    if (pressed && !m_dragging) {
        m_dragging = true;
        m_grabOffset = ImVec2(mouse.x - m_framePos.x, mouse.y - m_framePos.y);
    }
    if (m_dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_dragging = false;
    }
    if (m_dragging && m_frameSize.x > m_canvasSize.x + 1.0f && onChange) {
        const float minX = m_canvasPos.x + m_canvasSize.x - m_frameSize.x;
        const float minY = m_canvasPos.y + m_canvasSize.y - m_frameSize.y;
        const float maxX = m_canvasPos.x;
        const float maxY = m_canvasPos.y;
        const float newX = std::clamp(mouse.x - m_grabOffset.x, minX, maxX);
        const float newY = std::clamp(mouse.y - m_grabOffset.y, minY, maxY);
        const float centerX = m_canvasPos.x + m_canvasSize.x * 0.5f;
        const float centerY = m_canvasPos.y + m_canvasSize.y * 0.5f;
        const float maxOffsetX = (m_frameSize.x - m_canvasSize.x) * 0.5f;
        const float maxOffsetY = (m_frameSize.y - m_canvasSize.y) * 0.5f;
        const float offX = (newX + m_frameSize.x * 0.5f) - centerX;
        const float offY = (newY + m_frameSize.y * 0.5f) - centerY;
        const float nPanX = maxOffsetX > 0.0f ? std::clamp(offX / maxOffsetX, -1.0f, 1.0f) : 0.0f;
        const float nPanY = maxOffsetY > 0.0f ? std::clamp(offY / maxOffsetY, -1.0f, 1.0f) : 0.0f;
        if (nPanX != panX || nPanY != panY) {
            onChange(zoom, nPanX, nPanY);
        }
    }

    if (frame) {
        draw->AddImage(reinterpret_cast<ImTextureID>(frame), m_framePos,
                       ImVec2(m_framePos.x + m_frameSize.x, m_framePos.y + m_frameSize.y));
    }

    draw->AddRect(m_canvasPos, ImVec2(m_canvasPos.x + m_canvasSize.x, m_canvasPos.y + m_canvasSize.y),
                  IM_COL32(255, 255, 255, 40), 0.0f, 0, 1.0f);

    ImGui::EndChild();
}

} // namespace htb
