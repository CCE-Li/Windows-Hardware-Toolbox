#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "core/config/Config.h"

namespace {

std::filesystem::path tempConfigPath(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "htb_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / name;
}

} // namespace

TEST(Config, CreatesDefaultFile) {
    const auto path = tempConfigPath("config_default.toml");
    std::filesystem::remove(path);
    const auto cfg = htb::Config::loadFrom(path);
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(cfg.ui.theme, "dark");
    EXPECT_EQ(cfg.ui.fps, 60);
    EXPECT_EQ(cfg.monitoring.interval_ms, 1000);
}

TEST(Config, RoundTrip) {
    const auto path = tempConfigPath("config_roundtrip.toml");
    std::filesystem::remove(path);
    auto cfg = htb::Config::loadFrom(path);
    cfg.ui.theme = "light";
    cfg.ui.fps = 120;
    cfg.monitoring.interval_ms = 500;
    cfg.save();

    const auto loaded = htb::Config::loadFrom(path);
    EXPECT_EQ(loaded.ui.theme, "light");
    EXPECT_EQ(loaded.ui.fps, 120);
    EXPECT_EQ(loaded.monitoring.interval_ms, 500);
}

TEST(Config, ClampsOutOfRangeValues) {
    const auto path = tempConfigPath("config_clamp.toml");
    {
        std::ofstream out(path);
        out << "[ui]\nfps = 9999\n[monitoring]\ninterval_ms = -5\n";
    }
    const auto cfg = htb::Config::loadFrom(path);
    EXPECT_EQ(cfg.ui.fps, 240);
    EXPECT_EQ(cfg.monitoring.interval_ms, 100);
}

TEST(Config, FallsBackOnInvalidToml) {
    const auto path = tempConfigPath("config_invalid.toml");
    {
        std::ofstream out(path);
        out << "this is = [ not valid toml\n";
    }
    const auto cfg = htb::Config::loadFrom(path);
    EXPECT_EQ(cfg.ui.theme, "dark");
    EXPECT_EQ(cfg.ui.fps, 60);
    EXPECT_EQ(cfg.monitoring.interval_ms, 1000);
}
