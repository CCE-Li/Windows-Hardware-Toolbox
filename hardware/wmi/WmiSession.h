#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <wbemidl.h>
#include <windows.h>
#include <wrl/client.h>

namespace htb {

class WmiSession {
public:
    bool connect();
    bool query(const wchar_t* wql, const std::function<bool(IWbemClassObject*)>& onRow);

private:
    Microsoft::WRL::ComPtr<IWbemLocator> m_locator;
    Microsoft::WRL::ComPtr<IWbemServices> m_services;
};

bool readWmiString(IWbemClassObject* obj, const wchar_t* name, std::string& out);
bool readWmiUint64(IWbemClassObject* obj, const wchar_t* name, uint64_t& out);

} // namespace htb
