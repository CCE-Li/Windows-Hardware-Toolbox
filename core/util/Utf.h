#pragma once

#include <string>
#include <string_view>

namespace htb {

std::string toUtf8(std::wstring_view w);
std::wstring toWide(std::string_view s);

} // namespace htb
