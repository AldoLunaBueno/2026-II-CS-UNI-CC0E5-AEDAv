/**
 * @file clipboard.h
 * @ingroup texcel
 * @brief Utilidades de portapapeles para la TUI mediante OSC 52 y herramientas nativas de Linux.
 */

#ifndef __TEXCEL_HELPERS_CLIPBOARD_H__
#define __TEXCEL_HELPERS_CLIPBOARD_H__

#include "../../types.h"
#include "text.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ostream>
#include <string>
#include <string_view>
#include <unistd.h>

/**
 * @namespace TexcelClipboard
 * @brief Codifica texto en Base64 para OSC 52 y lo envía al portapapeles del sistema si hay una utilidad disponible.
 */
namespace TexcelClipboard {

inline std::string base64Encode(const std::string& in) {
    static constexpr char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    uint32_t val = 0;
    int valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

inline bool hasExecutableInPath(std::string_view bin) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;
    std::string_view p(path_env);
    while (!p.empty()) {
        size_t sep = p.find(':');
        std::string_view dir = (sep == std::string_view::npos) ? p : p.substr(0, sep);
        if (!dir.empty()) {
            std::string full(dir);
            full.push_back('/');
            full.append(bin);
            if (::access(full.c_str(), X_OK) == 0) return true;
        }
        if (sep == std::string_view::npos) break;
        p.remove_prefix(sep + 1);
    }
    return false;
}

inline const char* detectNativeClipboardCmd() {
    static const char* cached_cmd = []() -> const char* {
        if (std::getenv("WAYLAND_DISPLAY") && hasExecutableInPath("wl-copy")) {
            return "wl-copy >/dev/null 2>&1";
        }
        if (std::getenv("DISPLAY")) {
            if (hasExecutableInPath("xsel")) return "xsel --clipboard --input >/dev/null 2>&1";
            if (hasExecutableInPath("xclip")) return "xclip -selection clipboard >/dev/null 2>&1";
        }
        if (hasExecutableInPath("clip.exe")) {
            return "clip.exe >/dev/null 2>&1";
        }
        return nullptr;
    }();
    return cached_cmd;
}

inline void copy(std::ostream& os, const std::string& text) {
    if (text.empty()) return;
    texcel_tui::writeOsc52Clipboard(os, base64Encode(text));
    if (const char* cmd = detectNativeClipboardCmd()) {
        if (FILE* p = ::popen(cmd, "w")) {
            std::fwrite(text.data(), 1, text.size(), p);
            ::pclose(p);
        }
    }
}

} // namespace TexcelClipboard

#endif // __TEXCEL_HELPERS_CLIPBOARD_H__
