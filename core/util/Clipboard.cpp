#include "core/util/Clipboard.h"

#include <windows.h>

#include <vector>

#include "core/util/Utf.h"

namespace htb {

bool copyToClipboard(const std::string& text) {
    if (!OpenClipboard(nullptr)) return false;
    bool ok = false;

    const std::wstring wide = toWide(text);
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        void* dst = GlobalLock(mem);
        if (dst) {
            memcpy(dst, wide.data(), bytes);
            GlobalUnlock(mem);
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, mem) != nullptr) {
                ok = true;
            } else {
                GlobalFree(mem);
            }
        } else {
            GlobalFree(mem);
        }
    }

    CloseClipboard();
    return ok;
}

} // namespace htb
