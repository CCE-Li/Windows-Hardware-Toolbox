#pragma once

#include "ui/pages/Page.h"

namespace htb {

class CpuPage final : public IPage {
public:
    std::string_view title() const override { return "CPU"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
