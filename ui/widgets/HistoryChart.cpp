#include "ui/widgets/HistoryChart.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "imgui_internal.h"

namespace htb {

HistoryChart::HistoryChart(size_t capacity, double windowSeconds)
    : m_capacity(capacity), m_windowSeconds(windowSeconds) {}

void HistoryChart::sample(double value, double nowSeconds) {
    if (nowSeconds <= m_lastSampleTime) return;
    m_lastSampleTime = nowSeconds;
    m_points.emplace_back(nowSeconds, value);
    while (m_points.size() > m_capacity || (!m_points.empty() &&
                                            m_points.front().first < nowSeconds - m_windowSeconds)) {
        m_points.pop_front();
    }
}

void HistoryChart::clear() {
    m_points.clear();
    m_lastSampleTime = -1.0;
}

double HistoryChart::maxInWindow() const {
    double max = 0.0;
    for (const auto& [t, v] : m_points) max = std::max(max, v);
    return max;
}

double HistoryChart::lastValue() const {
    return m_points.empty() ? 0.0 : m_points.back().second;
}

void HistoryChart::draw(const char* label, float height, float yMax, bool percentMode,
                        const std::string& valueText, const std::string& peakText) {
    ImGui::BeginChild(label, ImVec2(0, height), ImGuiChildFlags_Borders);
    ImGui::PushFont(ImGui::GetIO().FontDefault);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const float usableY = size.y - ImGui::GetTextLineHeight() - 4.0f;

    if (!m_points.empty()) {
        const double yScale = yMax > 0.0 ? static_cast<double>(usableY) / yMax : 1.0;
        const double t0 = m_points.front().first;
        const double t1 = m_points.back().first;
        const double span = (t1 - t0) > 0.001 ? (t1 - t0) : 1.0;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 gridColor = IM_COL32(255, 255, 255, 18);
        for (int i = 1; i < 4; ++i) {
            const float y = pos.y + size.y - static_cast<float>(usableY * i / 4.0);
            draw->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), gridColor, 1.0f);
        }

        const ImU32 lineColor = IM_COL32(110, 180, 245, 230);
        const ImU32 fillColor = IM_COL32(110, 180, 245, 36);
        std::vector<ImVec2> pts;
        pts.reserve(m_points.size() + 2);
        for (const auto& [t, v] : m_points) {
            const float x = pos.x + static_cast<float>((t - t0) / span) * size.x;
            const float y = pos.y + size.y - static_cast<float>(v * yScale);
            pts.emplace_back(x, y);
        }
        if (pts.size() >= 2) {
            std::vector<ImVec2> fill = pts;
            fill.emplace_back(pos.x + size.x, pos.y + size.y);
            fill.emplace_back(pos.x, pos.y + size.y);
            draw->AddConvexPolyFilled(fill.data(), static_cast<int>(fill.size()), fillColor);
            draw->AddPolyline(pts.data(), static_cast<int>(pts.size()), lineColor, 0, 2.0f);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 6, pos.y + 4));
    ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.72f, 1.0f), "%s", label);
    if (!valueText.empty()) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
        ImGui::TextUnformatted(valueText.c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 6, pos.y + size.y - ImGui::GetTextLineHeight() - 2));
    if (!peakText.empty()) {
        ImGui::TextDisabled("%s", peakText.c_str());
    }
    if (percentMode) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 70.0f);
        ImGui::TextDisabled("100%%");
    }
    ImGui::PopFont();
    ImGui::EndChild();
}

} // namespace htb
