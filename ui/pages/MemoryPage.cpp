#include "ui/pages/MemoryPage.h"

#include <string>

#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {
void propRow(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value.c_str());
}
} // namespace

void MemoryPage::draw(UiContext& ctx) {
    ImGui::Text("内存");
    ImGui::Separator();
    ImGui::Spacing();

    auto mem = ctx.service.memory().snapshot();
    if (!mem) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("mem_body", ImVec2(0, 0));
    if (ImGui::BeginTable("mem_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("值");
        ImGui::TableHeadersRow();
        propRow("总容量", formatBytes(mem->totalBytes));
        propRow("已用", formatBytes(mem->usedBytes));
        propRow("可用", formatBytes(mem->availableBytes));
        propRow("负载", std::to_string(static_cast<int>(mem->loadPercent)) + "%");
        propRow("来源", mem->source);
        ImGui::EndTable();
    }
    ImGui::ProgressBar(mem->loadPercent / 100.0f, ImVec2(-1.0f, 0.0f));
    ImGui::Text("负载: %.1f%%", mem->loadPercent);

    ImGui::Spacing();
    ImGui::Text("内存条 (DIMM)");
    ImGui::Separator();

    if (mem->dimmAvailability != Availability::Available) {
        ImGui::TextWrapped("内存条信息 %s。来源: %s",
                           availabilityLabel(mem->dimmAvailability).c_str(), mem->dimmSource.c_str());
    } else if (mem->dimms.empty()) {
        ImGui::Text("WMI 未报告内存条信息");
    } else {
        if (ImGui::BeginTable("dimm_table", 6,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("插槽", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("制造商", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableSetupColumn("部件号", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("容量", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("类型", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("频率", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();
            for (const DimInfo& dim : mem->dimms) {
                ImGui::TableNextRow();
                int col = 0;
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(dim.deviceLocator.empty() ? "-" : dim.deviceLocator.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(dim.manufacturer.empty() ? "-" : dim.manufacturer.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(dim.partNumber.empty() ? "-" : dim.partNumber.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(formatBytes(dim.capacityBytes).c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(dim.memoryType.empty() ? "-" : dim.memoryType.c_str());
                ImGui::TableSetColumnIndex(col++);
                ImGui::TextUnformatted(dim.speed.empty() ? "-" : dim.speed.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("来源: %s", mem->dimmSource.c_str());
    }
    ImGui::EndChild();
}

} // namespace htb
