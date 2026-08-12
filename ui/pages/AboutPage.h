#pragma once

#include "ui/pages/Page.h"

namespace htb {

class AboutPage final : public IPage {
public:
    std::string_view title() const override { return "关于"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
