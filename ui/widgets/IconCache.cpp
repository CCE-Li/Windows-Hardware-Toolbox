#include "ui/widgets/IconCache.h"

#include <d3d11.h>

namespace htb {

namespace {
constexpr size_t kMaxCachedIcons = 512;
}

IconCache::~IconCache() {
    clear();
}

void IconCache::update(const std::vector<ProcessIcon>& icons, void* d3dDevice) {
    auto* device = static_cast<ID3D11Device*>(d3dDevice);
    if (!device) return;

    for (const ProcessIcon& icon : icons) {
        if (!icon.available || icon.bgra.empty() || icon.width <= 0 || icon.height <= 0) continue;
        if (m_textures.contains(icon.name)) continue;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(icon.width);
        desc.Height = static_cast<UINT>(icon.height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = icon.bgra.data();
        init.SysMemPitch = static_cast<UINT>(icon.width * 4);

        ID3D11Texture2D* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        if (SUCCEEDED(device->CreateTexture2D(&desc, &init, &tex)) && tex &&
            SUCCEEDED(device->CreateShaderResourceView(tex, nullptr, &srv))) {
            tex->Release();
            m_textures.emplace(icon.name, srv);
        } else {
            if (tex) tex->Release();
        }
    }

    if (m_textures.size() > kMaxCachedIcons) {
        auto it = m_textures.begin();
        while (m_textures.size() > kMaxCachedIcons && it != m_textures.end()) {
            static_cast<ID3D11ShaderResourceView*>(it->second)->Release();
            it = m_textures.erase(it);
        }
    }
}

ImTextureID IconCache::texture(const std::string& name) const {
    const auto it = m_textures.find(name);
    if (it == m_textures.end()) return 0;
    return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(it->second));
}

void IconCache::clear() {
    for (auto& [name, srv] : m_textures) {
        (void)name;
        static_cast<ID3D11ShaderResourceView*>(srv)->Release();
    }
    m_textures.clear();
}

} // namespace htb