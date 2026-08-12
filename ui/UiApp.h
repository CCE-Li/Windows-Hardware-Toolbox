#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "ui/UiContext.h"
#include "ui/pages/Page.h"

namespace htb {

class UiApp {
public:
    UiApp(HardwareService& service, Config config);

    void frame();
    void setOnQuit(std::function<void()> onQuit) { m_onQuit = std::move(onQuit); }

private:
    void drawSidebar();
    void drawStatusBar();

    HardwareService& m_service;
    Config m_config;
    UiContext m_ctx;
    std::vector<std::unique_ptr<IPage>> m_pages;
    int m_activePage = 0;
    std::function<void()> m_onQuit;
};

} // namespace htb
