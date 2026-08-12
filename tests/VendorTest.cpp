#include <gtest/gtest.h>

#include "hardware/HardwareTypes.h"

TEST(Vendor, PciIds) {
    EXPECT_EQ(htb::vendorFromPciId(0x8086), htb::Vendor::Intel);
    EXPECT_EQ(htb::vendorFromPciId(0x10DE), htb::Vendor::Nvidia);
    EXPECT_EQ(htb::vendorFromPciId(0x1002), htb::Vendor::Amd);
    EXPECT_EQ(htb::vendorFromPciId(0x1022), htb::Vendor::Amd);
    EXPECT_EQ(htb::vendorFromPciId(0xFFFF), htb::Vendor::Unknown);
}

TEST(Vendor, Names) {
    EXPECT_EQ(htb::vendorName(htb::Vendor::Nvidia), "NVIDIA");
    EXPECT_EQ(htb::vendorName(htb::Vendor::Intel), "Intel");
    EXPECT_EQ(htb::vendorName(htb::Vendor::Unknown), "Unknown");
}
