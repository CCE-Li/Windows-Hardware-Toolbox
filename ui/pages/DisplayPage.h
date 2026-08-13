#pragma once

#include "ui/pages/Page.h"

namespace htb {

class DisplayPage final : public IPage {
public:
    std::string_view title() const override { return "显示"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
