#pragma once

#include "ui/pages/Page.h"

namespace htb {

class MemoryPage final : public IPage {
public:
    std::string_view title() const override { return "Memory"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
