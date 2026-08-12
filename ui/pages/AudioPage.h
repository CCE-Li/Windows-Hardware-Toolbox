#pragma once

#include "ui/pages/Page.h"

namespace htb {

class AudioPage final : public IPage {
public:
    std::string_view title() const override { return "音频"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
