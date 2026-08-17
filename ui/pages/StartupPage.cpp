#include "ui/pages/StartupPage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

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

} // namespace

void StartupPage::draw(UiContext& ctx) {
    ImGui::Text("启动项");
    ImGui::Separator();
    ImGui::Spacing();

    const auto items = ctx.service.startup().snapshot();
    if (!items) {
        ImGui::Text("正在采集...");
        return;
    }

    int enabledCount = 0;
    int disabledCount = 0;
    for (const StartupItem& it : *items) {
        if (it.enabled) {
            ++enabledCount;
        } else {
            ++disabledCount;
        }
    }
    ImGui::TextDisabled("共 %zu 项 (已启用 %d / 已禁用 %d)", items->size(), enabledCount, disabledCount);
    ImGui::SameLine();
    ImGui::TextDisabled("来源: Registry / 启动文件夹");

    const auto op = ctx.service.startupLastOperation();
    if (op && !op->operation.empty()) {
        if (op->success) {
            ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "%s %s 成功", op->operation.c_str(),
                               op->name.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s %s 失败: %s", op->operation.c_str(),
                               op->name.c_str(), op->message.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(300.0f);
    ImGui::InputTextWithHint("##startup_search", "搜索启动项名称...", m_search, sizeof(m_search));
    ImGui::Spacing();

    std::vector<const StartupItem*> rows;
    for (const StartupItem& it : *items) {
        if (icontains(it.name, m_search) || icontains(it.command, m_search)) rows.push_back(&it);
    }

    const float tableH = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginTable("startup_table", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY,
                          ImVec2(0, tableH))) {
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("命令", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("来源", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableHeadersRow();

        for (const StartupItem* it : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(it->name.c_str());
            if (ImGui::BeginPopupContextItem()) {
                if (it->enabled) {
                    if (ImGui::MenuItem("禁用")) {
                        m_pendingItem = *it;
                        m_pendingOp = "toggle";
                    }
                } else {
                    if (ImGui::MenuItem("启用")) {
                        m_pendingItem = *it;
                        m_pendingOp = "toggle";
                    }
                }
                if (ImGui::MenuItem("打开位置")) ctx.service.openStartupItemLocation(*it);
                if (ImGui::MenuItem("删除")) {
                    m_pendingItem = *it;
                    m_pendingOp = "delete";
                }
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            if (!it->command.empty()) {
                ImGui::TextWrapped("%s", it->command.c_str());
            } else {
                ImGui::TextDisabled("-");
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(it->locationShort.c_str());
            ImGui::TableNextColumn();
            ImGui::TextColored(it->enabled ? ImVec4(0.40f, 0.78f, 0.45f, 1.0f)
                                           : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                               "%s", it->status.c_str());
            ImGui::TableNextColumn();
            if (it->sizeBytes > 0) {
                ImGui::Text("%s", formatBytes(it->sizeBytes).c_str());
            } else {
                ImGui::TextDisabled("-");
            }
            ImGui::TableNextColumn();
            ImGui::PushID(it->id.c_str());
            if (it->enabled) {
                if (ImGui::SmallButton("禁用")) {
                    m_pendingItem = *it;
                    m_pendingOp = "toggle";
                }
            } else {
                if (ImGui::SmallButton("启用")) {
                    m_pendingItem = *it;
                    m_pendingOp = "toggle";
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("打开位置")) ctx.service.openStartupItemLocation(*it);
            ImGui::SameLine();
            if (ImGui::SmallButton("删除")) {
                m_pendingItem = *it;
                m_pendingOp = "delete";
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    dispatchPending(ctx);
}

void StartupPage::dispatchPending(UiContext& ctx) {
    if (m_pendingOp.empty()) return;

    if (m_pendingOp == "toggle") {
        ctx.service.setStartupItemEnabled(m_pendingItem, !m_pendingItem.enabled);
        m_pendingOp.clear();
        return;
    }

    ImGui::OpenPopup("确认删除启动项");
    if (ImGui::BeginPopupModal("确认删除启动项", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("确定要删除启动项 %s 吗？此操作不可撤销。", m_pendingItem.name.c_str());
        ImGui::Spacing();
        if (ImGui::Button("确定", ImVec2(80, 0))) {
            ctx.service.removeStartupItem(m_pendingItem);
            m_pendingOp.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(80, 0))) {
            m_pendingOp.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace htb