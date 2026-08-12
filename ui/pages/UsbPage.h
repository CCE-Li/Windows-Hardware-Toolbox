#pragma once

#include "ui/pages/Page.h"

namespace htb {

class UsbPage final : public IPage {
public:
    std::string_view title() const override { return "USB"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
