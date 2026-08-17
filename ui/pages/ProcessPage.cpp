#include "ui/pages/ProcessPage.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/util/Clipboard.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {

std::string formatRate(double bps) {
    if (bps < 1024.0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f B/s", bps);
        return buf;
    }
    return formatBytes(static_cast<uint64_t>(bps)) + "/s";
}

int cmpName(const std::string& a, const std::string& b) {
    const int r = _stricmp(a.c_str(), b.c_str());
    return r;
}

int cmpU32(uint32_t a, uint32_t b) {
    return (a > b) - (a < b);
}

int cmpF64(double a, double b) {
    return (a > b) - (a < b);
}

ImVec4 cpuColor(double pct) {
    if (pct >= 50.0) return ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
    if (pct >= 20.0) return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

std::string processReport(const ProcessInfo& p) {
    std::string report;
    report += "进程: " + p.name + " (PID " + std::to_string(p.pid) + ")\n";
    report += "CPU: " + std::to_string(p.cpuPercent) + "%\n";
    report += "内存(工作集): " + formatBytes(p.workingSetBytes) + "\n";
    report += "专用内存: " + formatBytes(p.privateBytes) + "\n";
    report += "提交大小: " + formatBytes(p.commitBytes) + "\n";
    report += "磁盘读取: " + formatRate(p.readRateBps) + "\n";
    report += "磁盘写入: " + formatRate(p.writeRateBps) + "\n";
    report += "线程: " + std::to_string(p.threads) + "\n";
    report += "句柄: " + std::to_string(p.handles) + "\n";
    report += "会话: " + std::to_string(p.sessionId) + "\n";
    report += "优先级: " + p.priority + "\n";
    report += "父进程 PID: " + std::to_string(p.ppid);
    return report;
}

} // namespace

