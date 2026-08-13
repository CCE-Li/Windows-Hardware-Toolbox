#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/storage/StorageProvider.h"
#include "hardware/wmi/WmiSession.h"

#include <devioctl.h>
#include <ntddstor.h>
#include <nvme.h>
#include <windows.h>

#include <map>
#include <string>
#include <vector>

namespace htb {

namespace {

struct NvmeHealthLog {
    uint8_t criticalWarning;
    uint8_t temperature[2];
    uint8_t availableSpare;
    uint8_t availableSpareThreshold;
    uint8_t percentageUsed;
    uint8_t reserved1[26];
    uint8_t dataUnitsRead[16];
    uint8_t dataUnitsWritten[16];
    uint8_t hostReadCommands[16];
    uint8_t hostWriteCommands[16];
    uint8_t controllerBusyTime[16];
    uint8_t powerCycles[16];
    uint8_t powerOnHours[16];
    uint8_t unsafeShutdowns[16];
    uint8_t mediaErrors[16];
    uint8_t numberOfErrorInfoLogEntries[16];
};

uint64_t readLe128(const uint8_t* b) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(b[i]) << (8 * i);
    return v;
}

std::string criticalWarningText(uint8_t warning) {
    std::string out;
    if (warning & 0x01) out += "温度过高; ";
    if (warning & 0x02) out += "设备降级; ";
    if (warning & 0x04) out += "可靠性降低; ";
    if (warning & 0x08) out += "备用空间不足; ";
    if (warning & 0x10) out += "持久性媒体错误; ";
    if (warning & 0x20) out += "易失性备份失败; ";
    if (out.empty()) out = "无";
    return out;
}

struct NvmePropertyQuery {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE QueryType;
    STORAGE_PROTOCOL_SPECIFIC_DATA ProtocolSpecific;
};

bool readNvmeHealth(const std::string& deviceId, NvmeHealth& health) {
    const std::wstring path = toWide(deviceId);
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    NvmePropertyQuery query{};
    query.PropertyId = StorageDeviceProtocolSpecificProperty;
    query.QueryType = PropertyStandardQuery;
    query.ProtocolSpecific.ProtocolType = ProtocolTypeNvme;
    query.ProtocolSpecific.DataType = NVMeDataTypeLogPage;
    query.ProtocolSpecific.ProtocolDataRequestValue = NVME_LOG_PAGE_HEALTH_INFO;
    query.ProtocolSpecific.ProtocolDataRequestSubValue = 0;
    query.ProtocolSpecific.ProtocolDataOffset = sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA);
    query.ProtocolSpecific.ProtocolDataLength = sizeof(NvmeHealthLog);

    std::vector<uint8_t> buffer(sizeof(STORAGE_PROTOCOL_SPECIFIC_DATA) + sizeof(NvmeHealthLog));
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
                                    static_cast<DWORD>(buffer.size()), &bytesReturned, nullptr);
    CloseHandle(handle);
    if (!ok || bytesReturned < buffer.size()) return false;

    const auto* spec = reinterpret_cast<const STORAGE_PROTOCOL_SPECIFIC_DATA*>(buffer.data());
    if (spec->ProtocolDataOffset + spec->ProtocolDataLength > buffer.size()) return false;
    const auto* log = reinterpret_cast<const NvmeHealthLog*>(buffer.data() + spec->ProtocolDataOffset);

    const uint16_t tempK = static_cast<uint16_t>(log->temperature[0]) |
                           (static_cast<uint16_t>(log->temperature[1]) << 8);
    if (tempK > 0) health.temperatureC = static_cast<double>(tempK) - 273.15;
    if (log->percentageUsed > 0) health.percentageUsed = log->percentageUsed;
    health.powerOnHours = readLe128(log->powerOnHours);
    health.powerCycles = readLe128(log->powerCycles);
    health.unsafeShutdowns = readLe128(log->unsafeShutdowns);
    health.mediaErrors = readLe128(log->mediaErrors);
    health.dataUnitsRead = readLe128(log->dataUnitsRead);
    health.dataUnitsWritten = readLe128(log->dataUnitsWritten);
    health.criticalWarning = criticalWarningText(log->criticalWarning);
    health.availability = Availability::Available;
    return true;
}

std::string busTypeName(uint16_t busType) {
    switch (busType) {
        case 0: return "未知";
        case 1: return "SCSI";
        case 2: return "ATAPI";
        case 3: return "ATA";
        case 4: return "IEEE1394";
        case 5: return "SSA";
        case 6: return "光纤通道";
        case 7: return "USB";
        case 8: return "RAID";
        case 9: return "iSCSI";
        case 10: return "SAS";
        case 11: return "SATA";
        case 12: return "SD";
        case 13: return "MMC";
        case 14: return "虚拟";
        case 15: return "文件映射虚拟";
        case 16: return "存储空间";
        case 17: return "NVMe";
        default: return "未知";
    }
}

