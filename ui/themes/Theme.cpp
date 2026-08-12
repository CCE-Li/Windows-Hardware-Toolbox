#include "ui/themes/Theme.h"

#include "imgui.h"

namespace htb {

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.92f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.52f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.082f, 0.094f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.085f, 0.092f, 0.106f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.12f, 0.95f);
    c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.13f, 0.14f, 0.17f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.19f, 0.20f, 0.24f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.26f, 0.31f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    c[ImGuiCol_Tab] = ImVec4(0.11f, 0.12f, 0.14f, 1.0f);
    c[ImGuiCol_TabActive] = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.24f, 0.29f, 1.0f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.0f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.42f, 0.65f, 0.90f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.42f, 0.65f, 0.90f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.0f);
}

} // namespace htb
