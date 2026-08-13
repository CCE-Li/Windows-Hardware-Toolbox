#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

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

struct VolumeState {
    Availability availability = Availability::Unavailable;
    std::string source = "IAudioEndpointVolume";
    float level = 0.0f;
    bool muted = false;
};

class AudioProvider final : public HardwareProvider {
public:
    AudioProvider();
    ~AudioProvider() override;

    std::string_view name() const override { return "audio"; }
    void refresh() override;

    std::shared_ptr<const std::vector<AudioEndpoint>> snapshot() const { return m_snapshot.load(); }
    std::shared_ptr<const VolumeState> volume() const { return m_volume.load(); }
    void setVolumeAsync(float level, bool muted);

private:
    void refreshVolume();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<AudioEndpoint>>> m_snapshot;
    std::atomic<std::shared_ptr<const VolumeState>> m_volume;
};

} // namespace htb
