#include <gtest/gtest.h>

#include "core/util/Utf.h"

TEST(Utf, Ascii) {
    EXPECT_EQ(htb::toUtf8(L"Hello, world!"), "Hello, world!");
    EXPECT_EQ(htb::toWide("Hello"), L"Hello");
}

TEST(Utf, Empty) {
    EXPECT_TRUE(htb::toUtf8(L"").empty());
    EXPECT_TRUE(htb::toWide("").empty());
}

TEST(Utf, NonAsciiRoundTrip) {
    const std::wstring w = L"\u4F60\u597D \u4E16\u754C";
    const std::string s = htb::toUtf8(w);
    EXPECT_NE(s, "?");
    EXPECT_EQ(htb::toWide(s), w);
}
