#pragma once

#include <cstdint>
#include <string>

#include "hardware/process/ProcessProvider.h"
#include "ui/pages/Page.h"

namespace htb {

class ProcessPage final : public IPage {
public:
    std::string_view title() const override { return "进程"; }
    void draw(UiContext& ctx) override;

private:
    enum class PendingOp { None, End, EndTree, Suspend, Resume, Restart, Priority };

    void drawContextMenu(UiContext& ctx, const ProcessInfo& proc);
    void dispatchPending(UiContext& ctx);
    void drawAffinityModal(UiContext& ctx);
    void drawDetails(UiContext& ctx);

    uint32_t m_selectedPid = 0;

    PendingOp m_pendingOp = PendingOp::None;
    uint32_t m_pendingPid = 0;
    std::string m_pendingName;
    std::string m_pendingOpName;
    ProcessPriority m_pendingPriority = ProcessPriority::Normal;

    bool m_affinityOpen = false;
    uint32_t m_affinityPid = 0;
    uint64_t m_affinityMask = 0;
    uint32_t m_affinityCores = 0;
};

} // namespace htb