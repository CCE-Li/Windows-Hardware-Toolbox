#pragma once

#include <string>

#include "hardware/startup/StartupProvider.h"
#include "ui/pages/Page.h"

namespace htb {

class StartupPage final : public IPage {
public:
    std::string_view title() const override { return "启动项"; }
    void draw(UiContext& ctx) override;

private:
    void dispatchPending(UiContext& ctx);

    char m_search[128] = {};
    StartupItem m_pendingItem;
    std::string m_pendingOp;
};

} // namespace htb