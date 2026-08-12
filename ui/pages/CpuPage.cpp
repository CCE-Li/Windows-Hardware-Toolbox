#include "ui/pages/CpuPage.h"

#include <cstdio>
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

void CpuPage::draw(UiContext& ctx) {
    ImGui::Text("CPU");
    ImGui::Separator();
    ImGui::Spacing();

    auto cpu = ctx.service.cpu().snapshot();
    if (!cpu) {
        ImGui::Text("Collecting...");
        return;
    }

    ImGui::BeginChild("cpu_body", ImVec2(0, 0));
    if (ImGui::BeginTable("cpu_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        propRow("Name", cpu->name);
        propRow("Vendor", cpu->vendor);
        propRow("Architecture", cpu->architecture);
        propRow("Physical cores", std::to_string(cpu->physicalCores));
        propRow("Logical cores", std::to_string(cpu->logicalCores));
        propRow("Base frequency", cpu->baseFrequencyMHz
                                       ? formatMhz(*cpu->baseFrequencyMHz)
                                       : std::string(availabilityLabel(Availability::Unsupported)));
        propRow("Static source", cpu->staticSource);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Text("Usage");
    ImGui::Separator();

    if (cpu->usageAvailability != Availability::Available) {
        ImGui::TextWrapped("Usage data %s. Source: %s",
                           availabilityLabel(cpu->usageAvailability).c_str(),
                           cpu->usageSource.empty() ? "-" : cpu->usageSource.c_str());
    } else {
        ImGui::Text("Total");
        ImGui::ProgressBar(cpu->totalUsage / 100.0f, ImVec2(-1.0f, 0.0f), nullptr);
        ImGui::Text("Total: %.1f%%   (source: %s)", cpu->totalUsage, cpu->usageSource.c_str());
        ImGui::Spacing();
        ImGui::Text("Per logical core");
        const int perRow = 4;
        for (size_t i = 0; i < cpu->perCoreUsage.size(); ++i) {
            if (i > 0 && i % perRow != 0) ImGui::SameLine();
            const float w = (ImGui::GetContentRegionAvail().x -
                             ImGui::GetStyle().ItemSpacing.x * (perRow - 1)) /
                            perRow;
            char label[32];
            snprintf(label, sizeof(label), "Core %zu", i);
            ImGui::BeginGroup();
            ImGui::TextDisabled("%s", label);
            ImGui::ProgressBar(cpu->perCoreUsage[i] / 100.0f, ImVec2(w, 0.0f), nullptr);
            ImGui::Text("%.1f%%", cpu->perCoreUsage[i]);
            ImGui::EndGroup();
        }
    }
    ImGui::EndChild();
}

} // namespace htb
