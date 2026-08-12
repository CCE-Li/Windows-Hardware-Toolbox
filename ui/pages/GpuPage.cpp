#include "ui/pages/GpuPage.h"

#include <algorithm>
#include <string>

#include "hardware/HardwareTypes.h"
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

std::string outputsJoined(const std::vector<std::string>& outputs) {
    std::string joined;
    for (size_t i = 0; i < outputs.size(); ++i) {
        if (i > 0) joined += ", ";
        joined += outputs[i];
    }
    return joined.empty() ? "-" : joined;
}
} // namespace

void GpuPage::draw(UiContext& ctx) {
    ImGui::Text("GPU");
    ImGui::Separator();
    ImGui::Spacing();

    auto gpus = ctx.service.gpu().snapshot();
    if (!gpus || gpus->empty()) {
        ImGui::Text("No GPU adapters detected");
        return;
    }

    ImGui::BeginChild("gpu_body", ImVec2(0, 0));
    const float cardH = std::max(
        170.0f,
        (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y * (gpus->size() - 1)) /
            static_cast<float>(gpus->size()));
    int index = 0;
    for (const GpuInfo& gpu : *gpus) {
        const std::string id = "gpu_card_" + std::to_string(index++);
        ImGui::BeginChild(id.c_str(), ImVec2(0, cardH), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(0.86f, 0.60f, 0.15f, 1.0f), "%s", gpu.name.c_str());
        ImGui::Separator();

        if (ImGui::BeginTable("gpu_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            propRow("Vendor", std::string(vendorName(gpu.vendor)));
            propRow("Driver version",
                    gpu.driverAvailability == Availability::Available ? gpu.driverVersion : availabilityLabel(gpu.driverAvailability));
            propRow("Driver date",
                    gpu.driverAvailability == Availability::Available ? gpu.driverDate : "-");
            propRow("Dedicated VRAM", formatBytes(gpu.dedicatedVramBytes));
            propRow("Dedicated system memory", formatBytes(gpu.dedicatedSystemMemoryBytes));
            propRow("Shared system memory", formatBytes(gpu.sharedSystemMemoryBytes));
            propRow("Outputs", outputsJoined(gpu.outputs));
            ImGui::EndTable();
        }

        if (gpu.usageAvailability == Availability::Available) {
            ImGui::ProgressBar(gpu.usagePercent / 100.0f, ImVec2(-1.0f, 0.0f));
            ImGui::Text("Memory usage: %s of %s (%.1f%%)", formatBytes(gpu.usageBytes).c_str(),
                        formatBytes(gpu.dedicatedVramBytes).c_str(), gpu.usagePercent);
            ImGui::TextDisabled("Source: %s", gpu.usageSource.c_str());
        } else {
            ImGui::Text("Memory usage: %s", availabilityLabel(gpu.usageAvailability).c_str());
        }
        ImGui::EndChild();
        ImGui::Spacing();
    }
    ImGui::EndChild();
}

} // namespace htb
