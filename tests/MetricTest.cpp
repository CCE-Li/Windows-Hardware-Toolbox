#include <gtest/gtest.h>

#include "monitoring/Metric.h"

TEST(Metric, AvailabilityLabels) {
    EXPECT_EQ(htb::toString(htb::Availability::Available), "Available");
    EXPECT_EQ(htb::toString(htb::Availability::Unavailable), "N/A");
    EXPECT_EQ(htb::toString(htb::Availability::Unsupported), "Unsupported");
    EXPECT_EQ(htb::toString(htb::Availability::PermissionDenied), "Permission denied");
}

TEST(Metric, Defaults) {
    const htb::Metric m;
    EXPECT_EQ(m.availability, htb::Availability::Unavailable);
    EXPECT_EQ(m.value, 0.0);
    EXPECT_TRUE(m.name.empty());
    EXPECT_TRUE(m.source.empty());
}
