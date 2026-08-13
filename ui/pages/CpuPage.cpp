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
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("cpu_body", ImVec2(0, 0));
    if (ImGui::BeginTable("cpu_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("值");
        ImGui::TableHeadersRow();
        propRow("名称", cpu->name);
        propRow("厂商", cpu->vendor);
        propRow("架构", cpu->architecture);
        propRow("物理核心数", std::to_string(cpu->physicalCores));
        propRow("逻辑核心数", std::to_string(cpu->logicalCores));
        propRow("基础频率", cpu->baseFrequencyMHz
                                       ? formatMhz(*cpu->baseFrequencyMHz)
                                       : std::string(availabilityLabel(Availability::Unsupported)));
        propRow("当前频率",
                cpu->currentFrequencyMHz ? formatMhz(static_cast<uint32_t>(*cpu->currentFrequencyMHz))
                                         : std::string(availabilityLabel(Availability::Unavailable)));
        propRow("静态信息来源", cpu->staticSource);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Text("使用率");
    ImGui::Separator();

    if (cpu->usageAvailability != Availability::Available) {
        ImGui::TextWrapped("使用率数据 %s。来源: %s",
                           availabilityLabel(cpu->usageAvailability).c_str(),
                           cpu->usageSource.empty() ? "-" : cpu->usageSource.c_str());
    } else {
        ImGui::Text("总计");
        ImGui::ProgressBar(cpu->totalUsage / 100.0f, ImVec2(-1.0f, 0.0f), nullptr);
        ImGui::Text("总计: %.1f%%   (来源: %s)", cpu->totalUsage, cpu->usageSource.c_str());
        ImGui::Spacing();
        ImGui::Text("各逻辑核心");
        const int perRow = 4;
        for (size_t i = 0; i < cpu->perCoreUsage.size(); ++i) {
            if (i > 0 && i % perRow != 0) ImGui::SameLine();
            const float w = (ImGui::GetContentRegionAvail().x -
                             ImGui::GetStyle().ItemSpacing.x * (perRow - 1)) /
                            perRow;
            char label[32];
            snprintf(label, sizeof(label), "核心 %zu", i);
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
