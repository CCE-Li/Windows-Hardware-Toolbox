#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "monitoring/Metric.h"

namespace htb {

struct VirtualCameraStatus {
    bool inProgress = false;
    bool success = false;
    std::string operation;
    std::string message;
    std::string friendlyName;
};

class VirtualCameraController {
public:
    VirtualCameraController();
    ~VirtualCameraController();

    void create(const std::string& friendlyName);
    void remove();
    std::shared_ptr<const VirtualCameraStatus> lastStatus() const { return m_status.load(); }

    static std::string clsidString();

private:
    void run(const std::string& operation, const std::string& friendlyName);

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::atomic<std::shared_ptr<const VirtualCameraStatus>> m_status;
};

} // namespace htb
