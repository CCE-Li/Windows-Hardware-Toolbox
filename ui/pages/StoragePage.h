#pragma once

#include "ui/pages/Page.h"

namespace htb {

class StoragePage final : public IPage {
public:
    std::string_view title() const override { return "存储"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
