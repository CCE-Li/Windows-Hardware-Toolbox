#include "core/logging/Logger.h"
#include "hardware/storage/StorageProvider.h"
#include "hardware/wmi/WmiSession.h"

#include <windows.h>

#include <map>
#include <string>

namespace htb {

namespace {
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
        disks->push_back(std::move(disk));
    }

    HTB_INFO("[storage] {} disk(s) reported by WMI", disks->size());
    m_snapshot.store(std::move(disks));
}

} // namespace htb
