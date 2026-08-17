#include "ui/pages/ServicesPage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "hardware/systemservice/SystemServiceProvider.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
                std::tolower(static_cast<unsigned char>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

ImVec4 stateColor(const std::string& state) {
    if (state == "运行中") return ImVec4(0.40f, 0.78f, 0.45f, 1.0f);
    if (state == "已暂停") return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
    if (state == "已停止") return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
}

} // namespace

void ServicesPage::draw(UiContext& ctx) {
    ImGui::Text("服务");
    ImGui::Separator();
    ImGui::Spacing();

    const auto services = ctx.service.systemService().snapshot();
    if (!services) {
        ImGui::Text("正在采集...");
        return;
    }

    const auto op = ctx.service.serviceLastOperation();
    if (op && !op->operation.empty()) {
        if (op->success) {
            ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s %s 成功", op->operation.c_str(),
                               op->name.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s %s 失败: %s", op->operation.c_str(),
                               op->name.c_str(), op->message.c_str());
        }
    }

    static const char* filterLabels[] = {"全部", "运行中", "已停止", "自动启动"};
    int filterIdx = static_cast<int>(m_filter);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::Combo("状态筛选", &filterIdx, filterLabels, 4)) m_filter = static_cast<StateFilter>(filterIdx);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##svc_search", "搜索服务名称...", m_search, sizeof(m_search));

    std::vector<const ServiceInfo*> rows;
    rows.reserve(services->size());
    for (const ServiceInfo& s : *services) {
        const bool nameMatch = icontains(s.name, m_search) || icontains(s.displayName, m_search);
        if (!nameMatch) continue;
        switch (m_filter) {
            case StateFilter::Running:
                if (s.state != "运行中") continue;
                break;
            case StateFilter::Stopped:
                if (s.state != "已停止") continue;
                break;
            case StateFilter::Auto:
                if (s.startType != "自动") continue;
                break;
            default:
                break;
        }
        rows.push_back(&s);
    }

    ImGui::Spacing();
    const float panelH = (m_selected.empty()) ? 0.0f : 90.0f;
    const float tableH = ImGui::GetContentRegionAvail().y - panelH - ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginTable("svc_table", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY,
                          ImVec2(0, tableH))) {
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("显示名称", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("启动类型", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("路径", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableHeadersRow();

        for (const ServiceInfo* s : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const bool selected = (m_selected == s->name);
            ImGui::PushID(s->name.c_str());
            if (ImGui::Selectable(s->name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selected = s->name;
            }
            if (ImGui::BeginPopupContextItem()) {
                m_selected = s->name;
                if (s->state == "已停止") {
                    if (ImGui::MenuItem("启动")) {
                        m_pendingOp = "启动服务";
                        m_pendingName = s->name;
                    }
                } else {
                    if (ImGui::MenuItem("停止")) {
                        m_pendingOp = "停止服务";
                        m_pendingName = s->name;
                    }
                    if (ImGui::MenuItem("重启")) {
                        m_pendingOp = "重启服务";
                        m_pendingName = s->name;
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(s->displayName.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(stateColor(s->state), "%s", s->state.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(s->startType.c_str());
            ImGui::TableNextColumn();
            if (s->pid != 0) {
                ImGui::Text("%u", s->pid);
            } else {
                ImGui::TextDisabled("-");
            }
            ImGui::TableNextColumn();
            if (!s->binaryPath.empty()) {
                ImGui::TextUnformatted(s->binaryPath.c_str());
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }

    if (!m_selected.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        const ServiceInfo* sel = nullptr;
        for (const ServiceInfo& s : *services) {
            if (s.name == m_selected) {
                sel = &s;
                break;
            }
        }
        if (sel) {
            ImGui::TextWrapped("描述: %s", sel->description.empty() ? "-" : sel->description.c_str());
            ImGui::Spacing();
            if (sel->state == "已停止") {
                if (ImGui::Button("启动服务")) {
                    m_pendingOp = "启动服务";
                    m_pendingName = sel->name;
                }
            } else {
                if (ImGui::Button("停止服务")) {
                    m_pendingOp = "停止服务";
                    m_pendingName = sel->name;
                }
                ImGui::SameLine();
                if (ImGui::Button("重启服务")) {
                    m_pendingOp = "重启服务";
                    m_pendingName = sel->name;
                }
            }
        }
    }

    dispatchPending(ctx);
}

void ServicesPage::dispatchPending(UiContext& ctx) {
    if (m_pendingName.empty()) return;

    if (m_pendingOp == "启动服务") {
        ctx.service.startService(m_pendingName);
        m_pendingName.clear();
        return;
    }

    ImGui::OpenPopup("确认服务操作");
    if (ImGui::BeginPopupModal("确认服务操作", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("确定要对服务执行以下操作吗？");
        ImGui::TextWrapped("%s: %s", m_pendingOp.c_str(), m_pendingName.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("停止/重启服务可能影响依赖它的程序。");
        ImGui::Spacing();
        if (ImGui::Button("确定", ImVec2(80, 0))) {
            if (m_pendingOp == "停止服务") {
                ctx.service.stopService(m_pendingName);
            } else if (m_pendingOp == "重启服务") {
                ctx.service.restartService(m_pendingName);
            }
            m_pendingName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(80, 0))) {
            m_pendingName.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace htb