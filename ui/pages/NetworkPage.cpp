#include "ui/pages/NetworkPage.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "core/util/Clipboard.h"
#include "hardware/network/NetworkProvider.h"
#include "ui/Format.h"

#include "imgui.h"

namespace htb {

namespace {
std::string formatRate(double bps) {
    if (bps < 1024.0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f B/s", bps);
        return buf;
    }
    return formatBytes(static_cast<uint64_t>(bps)) + "/s";
}

std::string joinStrings(const std::vector<std::string>& items) {
    std::string joined;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) joined += "\n";
        joined += items[i];
    }
    return joined.empty() ? "-" : joined;
}

void propRow(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value.c_str());
}
} // namespace

void NetworkPage::draw(UiContext& ctx) {
    ImGui::Text("网络");
    ImGui::Separator();
    ImGui::Spacing();

    auto adapters = ctx.service.network().snapshot();
    if (!adapters) {
        ImGui::Text("正在采集...");
        return;
    }

    ImGui::BeginChild("net_body", ImVec2(0, 0));
    for (const NetworkAdapter& a : *adapters) {
            const std::string id = "net_card_" + a.name;
            ImGui::BeginChild(id.c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders);
            const bool up = (a.status == "已连接");
            ImGui::TextColored(up ? ImVec4(0.40f, 0.78f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.30f, 1.0f),
                               "%s  [%s]", a.name.c_str(), a.status.c_str());
            ImGui::Separator();

            if (ImGui::BeginTable("net_props", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("属性", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn("值");
                ImGui::TableHeadersRow();
                propRow("描述", a.description);
                propRow("MAC 地址", a.mac.empty() ? "-" : a.mac);
                propRow("链路速率", a.linkSpeedBps > 0 ? formatRate(static_cast<double>(a.linkSpeedBps)) : "-");
                propRow("MTU", a.mtu > 0 ? std::to_string(a.mtu) : "-");
                propRow("IP 地址", joinStrings(a.addresses));
                propRow("网关", joinStrings(a.gateways));
                propRow("DNS 服务器", joinStrings(a.dnsServers));
                propRow("DNS 后缀", a.dnsSuffix.empty() ? "-" : a.dnsSuffix);
                propRow("来源", a.source);
                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), nullptr);
            ImGui::Text("下行: %s", formatRate(a.rxRateBps).c_str());
            ImGui::SameLine();
            ImGui::Text("上行: %s", formatRate(a.txRateBps).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(累计 %s / %s)", formatBytes(a.rxBytes).c_str(), formatBytes(a.txBytes).c_str());
            ImGui::Text("错误: 入站 %llu / 出站 %llu", static_cast<unsigned long long>(a.inErrors),
                        static_cast<unsigned long long>(a.outErrors));
            ImGui::EndChild();
            ImGui::Spacing();
        }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("网络测试");
    ImGui::Separator();

    static char pingTarget[128]{};
    const NetworkAdapter* connected = nullptr;
    for (const NetworkAdapter& a : *adapters) {
        if (a.status == "已连接") {
            connected = &a;
            break;
        }
    }
    if (pingTarget[0] == '\0' && connected && !connected->gateways.empty()) {
        snprintf(pingTarget, sizeof(pingTarget), "%s", connected->gateways.front().c_str());
    }

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Ping 目标", pingTarget, sizeof(pingTarget));
    ImGui::SameLine();
    if (ImGui::Button("开始 Ping")) ctx.service.runPingTest(pingTarget, 4);
    ImGui::SameLine();
    if (connected && !connected->gateways.empty()) {
        if (ImGui::Button("网关")) snprintf(pingTarget, sizeof(pingTarget), "%s", connected->gateways.front().c_str());
        ImGui::SameLine();
    }
    if (ImGui::Button("8.8.8.8")) snprintf(pingTarget, sizeof(pingTarget), "8.8.8.8");
    ImGui::SameLine();
    if (ImGui::Button("114.114.114.114")) snprintf(pingTarget, sizeof(pingTarget), "114.114.114.114");

    const auto ping = ctx.service.network().pingResult();
    if (ping && ping->inProgress) {
        ImGui::Text("正在测试 %s ...", ping->target.c_str());
    } else if (ping) {
        std::string report = "Ping " + ping->target + ": " + ping->status;
        report += "\n发送 " + std::to_string(ping->count) + " / 接收 " + std::to_string(ping->received);
        report += "\n平均 " + std::to_string(static_cast<int>(ping->avgMs)) + " ms";
        report += " | 最小 " + std::to_string(static_cast<int>(ping->minMs)) + " ms";
        report += " | 最大 " + std::to_string(static_cast<int>(ping->maxMs)) + " ms";
        const bool lostAll = ping->received == 0;
        if (lostAll) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", report.c_str());
        } else {
            ImGui::Text("%s", report.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("复制结果")) htb::copyToClipboard(report);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("DNS 解析测试");
    static char dnsHost[128]{};
    if (dnsHost[0] == '\0') snprintf(dnsHost, sizeof(dnsHost), "www.baidu.com");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("域名", dnsHost, sizeof(dnsHost));
    ImGui::SameLine();
    if (ImGui::Button("开始解析")) ctx.service.runDnsTest(dnsHost);

    const auto dns = ctx.service.network().dnsResult();
    if (dns && dns->inProgress) {
        ImGui::Text("正在解析 %s ...", dns->host.c_str());
    } else if (dns) {
        std::string report = "DNS " + dns->host + ": " + dns->status + " (" +
                             std::to_string(static_cast<int>(dns->elapsedMs)) + " ms)";
        for (const auto& addr : dns->addresses) {
            report += "\n  " + addr;
        }
        if (dns->status == "解析成功") {
            ImGui::Text("%s", report.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.30f, 1.0f), "%s", report.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("复制结果 (DNS)")) htb::copyToClipboard(report);
    }

    ImGui::EndChild();
}

} // namespace htb
