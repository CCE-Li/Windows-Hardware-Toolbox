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

struct PingTestResult {
    std::string target;
    int count = 0;
    int received = 0;
    double avgMs = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
    std::string status;
    bool inProgress = false;
};

struct DnsTestResult {
    std::string host;
    std::string status;
    std::vector<std::string> addresses;
    double elapsedMs = 0.0;
    bool inProgress = false;
};

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

    void runPingTest(const std::string& target, int count);
    void runDnsTest(const std::string& host);
    std::shared_ptr<const PingTestResult> pingResult() const { return m_pingResult.load(); }
    std::shared_ptr<const DnsTestResult> dnsResult() const { return m_dnsResult.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<NetworkAdapter>>> m_snapshot;
    std::atomic<std::shared_ptr<const PingTestResult>> m_pingResult;
    std::atomic<std::shared_ptr<const DnsTestResult>> m_dnsResult;
};

} // namespace htb
