#include "ui/pages/SensorsPage.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

#include "hardware/gpu/GpuProvider.h"
#include "hardware/memory/MemoryProvider.h"
#include "hardware/sensors/SensorsProvider.h"
#include "hardware/storage/StorageProvider.h"
#include "monitoring/Metric.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {

double nowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

ImVec4 tempColor(double c) {
    if (c >= 85.0) return ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
    if (c >= 70.0) return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
    return ImVec4(0.40f, 0.78f, 0.45f, 1.0f);
}

} // namespace

void SensorsPage::draw(UiContext& ctx) {
    ImGui::Text("传感器");
    ImGui::Separator();
    ImGui::Spacing();

    const double now = nowSeconds();

    ImGui::BeginChild("sensor_body", ImVec2(0, 0));

    auto drawCard = [&](const std::string& id, const std::string& title, const std::string& valueText,
                        bool available, double value, bool percentMode, float yMax, const std::string& source,
                        ImVec4 color) {
        auto it = m_charts.try_emplace(id, 300, 90.0).first;
        if (available) it->second.sample(value, now);

        ImGui::BeginChild(id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", title.c_str());
        ImGui::SameLine();
        if (available) {
            ImGui::TextColored(color, "%s", valueText.c_str());
        } else {
            ImGui::TextDisabled("%s", valueText.c_str());
        }
        ImGui::TextDisabled("来源: %s", source.c_str());
        const char* peak = it->second.pointCount() > 0 ? "峰值" : "";
        it->second.draw(id.c_str(), 80.0f, yMax, percentMode, valueText, peak);
        ImGui::EndChild();
        ImGui::Spacing();
    };

    const auto sensors = ctx.service.sensors().snapshot();
    if (!sensors) {
        ImGui::Text("正在采集...");
    } else {
        for (const SensorReading& s : sensors->sensors) {
            char val[64];
            const bool ok = s.availability == Availability::Available;
            if (ok) {
                snprintf(val, sizeof(val), "%.1f %s", s.value, s.unit.c_str());
            } else {
                snprintf(val, sizeof(val), "N/A");
            }
            drawCard("sensor_" + s.id, s.name, ok ? val : "N/A", ok, s.value, false, 100.0f, s.source,
                     ok ? tempColor(s.value) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }
    }

    ImGui::Separator();
    ImGui::Text("系统遥测");
    ImGui::Separator();
    ImGui::Spacing();

    // CPU 使用率 / 频率
    const auto cpu = ctx.service.cpu().snapshot();
    if (cpu) {
        const bool ok = cpu->usageAvailability == Availability::Available;
        char val[64];
        if (ok) {
            snprintf(val, sizeof(val), "%.1f%%", cpu->totalUsage);
        } else {
            snprintf(val, sizeof(val), "N/A");
        }
        drawCard("cpu_usage", "CPU 使用率", ok ? val : "N/A", ok, cpu->totalUsage, true, 100.0f,
                 cpu->usageSource, ImVec4(0.40f, 0.70f, 0.95f, 1.0f));

        if (cpu->currentFrequencyMHz) {
            char fv[64];
            snprintf(fv, sizeof(fv), "%.2f GHz", *cpu->currentFrequencyMHz / 1000.0);
            drawCard("cpu_freq", "CPU 当前频率", fv, true, *cpu->currentFrequencyMHz, false, 6000.0f,
                     "PDH", ImVec4(0.55f, 0.65f, 0.85f, 1.0f));
        }
    }

    // GPU 使用率
    const auto gpus = ctx.service.gpu().snapshot();
    if (gpus && !gpus->empty()) {
        const GpuInfo& g = gpus->front();
        const bool ok = g.engineAvailability == Availability::Available || g.usageAvailability == Availability::Available;
        const float usage = g.engineAvailability == Availability::Available ? g.engineUsagePercent : g.usagePercent;
        char val[64];
        if (ok) {
            snprintf(val, sizeof(val), "%.1f%%", usage);
        } else {
            snprintf(val, sizeof(val), "N/A");
        }
        drawCard("gpu_usage", g.name.empty() ? "GPU 使用率" : g.name, ok ? val : "N/A", ok, usage, true,
                 100.0f, g.engineAvailability == Availability::Available ? g.engineSource : g.usageSource,
                 ImVec4(0.75f, 0.55f, 0.40f, 1.0f));
    }

    // 内存使用率
    const auto mem = ctx.service.memory().snapshot();
    if (mem) {
        const bool ok = mem->totalBytes > 0;
        const double pct = ok ? static_cast<double>(mem->usedBytes) * 100.0 / static_cast<double>(mem->totalBytes) : 0.0;
        char val[64];
        if (ok) {
            snprintf(val, sizeof(val), "%.1f%% (%s / %s)", pct, formatBytes(mem->usedBytes).c_str(),
                     formatBytes(mem->totalBytes).c_str());
        } else {
            snprintf(val, sizeof(val), "N/A");
        }
        drawCard("memory", "内存使用率", ok ? val : "N/A", ok, pct, true, 100.0f, mem->source,
                 ImVec4(0.70f, 0.50f, 0.75f, 1.0f));
    }

    // NVMe 温度
    const auto disks = ctx.service.storage().snapshot();
    if (disks) {
        for (const StorageDisk& d : *disks) {
            if (d.nvme.temperatureC) {
                char val[64];
                snprintf(val, sizeof(val), "%.0f °C", *d.nvme.temperatureC);
                drawCard("nvme_" + d.name, d.name + " (NVMe 温度)", val, true, *d.nvme.temperatureC, false,
                         100.0f, d.nvme.source, tempColor(*d.nvme.temperatureC));
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("风扇转速、电压与更多温度传感器依赖硬件厂商私有接口 (NVAPI / AMD ADL / Intel ML) "
                       "或主板监控芯片 (EC/Super I/O)，当前版本不会伪造数值。"
                       "计划以可选插件形式接入厂商 SDK，缺失时对应数据保持 N/A。");

    ImGui::EndChild();
}

} // namespace htb