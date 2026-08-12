#include "ui/pages/DashboardPage.h"

#include <chrono>
#include <functional>
#include <string>

#include "core/runtime/SystemInfo.h"
#include "hardware/storage/StorageProvider.h"
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

    ImGui::Text("仪表盘");
    ImGui::Separator();
    ImGui::Spacing();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float tileW = (avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const float tileH = 160.0f;
    const float rowH = 140.0f;

    auto cpu = ctx.service.cpu().snapshot();
    auto gpus = ctx.service.gpu().snapshot();
    auto mem = ctx.service.memory().snapshot();
    auto disks = ctx.service.storage().snapshot();
    auto battery = ctx.service.battery().snapshot();

    ImGui::BeginChild("dash_body", ImVec2(0, 0));
    drawTile("tile_cpu", "CPU", ImVec2(tileW, tileH), [&] {
        if (!cpu) {
            ImGui::Text("正在采集...");
            return;
        }
        ImGui::TextWrapped("%s", cpu->name.c_str());
        ImGui::Spacing();
        if (cpu->usageAvailability == Availability::Available) {
            ImGui::ProgressBar(cpu->totalUsage / 100.0f, ImVec2(-1.0f, 0.0f));
            ImGui::Text("使用率: %.1f%%", cpu->totalUsage);
        } else {
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), availabilityLabel(cpu->usageAvailability).c_str());
        }
        ImGui::Text("%u核/%u线程", cpu->physicalCores, cpu->logicalCores);
        if (cpu->baseFrequencyMHz) {
            ImGui::Text("基础频率: %s", formatMhz(*cpu->baseFrequencyMHz).c_str());
        } else {
            ImGui::Text("基础频率: %s", availabilityLabel(Availability::Unsupported).c_str());
        }
    });
    ImGui::SameLine();
    drawTile("tile_gpu", "GPU", ImVec2(tileW, tileH), [&] {
        if (!gpus || gpus->empty()) {
            ImGui::Text("未检测到 GPU 适配器");
            return;
        }
        const GpuInfo& gpu = gpus->front();
        ImGui::TextWrapped("%s", gpu.name.c_str());
        ImGui::Spacing();
        if (gpu.usageAvailability == Availability::Available) {
            ImGui::ProgressBar(gpu.usagePercent / 100.0f, ImVec2(-1.0f, 0.0f));
            ImGui::Text("显存: %s / %s (%.1f%%)", formatBytes(gpu.usageBytes).c_str(),
                        formatBytes(gpu.dedicatedVramBytes).c_str(), gpu.usagePercent);
        } else {
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), availabilityLabel(gpu.usageAvailability).c_str());
            ImGui::Text("显存: %s", formatBytes(gpu.dedicatedVramBytes).c_str());
        }
        ImGui::Text("%s", vendorName(gpu.vendor).data());
    });
    drawTile("tile_mem", "内存", ImVec2(tileW, rowH), [&] {
        if (!mem) {
            ImGui::Text("正在采集...");
            return;
        }
        ImGui::ProgressBar(mem->loadPercent / 100.0f, ImVec2(-1.0f, 0.0f));
        ImGui::Text("已用: %s / %s", formatBytes(mem->usedBytes).c_str(), formatBytes(mem->totalBytes).c_str());
        ImGui::Text("可用: %s", formatBytes(mem->availableBytes).c_str());
    });
    ImGui::SameLine();
    drawTile("tile_sys", "系统", ImVec2(tileW, rowH), [&] {
        ImGui::TextWrapped("%s", system.osName.c_str());
        if (!system.osDisplayVersion.empty()) ImGui::Text("版本: %s", system.osDisplayVersion.c_str());
        ImGui::Text("系统版本: %s", system.osVersion.c_str());
        ImGui::Text("内部版本: %s", system.osBuild.c_str());
        ImGui::Text("架构: %s", system.architecture.c_str());
    });
    drawTile("tile_storage", "存储", ImVec2(tileW, rowH), [&] {
        if (!disks || disks->empty()) {
            ImGui::Text("未检测到磁盘");
            return;
        }
        size_t healthy = 0;
        size_t warning = 0;
        for (const StorageDisk& d : *disks) {
            if (d.healthAvailability == Availability::Available) {
                if (d.healthStatus == "健康") ++healthy;
                else ++warning;
            }
        }
        ImGui::Text("物理磁盘: %zu 块", disks->size());
        if (healthy > 0) ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.45f, 1.0f), "健康: %zu", healthy);
        if (warning > 0) ImGui::TextColored(ImVec4(0.90f, 0.75f, 0.30f, 1.0f), "需关注: %zu", warning);
        for (const StorageDisk& d : *disks) {
            ImGui::TextWrapped("%s (%s)", d.name.c_str(), formatBytes(d.sizeBytes).c_str());
        }
    });
    ImGui::SameLine();
    drawTile("tile_battery", "电池", ImVec2(tileW, rowH), [&] {
        if (!battery) {
            ImGui::Text("正在采集...");
            return;
        }
        if (battery->availability != Availability::Available) {
            ImGui::Text("电池信息 %s", availabilityLabel(battery->availability).c_str());
            return;
        }
        ImGui::ProgressBar(battery->chargePercent / 100.0f, ImVec2(-1.0f, 0.0f));
        ImGui::Text("电量: %u%%", battery->chargePercent);
        ImGui::Text("状态: %s", battery->status.empty() ? "-" : battery->status.c_str());
        if (!battery->name.empty()) ImGui::TextDisabled("%s", battery->name.c_str());
    });
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("监控");
    ImGui::Text("刷新间隔: %d ms | 最近刷新: %.1fs 前",
                ctx.service.intervalMs(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx.service.lastRefresh()).count());
    ImGui::TextDisabled("硬件查询在工作线程执行，UI 线程不会被阻塞。");
    ImGui::EndChild();
}

} // namespace htb
