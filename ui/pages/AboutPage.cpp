#include "ui/pages/AboutPage.h"

#include <cstdlib>
#include <string>

#include "core/config/Config.h"

#include "imgui.h"

namespace htb {

namespace {
std::string localAppData() {
    wchar_t* base = nullptr;
    size_t len = 0;
    _wdupenv_s(&base, &len, L"LOCALAPPDATA");
    std::string out = base ? "C:\\Users\\...\\AppData\\Local" : "-";
    free(base);
    return out;
}
} // namespace

void AboutPage::draw(UiContext& ctx) {
    (void)ctx;
    ImGui::Text("关于");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("about_body", ImVec2(0, 0));
    ImGui::TextColored(ImVec4(0.86f, 0.60f, 0.15f, 1.0f), "Windows Hardware Toolbox");
    ImGui::Text("版本: v" HTB_VERSION_STRING);
#ifdef _DEBUG
    ImGui::Text("构建: Debug");
#else
    ImGui::Text("构建: Release");
#endif
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("轻量、原生、快速的 Windows 硬件管理与诊断工具箱。"
                       "数据来源包括 Win32 API、WMI、PDH、DXGI、SetupAPI、IP Helper 等，"
                       "所有数据均标记来源与可用性。");
    ImGui::Spacing();
    if (ImGui::BeginTable("about_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("值");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("技术栈");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("C++20 / CMake / Win32 / COM / WMI / SetupAPI / PDH / DXGI / D3D11 / Dear ImGui / spdlog / TOML");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("日志文件");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s\\HardwareToolbox\\logs\\toolbox.log", localAppData().c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("配置文件");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s\\HardwareToolbox\\config.toml", localAppData().c_str());
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("F1: 打开 ImGui 演示窗口 | 侧栏底部“退出”或右上角 X 关闭");
    ImGui::EndChild();
}

} // namespace htb
