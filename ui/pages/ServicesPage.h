#pragma once

#include <string>

#include "ui/pages/Page.h"

namespace htb {

class ServicesPage final : public IPage {
public:
    std::string_view title() const override { return "服务"; }
    void draw(UiContext& ctx) override;

private:
    enum class StateFilter { All, Running, Stopped, Auto };

    void dispatchPending(UiContext& ctx);

    StateFilter m_filter = StateFilter::All;
    char m_search[128] = {};
    std::string m_selected;
    std::string m_pendingName;
    std::string m_pendingOp;
};

} // namespace htb