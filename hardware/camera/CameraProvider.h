#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "hardware/HardwareProvider.h"
#include "monitoring/Metric.h"

namespace htb {

struct CameraInfo {
    std::string name;
    std::string symbolicLink;
    std::string source;
    Availability availability = Availability::Available;
};

class CameraProvider final : public HardwareProvider {
public:
    CameraProvider();
    ~CameraProvider() override;

    std::string_view name() const override { return "camera"; }
    void refresh() override;

    std::shared_ptr<const std::vector<CameraInfo>> snapshot() const { return m_snapshot.load(); }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const std::vector<CameraInfo>>> m_snapshot;
};

} // namespace htb
