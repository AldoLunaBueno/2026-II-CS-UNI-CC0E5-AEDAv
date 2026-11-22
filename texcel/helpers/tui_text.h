/**
 * @file tui_text.h
 * @ingroup texcel
 * @brief Utilidades puras de texto UTF-8, recorte ANSI y normalización de rangos para la TUI de Texcel.
 */

#ifndef __TEXCEL_HELPERS_TUI_TEXT_H__
#define __TEXCEL_HELPERS_TUI_TEXT_H__

#include "../../types.h"
#include "../../utilities/ansi_metrics.h"
#include <algorithm>
#include <string>
#include <utility>

namespace texcel_tui {

/** @brief Retorna la longitud en bytes de un codepoint UTF-8 a partir de su primer byte. */
inline size_t utf8CharLen(unsigned char c) {
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/** @brief Extrae la subcadena correspondiente a los codepoints `[cp_from, cp_to)`. */
inline std::string utf8Slice(const std::string& str, size_t cp_from, size_t cp_to) {
    std::string res;
    size_t cp = 0, i = 0;
    while (i < str.size()) {
        size_t len = std::min(utf8CharLen(static_cast<unsigned char>(str[i])), str.size() - i);
        if (cp >= cp_from && cp < cp_to) res.append(str, i, len);
        i += len;
        ++cp;
    }
    return res;
}

/** @brief Indica si un codepoint UTF-8 es una barra o trazo de división (`│`, `─`, `╭`, `╮`, `╰`, `╯`, `┼`, `┴`, `┬`, `├`, `┤`). */
inline bool isDividerGlyph(const std::string& glyph) {
    return glyph == "│" || glyph == "─" || glyph == "╭" || glyph == "╮" ||
           glyph == "╰" || glyph == "╯" || glyph == "┼" || glyph == "┴" ||
           glyph == "┬" || glyph == "├" || glyph == "┤" || glyph == "╱" || glyph == "╲";
}

/** @brief Obtiene el codepoint UTF-8 en la columna visual `cp_idx` (0-indexada). */
inline std::string utf8At(const std::string& str, size_t cp_idx) {
    return utf8Slice(str, cp_idx, cp_idx + 1);
}

/** @brief Filtra barras de división de un fragmento UTF-8 al copiar texto de una ventana. */
inline std::string stripDividerGlyphs(const std::string& str) {
    std::string out;
    size_t i = 0;
    while (i < str.size()) {
        size_t len = std::min(utf8CharLen(static_cast<unsigned char>(str[i])), str.size() - i);
        std::string g = str.substr(i, len);
        if (!isDividerGlyph(g)) out += g;
        i += len;
    }
    return out;
}

/** @brief Elimina secuencias de escape ANSI CSI/SGR dejando solo texto plano. */
inline std::string stripAnsi(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033' || s[i] == '\x1B') {
            if (i + 1 < s.size() && s[i + 1] == '[') {
                i += 2;
                while (i < s.size() && (s[i] < '@' || s[i] > '~')) ++i;
            }
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

/** @brief Ajusta (recorta o rellena) una cadena con o sin ANSI a exactamente `width` columnas visibles. */
inline std::string fitWidth(const std::string& s, size_t width) {
    size_t vlen = visible_length(s);
    if (vlen < width) return s + std::string(width - vlen, ' ');
    if (vlen == width) return s;

    std::string out;
    out.reserve(s.size());
    size_t count = 0, i = 0;
    bool had_ansi = false;
    while (i < s.size() && count < width) {
        if (s[i] == '\033' || s[i] == '\x1B') {
            size_t start = i++;
            if (i < s.size() && s[i] == '[') {
                ++i;
                while (i < s.size() && (s[i] < '@' || s[i] > '~')) ++i;
                if (i < s.size()) ++i;
            }
            out.append(s, start, i - start);
            had_ansi = true;
            continue;
        }
        size_t len = std::min(utf8CharLen(static_cast<unsigned char>(s[i])), s.size() - i);
        out.append(s, i, len);
        i += len;
        ++count;
    }
    if (had_ansi) out += "\033[0m";
    if (count < width) out.append(width - count, ' ');
    return out;
}

/** @brief Trunca texto plano a `max_len` bytes añadiendo `~` si se recorta. */
inline std::string clipPlain(const std::string& s, size_t max_len) {
    if (s.size() <= max_len) return s;
    if (max_len <= 2) return s.substr(0, max_len);
    return s.substr(0, max_len - 1) + "~";
}

/** @brief Alinea (derecha o centrado) un texto plano en una celda de ancho visual `w`. */
inline std::string alignCell(const std::string& text, size_t w, bool right_align) {
    std::string clipped = clipPlain(text, w - 2);
    size_t vlen = visible_length(clipped);
    size_t pad = (w - 2 > vlen) ? (w - 2 - vlen) : 0;
    if (right_align) {
        return " " + std::string(pad, ' ') + clipped + " ";
    }
    size_t left_pad = pad / 2;
    size_t right_pad = pad - left_pad;
    return " " + std::string(left_pad, ' ') + clipped + std::string(right_pad, ' ') + " ";
}

/** @brief Normaliza un rectángulo de celdas para que `cmin <= cmax` y `rmin <= rmax`. */
inline void normalizeCellRange(TX c0, TX r0, TX c1, TX r1,
                               TX& cmin, TX& rmin, TX& cmax, TX& rmax) {
    cmin = std::min(c0, c1);
    cmax = std::max(c0, c1);
    rmin = std::min(r0, r1);
    rmax = std::max(r0, r1);
}

/** @brief Normaliza una selección lineal de texto para que `(sy0, sx0) <= (sy1, sx1)`. */
inline void normalizeTextRange(TX x0, TX y0, TX x1, TX y1,
                               TX& sx0, TX& sy0, TX& sx1, TX& sy1) {
    sx0 = x0; sy0 = y0; sx1 = x1; sy1 = y1;
    if (sy0 > sy1 || (sy0 == sy1 && sx0 > sx1)) {
        std::swap(sy0, sy1);
        std::swap(sx0, sx1);
    }
}

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_TUI_TEXT_H__
