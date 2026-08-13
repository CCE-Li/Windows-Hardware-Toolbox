#include "VirtualCameraProvider.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

std::string guidString(REFGUID g) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g.Data1, g.Data2, g.Data3,
             g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return buf;
}

void traceLog(const std::string& msg) {
    OutputDebugStringA(("htb-vcam: " + msg + "\n").c_str());
    char tmp[MAX_PATH]{};
    if (GetTempPathA(MAX_PATH, tmp) == 0) return;
    std::string path = std::string(tmp) + "htb_vcam_trace.log";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "a") == 0 && f) {
        fprintf(f, "%s\n", msg.c_str());
        fclose(f);
    }
}

HRESULT queueEvent(IMFMediaEventQueue* queue, MediaEventType type, HRESULT hrStatus, const PROPVARIANT* value) {
    ComPtr<IMFMediaEvent> ev;
    HRESULT hr = MFCreateMediaEvent(type, GUID_NULL, hrStatus, value, &ev);
    if (FAILED(hr)) return hr;
    return queue->QueueEvent(ev.Get());
}

constexpr DWORD kFrameWidth = 640;
constexpr DWORD kFrameHeight = 480;
constexpr DWORD kFrameRate = 30;
constexpr long long kFrameDuration = 10000000LL / kFrameRate;
constexpr DWORD kYPlaneSize = kFrameWidth * kFrameHeight;
constexpr DWORD kFrameSize = kYPlaneSize + (kFrameWidth / 2) * (kFrameHeight / 2) * 2;

struct YuvColor {
    uint8_t y;
    uint8_t u;
    uint8_t v;
};

constexpr YuvColor kColorBars[7] = {
    {180, 32, 32},   // 白
    {162, 44, 132},  // 黄
    {131, 156, 44},  // 青
    {113, 168, 144}, // 绿
    {84, 63, 184},   // 品红
    {66, 75, 224},   // 红
    {35, 187, 236},  // 蓝
};

void fillTestPattern(uint8_t* y, uint8_t* uv, uint64_t frameIndex) {
    const DWORD barWidth = kFrameWidth / 7;
    for (DWORD row = 0; row < kFrameHeight / 2; ++row) {
        for (DWORD col = 0; col < kFrameWidth; ++col) {
            const size_t bar = col / barWidth;
            const bool checker = ((col / 32) + (row / 32)) % 2 == 0;
            uint8_t yv = kColorBars[bar].y;
            if (row >= kFrameHeight / 4) {
                yv = checker ? 128 : 40;
            }
            y[row * kFrameWidth + col] = yv;
        }
    }
    const DWORD uvWidth = kFrameWidth / 2;
    for (DWORD row = 0; row < kFrameHeight / 2; ++row) {
        for (DWORD col = 0; col < uvWidth; ++col) {
            const size_t bar = (col * 2) / barWidth;
            uint8_t u = kColorBars[bar].u;
            uint8_t v = kColorBars[bar].v;
            if (row >= kFrameHeight / 4) {
                u = 128;
                v = 128;
            }
            uv[row * uvWidth * 2 + col * 2] = u;
            uv[row * uvWidth * 2 + col * 2 + 1] = v;
        }
    }
    const DWORD bottom = kFrameHeight - 8;
    for (DWORD i = 0; i < 24; ++i) {
        const bool bit = (frameIndex >> i) & 1ULL;
        for (DWORD col = 0; col < 8; ++col) {
            for (DWORD row = 0; row < 8; ++row) {
                const size_t idx = (bottom + row) * kFrameWidth + i * 8 + col;
                y[idx] = bit ? 210 : 24;
            }
        }
    }
    for (DWORD col = 0; col < kFrameWidth; ++col) {
        const bool bit = ((frameIndex / 30) & 1ULL) == 0;
        y[col] = bit ? 180 : 60;
    }
}

class VirtualCameraSource;

class VirtualCameraStream final : public IMFMediaStream {
public:
    explicit VirtualCameraStream(VirtualCameraSource* owner) : m_owner(owner) {
        MFCreateEventQueue(&m_events);
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }

    STDMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** ppEvent) override {
        return m_events->GetEvent(flags, ppEvent);
    }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* cb, IUnknown* state) override {
        return m_events->BeginGetEvent(cb, state);
    }
    STDMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** ppEvent) override {
        return m_events->EndGetEvent(result, ppEvent);
    }
    STDMETHODIMP QueueEvent(MediaEventType type, REFGUID extType, HRESULT hr, const PROPVARIANT* value) override {
        ComPtr<IMFMediaEvent> ev;
        HRESULT h = MFCreateMediaEvent(type, extType, hr, value, &ev);
        if (FAILED(h)) return h;
        return m_events->QueueEvent(ev.Get());
    }

    STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppDescriptor) override;
    STDMETHODIMP GetMediaSource(IMFMediaSource** ppSource) override;
    STDMETHODIMP RequestSample(IUnknown* token) override;

    HRESULT init();
    IMFMediaEventQueue* events() const { return m_events.Get(); }

private:
    volatile long m_ref = 1;
    ComPtr<IMFMediaEventQueue> m_events;
    ComPtr<IMFStreamDescriptor> m_descriptor;
    VirtualCameraSource* m_owner;
};

class VirtualCameraSource final : public IMFMediaSource, public IMFActivate {
public:
    VirtualCameraSource() {
        MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        MFCreateEventQueue(&m_events);
        MFCreateAttributes(&m_attributes, 4);
        MFCreateMediaType(&m_mediaType);
        m_mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        m_mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(m_mediaType.Get(), MF_MT_FRAME_SIZE, kFrameWidth, kFrameHeight);
        MFSetAttributeRatio(m_mediaType.Get(), MF_MT_FRAME_RATE, kFrameRate, 1);
        MFSetAttributeRatio(m_mediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        m_mediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

        m_stream = std::make_unique<VirtualCameraStream>(this);
        m_stream->init();
    }

    ~VirtualCameraSource() = default;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        traceLog("source QI " + guidString(riid));
        if (riid == IID_IUnknown || riid == IID_IMFMediaSource || riid == IID_IMFMediaEventGenerator) {
            *ppv = static_cast<IMFMediaSource*>(this);
        } else if (riid == IID_IMFActivate || riid == IID_IMFAttributes) {
            *ppv = static_cast<IMFActivate*>(this);
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }

    STDMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** ppEvent) override {
        return m_events->GetEvent(flags, ppEvent);
    }
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* cb, IUnknown* state) override {
        return m_events->BeginGetEvent(cb, state);
    }
    STDMETHODIMP EndGetEvent(IMFAsyncResult* result, IMFMediaEvent** ppEvent) override {
        return m_events->EndGetEvent(result, ppEvent);
    }
    STDMETHODIMP QueueEvent(MediaEventType type, REFGUID extType, HRESULT hr, const PROPVARIANT* value) override {
        ComPtr<IMFMediaEvent> ev;
        HRESULT h = MFCreateMediaEvent(type, extType, hr, value, &ev);
        if (FAILED(h)) return h;
        return m_events->QueueEvent(ev.Get());
    }

    STDMETHODIMP GetItem(REFGUID key, PROPVARIANT* value) override { return m_attributes->GetItem(key, value); }
    STDMETHODIMP GetItemType(REFGUID key, MF_ATTRIBUTE_TYPE* type) override {
        return m_attributes->GetItemType(key, type);
    }
    STDMETHODIMP CompareItem(REFGUID key, REFPROPVARIANT value, BOOL* result) override {
        return m_attributes->CompareItem(key, value, result);
    }
    STDMETHODIMP Compare(IMFAttributes* theirs, MF_ATTRIBUTES_MATCH_TYPE type, BOOL* result) override {
        return m_attributes->Compare(theirs, type, result);
    }
    STDMETHODIMP GetUINT32(REFGUID key, UINT32* value) override { return m_attributes->GetUINT32(key, value); }
    STDMETHODIMP GetUINT64(REFGUID key, UINT64* value) override { return m_attributes->GetUINT64(key, value); }
    STDMETHODIMP GetDouble(REFGUID key, double* value) override { return m_attributes->GetDouble(key, value); }
    STDMETHODIMP GetGUID(REFGUID key, GUID* value) override { return m_attributes->GetGUID(key, value); }
    STDMETHODIMP GetStringLength(REFGUID key, UINT32* length) override {
        return m_attributes->GetStringLength(key, length);
    }
    STDMETHODIMP GetString(REFGUID key, WCHAR* value, UINT32 size, UINT32* length) override {
        return m_attributes->GetString(key, value, size, length);
    }
    STDMETHODIMP GetAllocatedString(REFGUID key, WCHAR** value, UINT32* length) override {
        return m_attributes->GetAllocatedString(key, value, length);
    }
    STDMETHODIMP GetBlobSize(REFGUID key, UINT32* size) override { return m_attributes->GetBlobSize(key, size); }
    STDMETHODIMP GetBlob(REFGUID key, UINT8* buf, UINT32 size, UINT32* sizeOut) override {
        return m_attributes->GetBlob(key, buf, size, sizeOut);
    }
    STDMETHODIMP GetAllocatedBlob(REFGUID key, UINT8** buf, UINT32* size) override {
        return m_attributes->GetAllocatedBlob(key, buf, size);
    }
    STDMETHODIMP GetUnknown(REFGUID key, REFIID riid, void** ppv) override {
        return m_attributes->GetUnknown(key, riid, ppv);
    }
    STDMETHODIMP SetItem(REFGUID key, REFPROPVARIANT value) override { return m_attributes->SetItem(key, value); }
    STDMETHODIMP DeleteItem(REFGUID key) override { return m_attributes->DeleteItem(key); }
    STDMETHODIMP DeleteAllItems() override { return m_attributes->DeleteAllItems(); }
    STDMETHODIMP SetUINT32(REFGUID key, UINT32 value) override { return m_attributes->SetUINT32(key, value); }
    STDMETHODIMP SetUINT64(REFGUID key, UINT64 value) override { return m_attributes->SetUINT64(key, value); }
    STDMETHODIMP SetDouble(REFGUID key, double value) override { return m_attributes->SetDouble(key, value); }
    STDMETHODIMP SetGUID(REFGUID key, REFGUID value) override { return m_attributes->SetGUID(key, value); }
    STDMETHODIMP SetString(REFGUID key, LPCWSTR value) override { return m_attributes->SetString(key, value); }
    STDMETHODIMP SetBlob(REFGUID key, const UINT8* buf, UINT32 size) override {
        return m_attributes->SetBlob(key, buf, size);
    }
    STDMETHODIMP SetUnknown(REFGUID key, IUnknown* unknown) override { return m_attributes->SetUnknown(key, unknown); }
    STDMETHODIMP LockStore() override { return m_attributes->LockStore(); }
    STDMETHODIMP UnlockStore() override { return m_attributes->UnlockStore(); }
    STDMETHODIMP GetCount(UINT32* count) override { return m_attributes->GetCount(count); }
    STDMETHODIMP GetItemByIndex(UINT32 index, GUID* key, PROPVARIANT* value) override {
        return m_attributes->GetItemByIndex(index, key, value);
    }
    STDMETHODIMP CopyAllItems(IMFAttributes* dest) override { return m_attributes->CopyAllItems(dest); }

    STDMETHODIMP ActivateObject(REFIID riid, void** ppv) override {
        traceLog("ActivateObject " + guidString(riid));
        return QueryInterface(riid, ppv);
    }
    STDMETHODIMP DetachObject() override { return E_NOTIMPL; }
    STDMETHODIMP ShutdownObject() override { return Shutdown(); }

    STDMETHODIMP GetCharacteristics(DWORD* characteristics) override {
        *characteristics = MFMEDIASOURCE_CAN_PAUSE;
        return S_OK;
    }
    STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor) override;
    STDMETHODIMP Start(IMFPresentationDescriptor* pDescriptor, const GUID* pguidTimeFormat,
                       const PROPVARIANT* pvarStartPosition) override;
    STDMETHODIMP Stop() override;
    STDMETHODIMP Pause() override;
    STDMETHODIMP Shutdown() override;

    HRESULT generateSample(IMFMediaEventQueue* streamEvents);
    bool isRunning() const { return m_state == State::Started; }
    bool isShutdown() const { return m_state == State::Shutdown; }
    IMFMediaType* mediaType() const { return m_mediaType.Get(); }
    IMFMediaEventQueue* streamEvents() const { return m_stream->events(); }

