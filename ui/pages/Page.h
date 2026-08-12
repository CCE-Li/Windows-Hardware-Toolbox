#pragma once

#include <string_view>

#include "ui/UiContext.h"

namespace htb {

class IPage {
public:
    virtual ~IPage() = default;
    virtual std::string_view title() const = 0;
    virtual void draw(UiContext& ctx) = 0;
};

} // namespace htb
