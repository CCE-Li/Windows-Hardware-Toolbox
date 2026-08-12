#pragma once

#include "ui/pages/Page.h"

namespace htb {

class DevicesPage final : public IPage {
public:
    std::string_view title() const override { return "Devices"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