private:
    enum class State { Stopped, Started, Paused, Shutdown };

    volatile long m_ref = 1;
    std::mutex m_mutex;
    State m_state = State::Stopped;
    ComPtr<IMFMediaEventQueue> m_events;
    ComPtr<IMFAttributes> m_attributes;
    ComPtr<IMFMediaType> m_mediaType;
    std::unique_ptr<VirtualCameraStream> m_stream;
    uint64_t m_frameCounter = 0;
    long long m_streamStartTime = 0;
};

STDMETHODIMP VirtualCameraStream::QueryInterface(REFIID riid, void** ppv) {
    if (riid == IID_IUnknown || riid == IID_IMFMediaStream || riid == IID_IMFMediaEventGenerator) {
        *ppv = static_cast<IMFMediaStream*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

HRESULT VirtualCameraStream::init() {
    ComPtr<IMFMediaType> mt = m_owner->mediaType();
    ComPtr<IMFStreamDescriptor> sd;
    const HRESULT hr = MFCreateStreamDescriptor(0, 1, mt.GetAddressOf(), &sd);
    if (FAILED(hr)) return hr;
    m_descriptor = sd;
    return S_OK;
}

STDMETHODIMP VirtualCameraStream::GetStreamDescriptor(IMFStreamDescriptor** ppDescriptor) {
    if (m_descriptor) {
        *ppDescriptor = m_descriptor.Get();
        m_descriptor->AddRef();
        return S_OK;
    }
    return MF_E_NOT_INITIALIZED;
}

STDMETHODIMP VirtualCameraStream::GetMediaSource(IMFMediaSource** ppSource) {
    return m_owner->QueryInterface(IID_IMFMediaSource, reinterpret_cast<void**>(ppSource));
}

STDMETHODIMP VirtualCameraStream::RequestSample(IUnknown* token) {
    (void)token;
    if (m_owner->isShutdown() || !m_owner->isRunning()) return MF_E_END_OF_STREAM;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / kFrameRate));
    return m_owner->generateSample(m_events.Get());
}

