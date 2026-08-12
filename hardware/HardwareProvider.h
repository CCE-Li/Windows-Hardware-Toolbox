#pragma once

#include <string_view>

namespace htb {

class HardwareProvider {
public:
    virtual ~HardwareProvider() = default;

    virtual std::string_view name() const = 0;
    virtual void refresh() = 0;
};

} // namespace htb
