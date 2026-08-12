#pragma once

#include "ui/pages/Page.h"

namespace htb {

class DiagnosticsPage final : public IPage {
public:
    std::string_view title() const override { return "诊断"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
