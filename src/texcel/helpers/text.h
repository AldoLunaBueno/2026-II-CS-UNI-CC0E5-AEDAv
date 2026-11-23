/**
 * @file text.h
 * @ingroup texcel
 * @brief Utilidades puras de texto UTF-8, recorte ANSI y normalización de rangos para la TUI de Texcel.
 */

#ifndef __TEXCEL_HELPERS_TEXT_H__
#define __TEXCEL_HELPERS_TEXT_H__

#include "../../types.h"
#include "../../utilities/ansi_metrics.h"
#include "../../utilities/gotoxy.h"
#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace texcel_tui {

using Ansi::utf8CharLen;
using Ansi::skipCsiSeq;
using Ansi::utf8Slice;
using Ansi::utf8At;
using Ansi::stripAnsi;
using Ansi::fitWidth;

// Secuencias de control de sesión TUI, ratón SGR, bracketed paste y portapapeles OSC 52
inline constexpr const char* BRACKETED_PASTE_END = "\033[201~";
inline constexpr const char* MOUSE_SGR_ON        = "\033[?2004h\033[?1003l\033[?1000h\033[?1002h\033[?1006h";
inline constexpr const char* MOUSE_SGR_OFF       = "\033[?2004l\033[?1006l\033[?1003l\033[?1002l\033[?1000l";
inline constexpr const char* OSC52_PREFIX        = "\033]52;c;";
inline constexpr const char* OSC52_SUFFIX        = "\a";

/** @brief Activa el tracking de ratón SGR y bracketed paste (también re-arma el modo tras pulsar Esc). */
inline void enableMouseTracking(std::ostream& os) {
    os << MOUSE_SGR_ON << std::flush;
}

/** @brief Prepara la terminal para la TUI: pantalla alternativa, sin autowrap, cursor oculto en `(1,1)` y ratón SGR. */
inline void enterInteractiveTerminal(std::ostream& os) {
    os << Ansi::ALT_SCREEN_ON << Ansi::AUTOWRAP_OFF << Ansi::CURSOR_HOME
       << Ansi::CURSOR_SAVE << Ansi::CURSOR_HIDE;
    enableMouseTracking(os);
}

/** @brief Restaura la terminal original: desactiva ratón SGR/paste, reactiva autowrap, muestra cursor y sale de pantalla alternativa. */
inline void exitInteractiveTerminal(std::ostream& os) {
    os << MOUSE_SGR_OFF << Ansi::AUTOWRAP_ON
       << Ansi::CURSOR_SHOW << Ansi::ALT_SCREEN_OFF << std::flush;
}

/** @brief Emite la secuencia OSC 52 para enviar texto codificado en Base64 al portapapeles de la terminal. */
inline void writeOsc52Clipboard(std::ostream& os, std::string_view base64_text) {
    os << OSC52_PREFIX << base64_text << OSC52_SUFFIX << std::flush;
}

/** @brief Indica si un codepoint UTF-8 es una barra o trazo de división (`│`, `─`, `╭`, `╮`, `╰`, `╯`, `┼`, `┴`, `┬`, `├`, `┤`). */
inline bool isDividerGlyph(const std::string& glyph) {
    return glyph == "│" || glyph == "─" || glyph == "╭" || glyph == "╮" ||
           glyph == "╰" || glyph == "╯" || glyph == "┼" || glyph == "┴" ||
           glyph == "┬" || glyph == "├" || glyph == "┤" || glyph == "╱" || glyph == "╲";
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

/** @brief Repite un glifo UTF-8 `count` veces. */
inline std::string repeatGlyph(const std::string& glyph, size_t count) {
    std::string out;
    out.reserve(glyph.size() * count);
    for (size_t i = 0; i < count; ++i) out += glyph;
    return out;
}

/** @brief Construye un encabezado de sección `─── <title> ────...` ajustado a `width` columnas visibles. */
inline std::string makeSectionHeader(const std::string& title, size_t width) {
    std::string s = "─── " + title + " ";
    size_t v = visible_length(s);
    if (v < width) s += repeatGlyph("─", width - v);
    return s;
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
