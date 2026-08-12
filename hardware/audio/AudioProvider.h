#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"

namespace htb {

struct AudioEndpoint {
    std::string name;
    std::string description;
    std::string id;
    std::string direction;
    std::string state;
    bool isDefault = false;
    uint32_t sampleRate = 0;
    uint32_t channels = 0;
    std::string format;
    std::string source;
};

class AudioProvider final : public HardwareProvider {
public:
    AudioProvider();
    ~AudioProvider() override;

    std::string_view name() const override { return "audio"; }
    void refresh() override;

    std::shared_ptr<const std::vector<AudioEndpoint>> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<AudioEndpoint>>> m_snapshot;
};

} // namespace htb
