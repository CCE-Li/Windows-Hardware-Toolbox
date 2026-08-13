#include "core/logging/Logger.h"
#include "hardware/camera/ObsVirtualCam.h"

#include <windows.h>

#include <cstring>
#include <new>

namespace htb {

namespace {
constexpr const wchar_t* kQueueName = L"OBSVirtualCamVideo";
constexpr size_t kFrameHeaderSize = 32;
constexpr LONG kStateStarting = 1;
constexpr LONG kStateReady = 2;
constexpr LONG kStateStopping = 3;

struct QueueHeader {
    volatile LONG write_idx;
    volatile LONG read_idx;
    volatile LONG state;
    uint32_t offsets[3];
    uint32_t type;
    uint32_t cx;
    uint32_t cy;
    uint64_t interval;
    uint32_t reserved[8];
};

size_t align32(size_t v) {
    return (v + 31) & ~static_cast<size_t>(31);
}
} // namespace

struct ObsVirtualCam::Impl {
    HANDLE handle = nullptr;
    QueueHeader* header = nullptr;
    uint64_t* ts[3] = {};
    uint8_t* frame[3] = {};
    uint32_t cx = 0;
    uint32_t cy = 0;
};

ObsVirtualCam::~ObsVirtualCam() {
    close();
}

bool ObsVirtualCam::active() const {
    return m_impl != nullptr && m_impl->header != nullptr;
}

bool ObsVirtualCam::open(uint32_t width, uint32_t height, uint64_t interval100ns) {
    close();
    delete m_impl;
    m_impl = new (std::nothrow) Impl();
    if (!m_impl) {
        m_lastError = "内存不足";
        return false;
    }

    HANDLE existing = OpenFileMappingW(FILE_MAP_READ, FALSE, kQueueName);
    if (existing) {
        CloseHandle(existing);
        m_lastError = "OBS 虚拟摄像头队列已被占用（可能 OBS 正在运行并使用虚拟摄像头）";
        HTB_WARN("[camera] OBS virtual cam queue already in use");
        return false;
    }

    const DWORD frameSize = width * height * 3 / 2;
    size_t size = align32(sizeof(QueueHeader));
    uint32_t offsets[3]{};
    for (int i = 0; i < 3; ++i) {
        offsets[i] = static_cast<uint32_t>(size);
        size += frameSize + kFrameHeaderSize;
        size = align32(size);
    }

    m_impl->handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                        static_cast<DWORD>(size), kQueueName);
    if (!m_impl->handle) {
        m_lastError = "创建共享内存失败";
        delete m_impl;
        m_impl = nullptr;
        return false;
    }
    m_impl->header = static_cast<QueueHeader*>(MapViewOfFile(m_impl->handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!m_impl->header) {
        m_lastError = "映射共享内存失败";
        CloseHandle(m_impl->handle);
        m_impl->handle = nullptr;
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    std::memset(m_impl->header, 0, sizeof(QueueHeader));
    m_impl->header->state = kStateStarting;
    m_impl->header->cx = width;
    m_impl->header->cy = height;
    m_impl->header->interval = interval100ns;
    for (int i = 0; i < 3; ++i) {
        m_impl->header->offsets[i] = offsets[i];
        m_impl->ts[i] = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(m_impl->header) + offsets[i]);
        m_impl->frame[i] = reinterpret_cast<uint8_t*>(m_impl->header) + offsets[i] + kFrameHeaderSize;
    }
    m_impl->cx = width;
    m_impl->cy = height;
    m_lastError.clear();
    HTB_INFO("[camera] OBS virtual cam queue created: {}x{}", width, height);
    return true;
}

void ObsVirtualCam::writeFrame(const uint8_t* nv12, uint64_t timestamp100ns) {
    if (!m_impl || !m_impl->header) return;
    const LONG inc = InterlockedIncrement(&m_impl->header->write_idx);
    const unsigned idx = static_cast<unsigned>(inc % 3);
    *m_impl->ts[idx] = timestamp100ns;
    const size_t ySize = static_cast<size_t>(m_impl->cx) * m_impl->cy;
    memcpy(m_impl->frame[idx], nv12, ySize);
    memcpy(m_impl->frame[idx] + ySize, nv12 + ySize, ySize / 2);
    InterlockedExchange(&m_impl->header->read_idx, inc);
    InterlockedExchange(&m_impl->header->state, kStateReady);
}

void ObsVirtualCam::close() {
    if (!m_impl) return;
    if (m_impl->header && m_impl->handle) {
        InterlockedExchange(&m_impl->header->state, kStateStopping);
    }
    if (m_impl->header) UnmapViewOfFile(m_impl->header);
    if (m_impl->handle) CloseHandle(m_impl->handle);
    m_impl->header = nullptr;
    m_impl->handle = nullptr;
    delete m_impl;
    m_impl = nullptr;
}

} // namespace htb
