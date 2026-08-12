#pragma once

#include "ui/pages/Page.h"

namespace htb {

class GpuPage final : public IPage {
public:
    std::string_view title() const override { return "GPU"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
