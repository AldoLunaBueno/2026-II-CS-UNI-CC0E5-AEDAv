/**
 * @file ansi_metrics.h
 * @ingroup visualization
 * @brief Métricas de texto de terminal (ancho visual UTF-8 ignorando secuencias de escape ANSI).
 */

#ifndef __ANSIMETRICS_H__
#define __ANSIMETRICS_H__

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace Ansi {

inline constexpr const char* RESET = "\033[0m";

/** @brief Retorna la longitud en bytes de un codepoint UTF-8 a partir de su primer byte. */
inline size_t utf8CharLen(unsigned char c) {
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/** @brief Avanza `i` sobre una secuencia de escape ANSI CSI/ESC si `s[i]` es ESC. */
inline bool skipCsiSeq(std::string_view s, size_t& i) {
    if (s[i] != '\033' && s[i] != '\x1B') return false;
    ++i;
    if (i < s.size() && s[i] == '[') {
        ++i;
        while (i < s.size() && (s[i] < '@' || s[i] > '~')) ++i;
        if (i < s.size()) ++i;
    }
    return true;
}

/**
 * @brief Calcula el ancho visual en columnas de terminal ignorando secuencias ANSI y contando codepoints UTF-8.
 * @param s Cadena de entrada que puede contener códigos de escape ANSI SGR y caracteres multibyte UTF-8.
 * @return Cantidad de caracteres visibles en pantalla.
 */
inline size_t visible_length(std::string_view s) {
    size_t count = 0, i = 0;
    while (i < s.size()) {
        if (skipCsiSeq(s, i)) continue;
        i += std::min(utf8CharLen(static_cast<unsigned char>(s[i])), s.size() - i);
        ++count;
    }
    return count;
}

/** @brief Extrae la subcadena correspondiente a los codepoints `[cp_from, cp_to)`. */
inline std::string utf8Slice(std::string_view str, size_t cp_from, size_t cp_to) {
    std::string res;
    size_t cp = 0, i = 0;
    while (i < str.size()) {
        size_t len = std::min(utf8CharLen(static_cast<unsigned char>(str[i])), str.size() - i);
        if (cp >= cp_from && cp < cp_to) res.append(str.substr(i, len));
        i += len;
        ++cp;
    }
    return res;
}

/** @brief Obtiene el codepoint UTF-8 en la columna visual `cp_idx` (0-indexada). */
inline std::string utf8At(std::string_view str, size_t cp_idx) {
    return utf8Slice(str, cp_idx, cp_idx + 1);
}

/** @brief Elimina secuencias de escape ANSI CSI/SGR dejando solo texto plano. */
inline std::string stripAnsi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (skipCsiSeq(s, i)) continue;
        out.push_back(s[i++]);
    }
    return out;
}

/** @brief Ajusta (recorta o rellena) una cadena con o sin ANSI a exactamente `width` columnas visibles. */
inline std::string fitWidth(std::string_view s, size_t width) {
    size_t vlen = visible_length(s);
    if (vlen < width) return std::string(s) + std::string(width - vlen, ' ');
    if (vlen == width) return std::string(s);

    std::string out;
    out.reserve(s.size());
    size_t count = 0, i = 0;
    bool had_ansi = false;
    while (i < s.size() && count < width) {
        size_t start = i;
        if (skipCsiSeq(s, i)) {
            out.append(s.substr(start, i - start));
            had_ansi = true;
            continue;
        }
        size_t len = std::min(utf8CharLen(static_cast<unsigned char>(s[i])), s.size() - i);
        out.append(s.substr(i, len));
        i += len;
        ++count;
    }
    if (had_ansi) out += RESET;
    if (count < width) out.append(width - count, ' ');
    return out;
}

} // namespace Ansi

using Ansi::visible_length;

#endif // __ANSIMETRICS_H__
