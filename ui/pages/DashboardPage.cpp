#include "ui/pages/DashboardPage.h"

#include <chrono>
#include <functional>
#include <string>

#include "core/runtime/SystemInfo.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {
void drawTile(const char* id, const char* label, const ImVec2& size, const std::function<void()>& body) {
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.72f, 1.0f), "%s", label);
    ImGui::Separator();
    body();
    ImGui::EndChild();
}
} // namespace

void DashboardPage::draw(UiContext& ctx) {
    static const SystemInfo system = querySystemInfo();

    ImGui::Text("Dashboard");
    ImGui::Separator();
    ImGui::Spacing();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float tileW = (avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const float tileH = 150.0f;
    const float rowH = 130.0f;

    auto cpu = ctx.service.cpu().snapshot();
    auto gpus = ctx.service.gpu().snapshot();
    auto mem = ctx.service.memory().snapshot();

    ImGui::BeginChild("dash_body", ImVec2(0, 0));
    drawTile("tile_cpu", "CPU", ImVec2(tileW, tileH), [&] {
        if (!cpu) {
            ImGui::Text("Collecting...");
            return;
        }
        ImGui::TextWrapped("%s", cpu->name.c_str());
        ImGui::Spacing();
        if (cpu->usageAvailability == Availability::Available) {
            ImGui::ProgressBar(cpu->totalUsage / 100.0f, ImVec2(-1.0f, 0.0f));
            ImGui::Text("Usage: %.1f%%", cpu->totalUsage);
        } else {
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), availabilityLabel(cpu->usageAvailability).c_str());
        }
        ImGui::Text("%uC/%uT", cpu->physicalCores, cpu->logicalCores);
        if (cpu->baseFrequencyMHz) {
            ImGui::Text("Base: %s", formatMhz(*cpu->baseFrequencyMHz).c_str());
        } else {
            ImGui::Text("Base: %s", availabilityLabel(Availability::Unsupported).c_str());
        }
    });
    ImGui::SameLine();
    drawTile("tile_gpu", "GPU", ImVec2(tileW, tileH), [&] {
        if (!gpus || gpus->empty()) {
            ImGui::Text("No GPU adapters detected");
            return;
        }
        const GpuInfo& gpu = gpus->front();
        ImGui::TextWrapped("%s", gpu.name.c_str());
        ImGui::Spacing();
        if (gpu.usageAvailability == Availability::Available) {
            ImGui::ProgressBar(gpu.usagePercent / 100.0f, ImVec2(-1.0f, 0.0f));
            ImGui::Text("VRAM: %s / %s (%.1f%%)", formatBytes(gpu.usageBytes).c_str(),
                        formatBytes(gpu.dedicatedVramBytes).c_str(), gpu.usagePercent);
        } else {
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), availabilityLabel(gpu.usageAvailability).c_str());
            ImGui::Text("VRAM: %s", formatBytes(gpu.dedicatedVramBytes).c_str());
        }
        ImGui::Text("%s", vendorName(gpu.vendor).data());
    });
    drawTile("tile_mem", "Memory", ImVec2(tileW, rowH), [&] {
        if (!mem) {
            ImGui::Text("Collecting...");
            return;
        }
        ImGui::ProgressBar(mem->loadPercent / 100.0f, ImVec2(-1.0f, 0.0f));
        ImGui::Text("Used: %s / %s", formatBytes(mem->usedBytes).c_str(), formatBytes(mem->totalBytes).c_str());
        ImGui::Text("Available: %s", formatBytes(mem->availableBytes).c_str());
    });
    ImGui::SameLine();
    drawTile("tile_sys", "System", ImVec2(tileW, rowH), [&] {
        ImGui::TextWrapped("%s", system.osName.c_str());
        if (!system.osDisplayVersion.empty()) ImGui::Text("Edition: %s", system.osDisplayVersion.c_str());
        ImGui::Text("Version: %s", system.osVersion.c_str());
        ImGui::Text("Build: %s", system.osBuild.c_str());
        ImGui::Text("Arch: %s", system.architecture.c_str());
    });
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Monitoring");
    ImGui::Text("Refresh interval: %d ms | Last refresh: %.1fs ago",
                ctx.service.intervalMs(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx.service.lastRefresh()).count());
    ImGui::TextDisabled("Hardware queries run on a worker thread; the UI thread is never blocked.");
    ImGui::EndChild();
}

} // namespace htb
