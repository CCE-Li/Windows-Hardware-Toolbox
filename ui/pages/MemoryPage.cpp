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
    ImGui::Text("Memory");
    ImGui::Separator();
    ImGui::Spacing();

    auto mem = ctx.service.memory().snapshot();
    if (!mem) {
        ImGui::Text("Collecting...");
        return;
    }

    ImGui::BeginChild("mem_body", ImVec2(0, 0));
    if (ImGui::BeginTable("mem_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        propRow("Total", formatBytes(mem->totalBytes));
        propRow("Used", formatBytes(mem->usedBytes));
        propRow("Available", formatBytes(mem->availableBytes));
        propRow("Load", std::to_string(static_cast<int>(mem->loadPercent)) + "%");
        propRow("Source", mem->source);
        ImGui::EndTable();
    }
    ImGui::ProgressBar(mem->loadPercent / 100.0f, ImVec2(-1.0f, 0.0f));
    ImGui::Text("Load: %.1f%%", mem->loadPercent);

    ImGui::Spacing();
    ImGui::Text("Modules (DIMM)");
    ImGui::Separator();

    if (mem->dimmAvailability != Availability::Available) {
        ImGui::TextWrapped("DIMM information %s. Source: %s",
                           availabilityLabel(mem->dimmAvailability).c_str(), mem->dimmSource.c_str());
    } else if (mem->dimms.empty()) {
        ImGui::Text("No memory modules reported by WMI");
    } else {
        if (ImGui::BeginTable("dimm_table", 6,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Manufacturer", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Part Number", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Capacity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthFixed, 100.0f);
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
        ImGui::TextDisabled("Source: %s", mem->dimmSource.c_str());
    }
    ImGui::EndChild();
}

} // namespace htb
