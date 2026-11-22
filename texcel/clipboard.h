/**
 * @file clipboard.h
 * @brief Utilidades de portapapeles (OSC 52 y herramientas nativas de Linux).
 */

#ifndef __TEXCEL_CLIPBOARD_H__
#define __TEXCEL_CLIPBOARD_H__

#include "../types.h"
#include <cstdio>
#include <ostream>
#include <string>

/**
 * @namespace TexcelClipboard
 * @brief Codifica texto en Base64 para OSC 52 y copia al portapapeles del sistema si hay utilidades disponibles.
 */
namespace TexcelClipboard {

inline std::string base64Encode(const std::string& in) {
    static constexpr char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    TX val = 0, valb = -6;
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

inline void copy(std::ostream& os, const std::string& text) {
    if (text.empty()) return;
    os << "\033]52;c;" << base64Encode(text) << "\a" << std::flush;
    const char* cmds[] = {
        "command -v wl-copy >/dev/null 2>&1 && wl-copy >/dev/null 2>&1",
        "command -v xsel >/dev/null 2>&1 && xsel --clipboard --input >/dev/null 2>&1",
        "command -v xclip >/dev/null 2>&1 && xclip -selection clipboard >/dev/null 2>&1"
    };
    for (const char* cmd : cmds) {
        if (FILE* p = popen(cmd, "w")) {
            std::fwrite(text.data(), 1, text.size(), p);
            if (pclose(p) == 0) break;
        }
    }
}

} // namespace TexcelClipboard

#endif // __TEXCEL_CLIPBOARD_H__