STDMETHODIMP VirtualCameraSource::CreatePresentationDescriptor(IMFPresentationDescriptor** ppPresentationDescriptor) {
    ComPtr<IMFStreamDescriptor> sd;
    HRESULT hr = m_stream->GetStreamDescriptor(&sd);
    if (FAILED(hr)) return hr;
    ComPtr<IMFPresentationDescriptor> pd;
    hr = MFCreatePresentationDescriptor(1, sd.GetAddressOf(), &pd);
    if (FAILED(hr)) return hr;
    pd->SelectStream(0);
    *ppPresentationDescriptor = pd.Detach();
    return S_OK;
}

STDMETHODIMP VirtualCameraSource::Start(IMFPresentationDescriptor* pDescriptor, const GUID* pguidTimeFormat,
                                        const PROPVARIANT* pvarStartPosition) {
    (void)pDescriptor;
    (void)pvarStartPosition;
    if (pguidTimeFormat && *pguidTimeFormat != GUID_NULL) return MF_E_UNSUPPORTED_TIME_FORMAT;
    std::lock_guard lock(m_mutex);
    if (m_state == State::Shutdown) return MF_E_SHUTDOWN;
    m_state = State::Started;
    m_frameCounter = 0;
    PROPVARIANT start{};
    start.vt = VT_I8;
    start.hVal.QuadPart = 0;
    if (m_stream) queueEvent(m_stream->events(), MEStreamStarted, S_OK, &start);
    return queueEvent(m_events.Get(), MESourceStarted, S_OK, &start);
}

