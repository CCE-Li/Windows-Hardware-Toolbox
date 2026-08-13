#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/network/NetworkProvider.h"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <IPExport.h>
#include <icmpapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <ws2tcpip.h>

#include <chrono>
#include <map>
#include <string>

namespace htb {

namespace {
std::string macString(const UCHAR* addr, ULONG len) {
    std::string out;
    for (ULONG i = 0; i < len; ++i) {
        if (i > 0) out += "-";
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", addr[i]);
        out += buf;
    }
    return out;
}

std::string statusString(IF_OPER_STATUS status) {
    switch (status) {
        case IfOperStatusUp: return "已连接";
        case IfOperStatusDown: return "未连接";
        case IfOperStatusTesting: return "测试中";
        case IfOperStatusUnknown: return "未知";
        case IfOperStatusDormant: return "休眠";
        case IfOperStatusNotPresent: return "不存在";
        case IfOperStatusLowerLayerDown: return "下层断开";
        default: return "未知";
    }
}

struct Sample {
    std::chrono::steady_clock::time_point time;
    uint64_t rx = 0;
    uint64_t tx = 0;
};

std::string pingGateway(const std::string& gateway, uint64_t& latencyMs) {
    std::wstring w = toWide(gateway);
    IN_ADDR addr{};
    if (InetPtonW(AF_INET, w.c_str(), &addr) != 1) return "目标格式无效";

    const HANDLE handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE) return "ICMP 不可用";

    char data[32]{};
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + 64]{};
    const DWORD rc = IcmpSendEcho(handle, addr.S_un.S_addr, data, static_cast<WORD>(sizeof(data)), nullptr,
                                  replyBuf, sizeof(replyBuf), 800);
    std::string status;
    if (rc != 0) {
        const auto* reply = reinterpret_cast<const ICMP_ECHO_REPLY*>(replyBuf);
        if (reply->Status == IP_SUCCESS) {
            latencyMs = reply->RoundTripTime;
            status = "正常";
        } else {
            status = "不可达";
        }
    } else {
        status = "超时";
    }
    IcmpCloseHandle(handle);
    return status;
}
} // namespace

struct NetworkProvider::Impl {
    std::map<uint32_t, Sample> prev;
};

NetworkProvider::NetworkProvider() : m_impl(std::make_unique<Impl>()) {}

NetworkProvider::~NetworkProvider() = default;

void NetworkProvider::refresh() {
    auto adapters = std::make_shared<std::vector<NetworkAdapter>>();
    std::map<uint32_t, NetworkAdapter> byIndex;

    ULONG bufLen = 16 * 1024;
    std::vector<unsigned char> buf;
    IP_ADAPTER_ADDRESSES* aa = nullptr;
    for (int attempt = 0; attempt < 3; ++attempt) {
        buf.resize(bufLen);
        aa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        const ULONG rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, aa, &bufLen);
        if (rc == ERROR_BUFFER_OVERFLOW) continue;
        if (rc == ERROR_SUCCESS) break;
        HTB_WARN("[network] GetAdaptersAddresses failed: {:#x}", static_cast<unsigned>(rc));
        m_snapshot.store(std::move(adapters));
        return;
    }

    for (auto* a = aa; a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        NetworkAdapter adapter;
        adapter.name = toUtf8(a->FriendlyName);
        adapter.description = toUtf8(a->Description);
        adapter.mac = macString(a->PhysicalAddress, a->PhysicalAddressLength);
        adapter.linkSpeedBps = std::max(a->TransmitLinkSpeed, a->ReceiveLinkSpeed);
        adapter.status = statusString(a->OperStatus);
        adapter.dnsSuffix = toUtf8(a->DnsSuffix);
        adapter.mtu = a->Mtu;
        adapter.source = "IP Helper API";

        for (auto* addr = a->FirstUnicastAddress; addr; addr = addr->Next) {
            wchar_t text[128];
            DWORD textLen = 128;
            if (WSAAddressToStringW(addr->Address.lpSockaddr, addr->Address.iSockaddrLength, nullptr, text, &textLen) == 0) {
                adapter.addresses.push_back(toUtf8(text));
            }
        }
        for (auto* gw = a->FirstGatewayAddress; gw; gw = gw->Next) {
            wchar_t text[128];
            DWORD textLen = 128;
            if (WSAAddressToStringW(gw->Address.lpSockaddr, gw->Address.iSockaddrLength, nullptr, text, &textLen) == 0) {
                adapter.gateways.push_back(toUtf8(text));
            }
        }
        for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
            wchar_t text[128];
            DWORD textLen = 128;
            if (WSAAddressToStringW(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, text, &textLen) == 0) {
                adapter.dnsServers.push_back(toUtf8(text));
            }
        }
        byIndex[a->IfIndex] = std::move(adapter);
    }

    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR) {
        const auto now = std::chrono::steady_clock::now();
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const MIB_IF_ROW2& row = table->Table[i];
            auto it = byIndex.find(row.InterfaceIndex);
            if (it == byIndex.end()) continue;
            NetworkAdapter& adapter = it->second;
            adapter.rxBytes = row.InOctets;
            adapter.txBytes = row.OutOctets;
            adapter.inErrors = row.InErrors;
            adapter.outErrors = row.OutErrors;

            auto prevIt = m_impl->prev.find(row.InterfaceIndex);
            if (prevIt != m_impl->prev.end()) {
                const double dt = std::chrono::duration<double>(now - prevIt->second.time).count();
                if (dt > 0.01) {
                    adapter.rxRateBps = (static_cast<double>(row.InOctets) - static_cast<double>(prevIt->second.rx)) / dt;
                    adapter.txRateBps = (static_cast<double>(row.OutOctets) - static_cast<double>(prevIt->second.tx)) / dt;
                    if (adapter.rxRateBps < 0.0) adapter.rxRateBps = 0.0;
                    if (adapter.txRateBps < 0.0) adapter.txRateBps = 0.0;
                }
            }
            m_impl->prev[row.InterfaceIndex] = Sample{now, row.InOctets, row.OutOctets};
        }
        FreeMibTable(table);
    } else {
        HTB_WARN("[network] GetIfTable2 failed");
    }

    for (auto& [index, adapter] : byIndex) {
        if (!adapter.gateways.empty() && adapter.status == "已连接") {
            uint64_t latency = 0;
            const std::string ping = pingGateway(adapter.gateways.front(), latency);
            adapter.pingStatus = ping + (latency > 0 ? " (" + std::to_string(latency) + " ms)" : "");
            adapter.pingAvailability = Availability::Available;
        }
        adapters->push_back(std::move(adapter));
    }

    HTB_INFO("[network] {} adapter(s) enumerated", adapters->size());
    m_snapshot.store(std::move(adapters));
}

} // namespace htb
