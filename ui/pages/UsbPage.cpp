#include "ui/pages/UsbPage.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "hardware/device/DeviceProvider.h"

#include "imgui.h"

namespace htb {

namespace {
std::string extractVidPid(const std::vector<std::string>& ids) {
    for (const auto& id : ids) {
        if (id.find("VEN_") != std::string::npos && id.find("PID_") != std::string::npos) {
            const size_t ven = id.find("VEN_");
            const size_t pid = id.find("PID_");
            if (ven != std::string::npos && pid != std::string::npos && pid > ven) {
                return id.substr(ven, pid + 8 - ven);
            }
        }
    }
    return {};
}

std::string deviceLabel(const DeviceInfo& d) {
    std::string label = d.name.empty() ? d.instanceId : d.name;
    const std::string vp = extractVidPid(d.hardwareIds);
    if (!vp.empty()) label += "  [" + vp + "]";
    return label;
}
} // namespace

void UsbPage::draw(UiContext& ctx) {
    ImGui::Text("USB");
    ImGui::Separator();
    ImGui::Spacing();

    auto devices = ctx.service.device().snapshot();
    if (!devices) {
        ctx.service.requestDeviceRefresh();
        ImGui::Text("正在枚举设备...");
        return;
    }

    std::vector<size_t> usb;
    for (size_t i = 0; i < devices->size(); ++i) {
        const DeviceInfo& d = (*devices)[i];
        if (d.className == "USB" || d.enumerator == "USB") usb.push_back(i);
    }

    ImGui::BeginChild("usb_body", ImVec2(0, 0));
    if (usb.empty()) {
        ImGui::Text("未检测到 USB 设备");
    } else {
        std::map<std::string, std::vector<size_t>> children;
        for (const size_t idx : usb) {
            children[(*devices)[idx].parentId].push_back(idx);
        }
        const std::vector<size_t> roots = children[""];

        std::vector<size_t> drawn;
        std::function<void(const std::string&)> drawNode = [&](const std::string& parentId) {
            auto it = children.find(parentId);
            if (it == children.end()) return;
            for (const size_t idx : it->second) {
                drawn.push_back(idx);
                const DeviceInfo& d = (*devices)[idx];
                const auto childIt = children.find(d.instanceId);
                const bool hasChildren = childIt != children.end();
                if (hasChildren) {
                    if (ImGui::TreeNode(d.instanceId.c_str(), "%s", deviceLabel(d).c_str())) {
                        drawNode(d.instanceId);
                        ImGui::TreePop();
                    }
                } else {
                    ImGui::BulletText("%s", deviceLabel(d).c_str());
                }
            }
        };
        for (const size_t root : roots) {
            const std::string& rid = (*devices)[root].instanceId;
            if (ImGui::TreeNode(rid.c_str(), "%s", deviceLabel((*devices)[root]).c_str())) {
                drawNode(rid);
                ImGui::TreePop();
            }
            drawn.push_back(root);
        }
        for (const size_t idx : usb) {
            if (std::find(drawn.begin(), drawn.end(), idx) == drawn.end()) {
                ImGui::BulletText("%s", deviceLabel((*devices)[idx]).c_str());
            }
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("共 %zu 个 USB 设备，树形结构基于父设备关系。", usb.size());
    }
    ImGui::EndChild();
}

} // namespace htb