STDMETHODIMP VirtualCameraSource::Stop() {
    std::lock_guard lock(m_mutex);
    if (m_state == State::Shutdown) return MF_E_SHUTDOWN;
    m_state = State::Stopped;
    if (m_stream) queueEvent(m_stream->events(), MEStreamStopped, S_OK, nullptr);
    return queueEvent(m_events.Get(), MESourceStopped, S_OK, nullptr);
}

STDMETHODIMP VirtualCameraSource::Pause() {
    std::lock_guard lock(m_mutex);
    if (m_state == State::Shutdown) return MF_E_SHUTDOWN;
    if (m_state == State::Started) m_state = State::Paused;
    return queueEvent(m_events.Get(), MESourcePaused, S_OK, nullptr);
}

STDMETHODIMP VirtualCameraSource::Shutdown() {
    std::lock_guard lock(m_mutex);
    if (m_state == State::Shutdown) return MF_E_SHUTDOWN;
    m_state = State::Shutdown;
    m_events->Shutdown();
    if (m_stream) m_stream->events()->Shutdown();
    return S_OK;
}

HRESULT VirtualCameraSource::generateSample(IMFMediaEventQueue* streamEvents) {
    std::lock_guard lock(m_mutex);
    if (m_state != State::Started) return MF_E_END_OF_STREAM;

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(kFrameSize, &buffer);
    if (FAILED(hr)) return hr;
    BYTE* data = nullptr;
    hr = buffer->Lock(&data, nullptr, nullptr);
    if (FAILED(hr)) return hr;
    fillTestPattern(data, data + kYPlaneSize, m_frameCounter);
    buffer->Unlock();
    buffer->SetCurrentLength(kFrameSize);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample(&sample);
    if (FAILED(hr)) return hr;
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(m_streamStartTime + static_cast<long long>(m_frameCounter) * kFrameDuration);
    sample->SetSampleDuration(kFrameDuration);
    sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);

    PROPVARIANT value{};
    value.vt = VT_UNKNOWN;
    value.punkVal = sample.Get();
    ++m_frameCounter;
    return queueEvent(streamEvents, MEMediaSample, S_OK, &value);
}

class VirtualCameraFactory final : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* source = new (std::nothrow) VirtualCameraSource();
        if (!source) return E_OUTOFMEMORY;
        const HRESULT hr = source->QueryInterface(riid, ppv);
        source->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL) override { return S_OK; }

private:
    volatile long m_ref = 1;
};

HRESULT registerComClass() {
    wchar_t dllPath[MAX_PATH]{};
    GetModuleFileNameW(GetModuleHandleW(L"virtualcamera.dll"), dllPath, MAX_PATH);
    std::wstring clsidPath = L"Software\\Classes\\CLSID\\{7D8E9F3A-2C4B-4E5F-9A1C-3D2B6E8F4A51}";
    HKEY hKey = nullptr;
    LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsidPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);
    HKEY hServer = nullptr;
    rc = RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &hServer, nullptr);
    if (rc == ERROR_SUCCESS) {
        RegSetValueExW(hServer, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(dllPath),
                       static_cast<DWORD>((wcslen(dllPath) + 1) * sizeof(wchar_t)));
        const wchar_t threading[] = L"Both";
        RegSetValueExW(hServer, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(threading),
                       static_cast<DWORD>(sizeof(threading)));
        RegCloseKey(hServer);
    }
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(rc);
}

} // namespace

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (rclsid != CLSID_HtbVirtualCamera) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) VirtualCameraFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return S_OK;
}

STDAPI DllRegisterServer() {
    return registerComClass();
}

STDAPI DllUnregisterServer() {
    const std::wstring clsidPath = L"Software\\Classes\\CLSID\\{7D8E9F3A-2C4B-4E5F-9A1C-3D2B6E8F4A51}";
    const LONG rc = RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidPath.c_str());
    return (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND) ? S_OK : HRESULT_FROM_WIN32(rc);
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    } else if (reason == DLL_PROCESS_DETACH) {
        MFShutdown();
    }
    return TRUE;
}
