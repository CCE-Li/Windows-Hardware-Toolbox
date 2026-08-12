#include "ui/UiApp.h"

#include <chrono>
#include <string>
#include <utility>

#include "ui/pages/AboutPage.h"
#include "ui/pages/CpuPage.h"
#include "ui/pages/DashboardPage.h"
#include "ui/pages/DevicesPage.h"
#include "ui/pages/DiagnosticsPage.h"
#include "ui/pages/GpuPage.h"
#include "ui/pages/MemoryPage.h"
#include "ui/pages/NetworkPage.h"
#include "ui/pages/SensorsPage.h"
#include "ui/pages/StoragePage.h"
#include "ui/pages/UsbPage.h"

#include "imgui.h"

namespace htb {

namespace {
template <typename T, typename... Args>
void addPage(std::vector<std::unique_ptr<IPage>>& pages, Args&&... args) {
    pages.push_back(std::make_unique<T>(std::forward<Args>(args)...));
}
} // namespace

UiApp::UiApp(HardwareService& service, Config config)
    : m_service(service), m_config(std::move(config)), m_ctx{service} {
    addPage<DashboardPage>(m_pages);
    addPage<CpuPage>(m_pages);
    addPage<GpuPage>(m_pages);
    addPage<MemoryPage>(m_pages);
    addPage<StoragePage>(m_pages);
    addPage<NetworkPage>(m_pages);
    addPage<UsbPage>(m_pages);
    addPage<DevicesPage>(m_pages);
    addPage<DiagnosticsPage>(m_pages);
    addPage<SensorsPage>(m_pages);
    addPage<AboutPage>(m_pages);
}

void UiApp::frame() {
    static bool showDemo = false;
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) showDemo = !showDemo;
    if (showDemo) ImGui::ShowDemoWindow(&showDemo);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::Begin("##toolbox_main", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNav);
    {
        const float statusH = ImGui::GetFrameHeightWithSpacing();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float sideW = 180.0f;

        ImGui::BeginChild("sidebar", ImVec2(sideW, avail.y - statusH), ImGuiChildFlags_Borders);
        drawSidebar();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("content", ImVec2(avail.x - sideW - ImGui::GetStyle().ItemSpacing.x, avail.y - statusH),
                          ImGuiChildFlags_Borders);
        if (m_activePage >= 0 && m_activePage < static_cast<int>(m_pages.size()))
            m_pages[static_cast<size_t>(m_activePage)]->draw(m_ctx);
        ImGui::EndChild();

        drawStatusBar();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void UiApp::drawSidebar() {
    ImGui::TextColored(ImVec4(0.86f, 0.60f, 0.15f, 1.0f), "硬件工具箱");
    ImGui::TextDisabled("v" HTB_VERSION_STRING);
    ImGui::Separator();
    ImGui::Spacing();

    for (size_t i = 0; i < m_pages.size(); ++i) {
        const bool selected = (m_activePage == static_cast<int>(i));
        const std::string title(m_pages[i]->title());
        if (ImGui::Selectable(title.c_str(), selected)) m_activePage = static_cast<int>(i);
    }

    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Selectable("退出")) {
        if (m_onQuit) m_onQuit();
    }
}

void UiApp::drawStatusBar() {
    ImGui::Separator();
    ImGui::TextDisabled("FPS: %.0f", ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::TextDisabled("刷新间隔: %d ms", m_service.intervalMs());
    ImGui::SameLine();
    const auto last = m_service.lastRefresh();
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - last).count();
    ImGui::TextDisabled("最近刷新: %.1fs 前", secs);
    if (m_activePage >= 0 && m_activePage < static_cast<int>(m_pages.size())) {
        const std::string title(m_pages[static_cast<size_t>(m_activePage)]->title());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", title.c_str());
    }
}

} // namespace htb
