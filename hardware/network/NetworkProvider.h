#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct NetworkAdapter {
    std::string name;
    std::string description;
    std::string mac;
    uint64_t linkSpeedBps = 0;
    std::string status;
    std::vector<std::string> addresses;
    std::vector<std::string> gateways;
    std::vector<std::string> dnsServers;
    std::string dnsSuffix;
    uint64_t rxBytes = 0;
    uint64_t txBytes = 0;
    double rxRateBps = 0.0;
    double txRateBps = 0.0;
    uint64_t inErrors = 0;
    uint64_t outErrors = 0;
    uint64_t mtu = 0;
    std::string pingStatus;
    Availability pingAvailability = Availability::Unavailable;
    std::string source;
};

class NetworkProvider final : public HardwareProvider {
public:
    NetworkProvider();
    ~NetworkProvider() override;

    std::string_view name() const override { return "network"; }
    void refresh() override;

    std::shared_ptr<const std::vector<NetworkAdapter>> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<NetworkAdapter>>> m_snapshot;
};

} // namespace htb
