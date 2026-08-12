#pragma once

#include "ui/pages/Page.h"

namespace htb {

class DashboardPage final : public IPage {
public:
    std::string_view title() const override { return "Dashboard"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