void ProcessPage::draw(UiContext& ctx) {
    ImGui::Text("进程");
    ImGui::Separator();
    ImGui::Spacing();

    const auto snap = ctx.service.process().snapshot();
    if (!snap || snap->processes.empty()) {
        ImGui::Text("正在采集...");
        return;
    }

    uint64_t totalRam = 0;
    for (const ProcessInfo& p : snap->processes) totalRam += p.workingSetBytes;

    ImGui::TextDisabled("共 %zu 个进程", snap->processes.size());
    ImGui::SameLine();
    ImGui::TextDisabled("CPU 总占用: %.1f%%", snap->totalCpuPercent);
    ImGui::SameLine();
    ImGui::TextDisabled("内存: %s / %s", formatBytes(totalRam).c_str(),
                        formatBytes(snap->totalRamBytes).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("来源: %s", snap->source.c_str());
    ImGui::Spacing();

    const auto op = ctx.service.processLastOperation();
    if (op && !op->operation.empty()) {
        if (op->success) {
            ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s (PID %u) 成功", op->operation.c_str(),
                               op->pid);
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s (PID %u) 失败: %s", op->operation.c_str(),
                               op->pid, op->message.c_str());
        }
    }

    const float detailsH = (m_selectedPid != 0) ? 150.0f : 0.0f;
    const float tableH =
        std::max(50.0f, ImGui::GetContentRegionAvail().y - detailsH - ImGui::GetFrameHeightWithSpacing());

    if (ImGui::BeginTable("proc_table", 10,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                          ImVec2(0, tableH))) {
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("内存", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("专用内存", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("磁盘读取", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("磁盘写入", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("线程", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("会话", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableSetupColumn("优先级", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        std::vector<size_t> order(snap->processes.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;

        const ImGuiTableSortSpecs* sorts = ImGui::TableGetSortSpecs();
        if (sorts && sorts->SpecsCount > 0) {
            const ImGuiTableColumnSortSpecs& spec = sorts->Specs[0];
            const bool asc = (spec.SortDirection == ImGuiSortDirection_Ascending);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                const ProcessInfo& pa = snap->processes[a];
                const ProcessInfo& pb = snap->processes[b];
                int c = 0;
                switch (spec.ColumnIndex) {
                    case 0: c = cmpName(pa.name, pb.name); break;
                    case 1: c = cmpU32(pa.pid, pb.pid); break;
                    case 2: c = cmpF64(pa.cpuPercent, pb.cpuPercent); break;
                    case 3: c = cmpF64(static_cast<double>(pa.workingSetBytes), static_cast<double>(pb.workingSetBytes)); break;
                    case 4: c = cmpF64(static_cast<double>(pa.privateBytes), static_cast<double>(pb.privateBytes)); break;
                    case 5: c = cmpF64(pa.readRateBps, pb.readRateBps); break;
                    case 6: c = cmpF64(pa.writeRateBps, pb.writeRateBps); break;
                    case 7: c = cmpU32(pa.threads, pb.threads); break;
                    case 8: c = cmpU32(pa.sessionId, pb.sessionId); break;
                    case 9: c = cmpName(pa.priority, pb.priority); break;
                    default: break;
                }
                if (c == 0) c = cmpU32(pa.pid, pb.pid);
                return asc ? (c < 0) : (c > 0);
            });
        }

        for (const size_t idx : order) {
            const ProcessInfo& p = snap->processes[idx];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool selected = (p.pid == m_selectedPid);
            ImGui::PushID(static_cast<int>(p.pid));
            if (ImGui::Selectable(p.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedPid = p.pid;
            }
            if (ImGui::BeginPopupContextItem()) {
                m_selectedPid = p.pid;
                drawContextMenu(ctx, p);
                ImGui::EndPopup();
            }
            ImGui::PopID();

            ImGui::TableNextColumn();
            ImGui::Text("%u", p.pid);
            ImGui::TableNextColumn();
            ImGui::TextColored(cpuColor(p.cpuPercent), "%.1f%%", p.cpuPercent);
            ImGui::TableNextColumn();
            ImGui::Text("%s", formatBytes(p.workingSetBytes).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", formatBytes(p.privateBytes).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", formatRate(p.readRateBps).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s", formatRate(p.writeRateBps).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", p.threads);
            ImGui::TableNextColumn();
            ImGui::Text("%u", p.sessionId);
            ImGui::TableNextColumn();
            ImGui::Text("%s", p.priority.c_str());
        }
        ImGui::EndTable();
    }

    drawDetails(ctx);

    dispatchPending(ctx);
    drawAffinityModal(ctx);
}

void ProcessPage::drawDetails(UiContext& ctx) {
    if (m_selectedPid == 0) return;
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("获取详细信息")) ctx.service.inspectProcess(m_selectedPid);
    ImGui::SameLine();
    ImGui::TextDisabled("PID %u", m_selectedPid);

    const auto detail = ctx.service.processDetail();
    if (detail && detail->pid == m_selectedPid) {
        if (detail->status != "ok") {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", detail->status.c_str());
            return;
        }
        ImGui::TextWrapped("可执行文件: %s", detail->executablePath.empty() ? "-" : detail->executablePath.c_str());
        ImGui::TextWrapped("命令行: %s", detail->commandLine.empty() ? "-" : detail->commandLine.c_str());
        ImGui::Text("用户: %s", detail->userName.empty() ? "-" : detail->userName.c_str());
    } else {
        ImGui::TextDisabled("点击“获取详细信息”查看命令行 / 路径 / 用户");
    }
}

void ProcessPage::drawContextMenu(UiContext& ctx, const ProcessInfo& proc) {
    if (ImGui::MenuItem("结束任务")) {
        m_pendingOp = PendingOp::End;
        m_pendingOpName = "结束任务";
        m_pendingPid = proc.pid;
        m_pendingName = proc.name;
    }
    if (ImGui::MenuItem("结束进程树")) {
        m_pendingOp = PendingOp::EndTree;
        m_pendingOpName = "结束进程树";
        m_pendingPid = proc.pid;
        m_pendingName = proc.name;
    }
    if (ImGui::MenuItem("暂停")) {
        m_pendingOp = PendingOp::Suspend;
        m_pendingOpName = "暂停";
        m_pendingPid = proc.pid;
        m_pendingName = proc.name;
    }
    if (ImGui::MenuItem("恢复")) {
        m_pendingOp = PendingOp::Resume;
        m_pendingOpName = "恢复";
        m_pendingPid = proc.pid;
        m_pendingName = proc.name;
    }
    if (ImGui::MenuItem("重启进程")) {
        m_pendingOp = PendingOp::Restart;
        m_pendingOpName = "重启进程";
        m_pendingPid = proc.pid;
        m_pendingName = proc.name;
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("设置优先级")) {
        const auto prioItem = [&](const char* label, ProcessPriority p, const char* current) {
            if (ImGui::MenuItem(label, nullptr, proc.priority == current)) {
                m_pendingOp = PendingOp::Priority;
                m_pendingPriority = p;
                m_pendingPid = proc.pid;
                m_pendingName = proc.name;
            }
        };
        prioItem("空闲", ProcessPriority::Idle, "空闲");
        prioItem("低于正常", ProcessPriority::BelowNormal, "低于正常");
        prioItem("正常", ProcessPriority::Normal, "正常");
        prioItem("高于正常", ProcessPriority::AboveNormal, "高于正常");
        prioItem("高", ProcessPriority::High, "高");
        prioItem("实时", ProcessPriority::Realtime, "实时");
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("CPU 亲和性...")) {
        m_affinityOpen = true;
        m_affinityPid = proc.pid;
        m_affinityCores = ctx.service.process().snapshot() ? ctx.service.process().snapshot()->coreCount : 1;
        m_affinityMask = (m_affinityCores >= 64) ? ~0ull : ((1ull << m_affinityCores) - 1);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("获取详细信息")) {
        m_selectedPid = proc.pid;
        ctx.service.inspectProcess(proc.pid);
    }
    if (ImGui::MenuItem("复制进程信息")) htb::copyToClipboard(processReport(proc));
}

void ProcessPage::dispatchPending(UiContext& ctx) {
    if (m_pendingOp == PendingOp::None) return;

    if (m_pendingOp == PendingOp::Suspend || m_pendingOp == PendingOp::Resume ||
        m_pendingOp == PendingOp::Priority) {
        switch (m_pendingOp) {
            case PendingOp::Suspend: ctx.service.suspendProcess(m_pendingPid); break;
            case PendingOp::Resume: ctx.service.resumeProcess(m_pendingPid); break;
            case PendingOp::Priority: ctx.service.setProcessPriority(m_pendingPid, m_pendingPriority); break;
            default: break;
        }
        m_pendingOp = PendingOp::None;
        return;
    }

    ImGui::OpenPopup("确认操作");
    if (ImGui::BeginPopupModal("确认操作", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("确定要对进程执行以下操作吗？");
        ImGui::TextWrapped("%s: %s (PID %u)", m_pendingOpName.c_str(), m_pendingName.c_str(), m_pendingPid);
        ImGui::Spacing();
        ImGui::TextDisabled("结束/重启操作不可撤销。");
        ImGui::Spacing();
        if (ImGui::Button("确定", ImVec2(80, 0))) {
            switch (m_pendingOp) {
                case PendingOp::End: ctx.service.endProcess(m_pendingPid); break;
                case PendingOp::EndTree: ctx.service.endProcessTree(m_pendingPid); break;
                case PendingOp::Restart: ctx.service.restartProcess(m_pendingPid); break;
                default: break;
            }
            m_pendingOp = PendingOp::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(80, 0))) {
            m_pendingOp = PendingOp::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ProcessPage::drawAffinityModal(UiContext& ctx) {
    if (!m_affinityOpen) return;
    ImGui::OpenPopup("CPU 亲和性");
    ImGui::SetNextWindowSize(ImVec2(400, 0));
    if (ImGui::BeginPopupModal("CPU 亲和性", &m_affinityOpen,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("PID %u - 选择允许运行的 CPU 核心", m_affinityPid);
        ImGui::Spacing();
        constexpr int kPerRow = 8;
        for (uint32_t i = 0; i < m_affinityCores; ++i) {
            if (i % kPerRow != 0) ImGui::SameLine();
            char label[16];
            snprintf(label, sizeof(label), "CPU %u", i);
            bool on = (m_affinityMask & (1ull << i)) != 0;
            if (ImGui::Checkbox(label, &on)) {
                if (on) {
                    m_affinityMask |= (1ull << i);
                } else {
                    m_affinityMask &= ~(1ull << i);
                }
            }
        }
        ImGui::Spacing();
        if (ImGui::Button("全部")) m_affinityMask = (m_affinityCores >= 64) ? ~0ull : ((1ull << m_affinityCores) - 1);
        ImGui::SameLine();
        if (ImGui::Button("清空")) m_affinityMask = 0;
        ImGui::SameLine();
        if (ImGui::Button("确定", ImVec2(80, 0))) {
            if (m_affinityMask != 0) ctx.service.setProcessAffinity(m_affinityPid, m_affinityMask);
            m_affinityOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(80, 0))) {
            m_affinityOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace htb