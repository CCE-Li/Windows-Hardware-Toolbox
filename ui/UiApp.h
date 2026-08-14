#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "core/config/Config.h"
#include "ui/UiContext.h"
#include "ui/pages/Page.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace htb {

class UiApp {
public:
    UiApp(HardwareService& service, Config config, ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext);

    void frame();
    void setOnQuit(std::function<void()> onQuit) { m_onQuit = std::move(onQuit); }
    void setInitialPage(const std::string& title);

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
