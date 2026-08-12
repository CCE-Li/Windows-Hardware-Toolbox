#include "core/logging/Logger.h"
#include "core/util/Utf.h"
#include "hardware/wmi/WmiSession.h"

#include <comdef.h>

namespace htb {

bool WmiSession::connect() {
    if (m_services) return true;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_locator));
    if (FAILED(hr)) {
        HTB_ERROR("[wmi] CoCreateInstance(WbemLocator) failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }
    hr = m_locator->ConnectServer(_bstr_t(L"root\\cimv2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &m_services);
    if (FAILED(hr)) {
        HTB_ERROR("[wmi] ConnectServer(root\\cimv2) failed: {:#x}", static_cast<unsigned>(hr));
        return false;
    }
    hr = CoSetProxyBlanket(m_services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        HTB_WARN("[wmi] CoSetProxyBlanket failed: {:#x}", static_cast<unsigned>(hr));
    }
    return true;
}

bool WmiSession::query(const wchar_t* wql, const std::function<bool(IWbemClassObject*)>& onRow) {
    if (!connect()) return false;
    Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
    const HRESULT hr =
        m_services->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql),
                              WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (FAILED(hr)) {
        HTB_WARN("[wmi] ExecQuery failed ({:#08x}): {}", static_cast<unsigned>(hr), toUtf8(wql));
        return false;
    }
    while (true) {
        ULONG returned = 0;
        Microsoft::WRL::ComPtr<IWbemClassObject> obj;
        const HRESULT row = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        if (FAILED(row) || returned == 0) break;
        if (!onRow(obj.Get())) break;
    }
    return true;
}

bool readWmiString(IWbemClassObject* obj, const wchar_t* name, std::string& out) {
    VARIANT v{};
    if (FAILED(obj->Get(name, 0, &v, nullptr, nullptr))) return false;
    bool ok = false;
    if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) {
        out = toUtf8(V_BSTR(&v));
        ok = true;
    }
    VariantClear(&v);
    return ok;
}

bool readWmiUint64(IWbemClassObject* obj, const wchar_t* name, uint64_t& out) {
    VARIANT v{};
    if (FAILED(obj->Get(name, 0, &v, nullptr, nullptr))) return false;
    bool ok = false;
    if (V_VT(&v) == VT_I8) {
        out = static_cast<uint64_t>(V_I8(&v));
        ok = true;
    } else if (V_VT(&v) == VT_UI8) {
        out = V_UI8(&v);
        ok = true;
    } else if (V_VT(&v) == VT_I4) {
        out = static_cast<uint64_t>(V_I4(&v));
        ok = true;
    } else if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) {
        out = _wcstoui64(V_BSTR(&v), nullptr, 10);
        ok = true;
    }
    VariantClear(&v);
    return ok;
}

} // namespace htb
