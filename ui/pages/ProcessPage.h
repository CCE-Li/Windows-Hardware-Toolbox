#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

#include "hardware/process/ProcessProvider.h"
#include "ui/pages/Page.h"
#include "ui/widgets/IconCache.h"

namespace htb {

class ProcessPage final : public IPage {
public:
    std::string_view title() const override { return "进程"; }
    void draw(UiContext& ctx) override;

private:
    enum class PendingOp { None, End, EndTree, Suspend, Resume, Restart, Priority };
    struct TreeNode {
        const ProcessInfo* info = nullptr;
        std::vector<TreeNode> children;
    };

    void drawContextMenu(UiContext& ctx, const ProcessInfo& proc);
    void dispatchPending(UiContext& ctx);
    void drawAffinityModal(UiContext& ctx);
    void drawDetails(UiContext& ctx);

    uint32_t m_selectedPid = 0;
    uint32_t m_inspectRequestedPid = 0;

    PendingOp m_pendingOp = PendingOp::None;
    uint32_t m_pendingPid = 0;
    std::string m_pendingName;
    std::string m_pendingOpName;
    ProcessPriority m_pendingPriority = ProcessPriority::Normal;

    bool m_affinityOpen = false;
    uint32_t m_affinityPid = 0;
    uint64_t m_affinityMask = 0;
    uint32_t m_affinityCores = 0;

    char m_search[128] = {};
    bool m_treeMode = false;
    std::unordered_set<uint32_t> m_expanded;
    IconCache m_icons;
};

} // namespace htb