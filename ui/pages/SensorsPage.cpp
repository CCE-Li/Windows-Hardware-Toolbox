#include "ui/pages/SensorsPage.h"

#include "monitoring/Metric.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

void SensorsPage::draw(UiContext& ctx) {
    (void)ctx;
    ImGui::Text("传感器");
    ImGui::Separator();
    ImGui::Spacing();

    const std::string unsupported = availabilityLabel(Availability::Unsupported);

    ImGui::BeginChild("sensor_body", ImVec2(0, 0));
    ImGui::TextWrapped("温度、风扇转速、功耗等传感器数据依赖硬件厂商私有接口"
                       "(NVAPI / AMD ADL / Intel ML) 或主板监控芯片 (EC/Super I/O)。"
                       "当前版本不会伪造任何数值：");
    ImGui::Spacing();
    ImGui::BulletText("CPU 温度: %s", unsupported.c_str());
    ImGui::BulletText("GPU 温度: %s", unsupported.c_str());
    ImGui::BulletText("风扇转速: %s", unsupported.c_str());
    ImGui::BulletText("功耗 (封装): %s", unsupported.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("计划: 以可选插件形式接入厂商 SDK —— NVIDIA (NVAPI)、AMD (ADL)、Intel (ML)。"
                       "插件按需加载，缺失时对应数据保持 %s，绝不因单个传感器失败导致程序崩溃。",
                       unsupported.c_str());
    ImGui::TextDisabled("数据可用性定义: %s / %s / %s / %s",
                        availabilityLabel(Availability::Available).c_str(),
                        availabilityLabel(Availability::Unavailable).c_str(),
                        availabilityLabel(Availability::Unsupported).c_str(),
                        availabilityLabel(Availability::PermissionDenied).c_str());
    ImGui::EndChild();
}

} // namespace htb