std::string mediaTypeName(uint16_t mediaType) {
    switch (mediaType) {
        case 0: return "未指定";
        case 3: return "HDD 机械硬盘";
        case 4: return "SSD 固态硬盘";
        case 5: return "SCM";
        default: return "未知";
    }
}

std::string healthName(uint16_t health) {
    switch (health) {
        case 0: return "健康";
        case 1: return "警告";
        case 2: return "不健康";
        default: return "未知";
    }
}
} // namespace

struct StorageProvider::Impl {
    std::unique_ptr<WmiSession> wmi;
};

StorageProvider::StorageProvider() : m_impl(std::make_unique<Impl>()) {}

StorageProvider::~StorageProvider() = default;

void StorageProvider::refresh() {
    auto disks = std::make_shared<std::vector<StorageDisk>>();
    if (!m_impl->wmi) m_impl->wmi = std::make_unique<WmiSession>();

    std::map<std::string, StorageDisk> byDeviceId;

    if (m_impl->wmi->connect(L"root\\cimv2")) {
        m_impl->wmi->query(
            L"SELECT Model,SerialNumber,FirmwareRevision,InterfaceType,Size,DeviceID "
            L"FROM Win32_DiskDrive",
            [&](IWbemClassObject* obj) {
                StorageDisk disk;
                readWmiString(obj, L"Model", disk.name);
                readWmiString(obj, L"SerialNumber", disk.serial);
                readWmiString(obj, L"FirmwareRevision", disk.firmware);
                readWmiString(obj, L"InterfaceType", disk.interfaceType);
                readWmiString(obj, L"DeviceID", disk.deviceId);
                readWmiUint64(obj, L"Size", disk.sizeBytes);
                if (disk.name.empty()) readWmiString(obj, L"Caption", disk.name);
                if (!disk.deviceId.empty()) byDeviceId[disk.deviceId] = std::move(disk);
                return true;
            });
    }

    if (m_impl->wmi->connect(L"root\\Microsoft\\Windows\\Storage")) {
        m_impl->wmi->query(
            L"SELECT DeviceId,MediaType,HealthStatus,OperationalStatus,Temperature,FriendlyName "
            L"FROM MSFT_PhysicalDisk",
            [&](IWbemClassObject* obj) {
                std::string deviceId;
                readWmiString(obj, L"DeviceId", deviceId);
                std::string key = "\\\\.\\PHYSICALDRIVE" + deviceId;
                StorageDisk& disk = byDeviceId[key];

                std::string friendly;
                if (readWmiString(obj, L"FriendlyName", friendly) && !friendly.empty()) disk.name = friendly;
                uint16_t media = 0;
                if (readWmiUint16(obj, L"MediaType", media)) disk.mediaType = mediaTypeName(media);
                uint16_t health = 0;
                if (readWmiUint16(obj, L"HealthStatus", health)) {
                    disk.healthStatus = healthName(health);
                    disk.healthAvailability = Availability::Available;
                }
                readWmiString(obj, L"OperationalStatus", disk.operationalStatus);
                uint16_t temp = 0;
                if (readWmiUint16(obj, L"Temperature", temp) && temp != 0)
                    disk.temperatureC = static_cast<double>(temp);
                return true;
            });
        m_impl->wmi->query(
            L"SELECT Number,BusType,IsBoot,IsSystem FROM MSFT_Disk",
            [&](IWbemClassObject* obj) {
                std::string number;
                readWmiString(obj, L"Number", number);
                std::string key = "\\\\.\\PHYSICALDRIVE" + number;
                auto it = byDeviceId.find(key);
                if (it == byDeviceId.end()) return true;
                uint16_t bus = 0;
                if (readWmiUint16(obj, L"BusType", bus)) it->second.busType = busTypeName(bus);
                uint16_t isBoot = 0;
                if (readWmiUint16(obj, L"IsBoot", isBoot)) it->second.isBoot = isBoot != 0;
                uint16_t isSystem = 0;
                if (readWmiUint16(obj, L"IsSystem", isSystem)) it->second.isSystem = isSystem != 0;
                return true;
            });
    }

    for (auto& [key, disk] : byDeviceId) {
        disk.source = "WMI (Win32_DiskDrive / Storage Management)";
        if (!disk.deviceId.empty()) {
            NvmeHealth nvme;
            if (readNvmeHealth(disk.deviceId, nvme)) {
                disk.nvme = std::move(nvme);
                HTB_DEBUG("[storage] {} NVMe health: temp {:.0f} C, wear {}%, POH {}, media errors {}",
                          disk.name, disk.nvme.temperatureC.value_or(-273.0), disk.nvme.percentageUsed.value_or(0),
                          disk.nvme.powerOnHours.value_or(0), disk.nvme.mediaErrors.value_or(0));
            } else {
                HTB_DEBUG("[storage] {}: NVMe health log unavailable (SATA or unsupported)", disk.name);
            }
        }
        disks->push_back(std::move(disk));
    }

    HTB_INFO("[storage] {} disk(s) reported by WMI", disks->size());
    m_snapshot.store(std::move(disks));
}

} // namespace htb
