#pragma once

#include "ui/pages/Page.h"

namespace htb {

class NetworkPage final : public IPage {
public:
    std::string_view title() const override { return "网络"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
