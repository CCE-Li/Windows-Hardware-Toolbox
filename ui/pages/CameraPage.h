#pragma once

#include "ui/pages/Page.h"

namespace htb {

class CameraPage final : public IPage {
public:
    std::string_view title() const override { return "摄像头"; }
    void draw(UiContext& ctx) override;
};

} // namespace htb
