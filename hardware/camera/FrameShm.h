#pragma once

#include <cstddef>
#include <cstdint>

namespace htb {

constexpr uint32_t kFrameShmMagic = 0x48544246;
constexpr uint32_t kFrameShmOutWidth = 1280;
constexpr uint32_t kFrameShmOutHeight = 720;
constexpr uint32_t kFrameShmMaxWidth = 1920;
constexpr uint32_t kFrameShmMaxHeight = 1080;
constexpr size_t kFrameShmMaxData = static_cast<size_t>(kFrameShmMaxWidth) * kFrameShmMaxHeight * 3 / 2;
constexpr const wchar_t* kFrameShmName = L"Local\\HTB_VirtualCamera_Frame";

struct FrameShmHeader {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint64_t frameIndex;
    int64_t timestamp100ns;
    uint32_t dataSize;
    uint32_t generation;
    uint32_t sourceActive;
};

constexpr size_t kFrameShmTotalSize = sizeof(FrameShmHeader) + kFrameShmMaxData;

constexpr uint32_t kPreviewShmMagic = 0x48544250;
constexpr uint32_t kPreviewShmWidth = kFrameShmOutWidth;
constexpr uint32_t kPreviewShmHeight = kFrameShmOutHeight;
constexpr size_t kPreviewShmDataSize = static_cast<size_t>(kPreviewShmWidth) * kPreviewShmHeight * 3;
constexpr size_t kPreviewShmHeaderSize = 64;
constexpr size_t kPreviewShmTotalSize = kPreviewShmHeaderSize + kPreviewShmDataSize;
constexpr const wchar_t* kPreviewShmName = L"Local\\HTB_Camera_Preview";

struct PreviewShmHeader {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    uint32_t dataSize;
    uint32_t generation;
    uint64_t timestamp;
    uint32_t reserved[2];
};

} // namespace htb
