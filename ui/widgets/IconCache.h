#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "hardware/process/ProcessProvider.h"
#include "imgui.h"

namespace htb {

// Converts ProcessIcon BGRA blobs from the provider into cached D3D11 texture
// handles for rendering (one texture per process image name).
class IconCache {
public:
    IconCache() = default;
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    void update(const std::vector<ProcessIcon>& icons, void* d3dDevice);
    ImTextureID texture(const std::string& name) const;
    void clear();

private:
    std::unordered_map<std::string, void*> m_textures;
};

} // namespace htb