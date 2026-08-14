#pragma once

#include "services/HardwareService.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace htb {

struct UiContext {
    HardwareService& service;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
};

} // namespace htb
