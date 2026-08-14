#pragma once

#include <functional>

#include "imgui.h"

struct ID3D11ShaderResourceView;

namespace htb {

class PreviewCanvas {
public:
    using OnChangeFn = std::function<void(float zoom, float panX, float panY)>;

    void draw(ID3D11ShaderResourceView* frame, float frameW, float frameH, float zoom, float panX, float panY,
              const OnChangeFn& onChange);

private:
    ImVec2 m_canvasPos{};
    ImVec2 m_canvasSize{};
    ImVec2 m_framePos{};
    ImVec2 m_frameSize{};
    bool m_dragging = false;
    ImVec2 m_grabOffset{};
    float m_lastZoom = 1.0f;
};

} // namespace htb
