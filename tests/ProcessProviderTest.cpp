#include <gtest/gtest.h>

#include "hardware/process/ProcessProvider.h"

TEST(ProcessProvider, PriorityDisplayNames) {
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::Idle), "空闲");
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::BelowNormal), "低于正常");
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::Normal), "正常");
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::AboveNormal), "高于正常");
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::High), "高");
    EXPECT_EQ(htb::processPriorityDisplayName(htb::ProcessPriority::Realtime), "实时");
}

TEST(ProcessProvider, CpuPercent) {
    EXPECT_EQ(htb::computeProcessCpuPercent(0, 0), 0.0);
    EXPECT_EQ(htb::computeProcessCpuPercent(10, 0), 0.0);
    EXPECT_DOUBLE_EQ(htb::computeProcessCpuPercent(0, 1000), 0.0);
    EXPECT_NEAR(htb::computeProcessCpuPercent(100, 1000), 10.0, 1e-9);
    EXPECT_NEAR(htb::computeProcessCpuPercent(500, 1000), 50.0, 1e-9);
    EXPECT_NEAR(htb::computeProcessCpuPercent(250, 1000), 25.0, 1e-9);
}

TEST(ProcessProvider, CpuPercentClamped) {
    EXPECT_LE(htb::computeProcessCpuPercent(1500, 1000), 100.0);
    EXPECT_EQ(htb::computeProcessCpuPercent(1500, 1000), 100.0);
    EXPECT_GE(htb::computeProcessCpuPercent(1, 1000), 0.0);
}

TEST(ProcessProvider, CpuPercentSums) {
    double sum = 0.0;
    sum += htb::computeProcessCpuPercent(200, 1000);
    sum += htb::computeProcessCpuPercent(300, 1000);
    sum += htb::computeProcessCpuPercent(400, 1000);
    EXPECT_NEAR(sum, 90.0, 1e-9);
}