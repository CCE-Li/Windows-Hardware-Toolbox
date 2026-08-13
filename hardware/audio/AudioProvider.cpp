#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/audio/AudioProvider.h"

#include <endpointvolume.h>
#include <initguid.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <windows.h>
#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace htb {

namespace {
ComPtr<IAudioEndpointVolume> defaultRenderVolume() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))))
        return {};
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) return {};
    ComPtr<IAudioEndpointVolume> volume;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &volume))) return {};
    return volume;
}
} // namespace

namespace {
std::string stateName(DWORD state) {
    switch (state) {
        case DEVICE_STATE_ACTIVE: return "活动";
        case DEVICE_STATE_DISABLED: return "已禁用";
        case DEVICE_STATE_NOTPRESENT: return "不存在";
        case DEVICE_STATE_UNPLUGGED: return "未插接";
        default: return "未知";
    }
}

std::string formatName(WORD formatTag) {
    switch (formatTag) {
        case WAVE_FORMAT_PCM: return "PCM";
        case WAVE_FORMAT_IEEE_FLOAT: return "浮点";
        case WAVE_FORMAT_EXTENSIBLE: return "可扩展";
        default: return "未知";
    }
}

bool readFormat(IPropertyStore* store, uint32_t& sampleRate, uint32_t& channels, std::string& format) {
    PROPVARIANT var{};
    HRESULT hr = store->GetValue(PKEY_AudioEngine_DeviceFormat, &var);
    if (FAILED(hr)) {
        PropVariantClear(&var);
        return false;
    }
    bool ok = false;
    if (var.vt == VT_BLOB && var.blob.pBlobData && var.blob.cbSize >= sizeof(WAVEFORMATEX)) {
        const auto* wf = reinterpret_cast<const WAVEFORMATEX*>(var.blob.pBlobData);
        sampleRate = wf->nSamplesPerSec;
        channels = wf->nChannels;
        format = formatName(wf->wFormatTag);
        ok = true;
    }
    PropVariantClear(&var);
    return ok;
}

bool readString(IPropertyStore* store, REFPROPERTYKEY key, std::string& out) {
    PROPVARIANT var{};
    HRESULT hr = store->GetValue(key, &var);
    if (FAILED(hr) || var.vt != VT_LPWSTR || !var.pwszVal) {
        PropVariantClear(&var);
        return false;
    }
    out = toUtf8(var.pwszVal);
    PropVariantClear(&var);
    return true;
}
} // namespace

struct AudioProvider::Impl {
    std::unique_ptr<std::thread> opThread;
    std::atomic<bool> opRunning{false};
};

AudioProvider::AudioProvider() = default;

AudioProvider::~AudioProvider() {
    if (m_impl && m_impl->opThread && m_impl->opThread->joinable()) m_impl->opThread->join();
}

void AudioProvider::refreshVolume() {
    auto volume = std::make_shared<VolumeState>();
    ComPtr<IAudioEndpointVolume> vol = defaultRenderVolume();
    if (!vol) {
        m_volume.store(std::move(volume));
        return;
    }
    float level = 0.0f;
    BOOL muted = FALSE;
    if (SUCCEEDED(vol->GetMasterVolumeLevelScalar(&level)) && SUCCEEDED(vol->GetMute(&muted))) {
        volume->availability = Availability::Available;
        volume->level = level;
        volume->muted = muted != FALSE;
        volume->source = "IAudioEndpointVolume";
        HTB_DEBUG("[audio] master volume {:.0f}%, muted {}", level * 100.0f, volume->muted);
    }
    m_volume.store(std::move(volume));
}

void AudioProvider::setVolumeAsync(float level, bool muted) {
    if (m_impl->opRunning.exchange(true)) return;
    m_impl->opThread = std::make_unique<std::thread>([this, level, muted] {
        ComPtr<IAudioEndpointVolume> vol = defaultRenderVolume();
        if (vol) {
            if (SUCCEEDED(vol->SetMasterVolumeLevelScalar(level, nullptr))) {
                HTB_INFO("[audio] master volume set to {:.0f}%", level * 100.0f);
            }
            vol->SetMute(muted ? TRUE : FALSE, nullptr);
        }
        m_impl->opRunning.store(false);
    });
}

void AudioProvider::refresh() {
    auto endpoints = std::make_shared<std::vector<AudioEndpoint>>();

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) {
        HTB_WARN("[audio] MMDeviceEnumerator create failed: {:#x}", static_cast<unsigned>(hr));
        m_snapshot.store(std::move(endpoints));
        return;
    }

    const struct {
        EDataFlow flow;
        const wchar_t* label;
    } directions[] = {{eRender, L"播放"}, {eCapture, L"录制"}};

    for (const auto& dir : directions) {
        ComPtr<IMMDevice> defaultDevice;
        enumerator->GetDefaultAudioEndpoint(dir.flow, eConsole, &defaultDevice);

        ComPtr<IMMDeviceCollection> collection;
        hr = enumerator->EnumAudioEndpoints(dir.flow, DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_UNPLUGGED,
                                            &collection);
        if (FAILED(hr)) continue;
        UINT count = 0;
        collection->GetCount(&count);

        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(collection->Item(i, &device))) continue;

            AudioEndpoint endpoint;
            endpoint.direction = toUtf8(dir.label);

            LPWSTR id = nullptr;
            if (SUCCEEDED(device->GetId(&id)) && id) {
                endpoint.id = toUtf8(id);
                CoTaskMemFree(id);
            }
            DWORD state = 0;
            if (SUCCEEDED(device->GetState(&state))) endpoint.state = stateName(state);
            if (defaultDevice && defaultDevice.Get() == device.Get()) endpoint.isDefault = true;

            ComPtr<IPropertyStore> store;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
                readString(store.Get(), PKEY_Device_FriendlyName, endpoint.name);
                readString(store.Get(), PKEY_Device_DeviceDesc, endpoint.description);
                readFormat(store.Get(), endpoint.sampleRate, endpoint.channels, endpoint.format);
            }
            if (endpoint.name.empty()) endpoint.name = endpoint.description.empty() ? endpoint.id : endpoint.description;
            endpoint.source = "MMDevice API (Core Audio)";
            endpoints->push_back(std::move(endpoint));
        }
    }

    HTB_INFO("[audio] {} endpoint(s) enumerated", endpoints->size());
    m_snapshot.store(std::move(endpoints));
    refreshVolume();
}

} // namespace htb
