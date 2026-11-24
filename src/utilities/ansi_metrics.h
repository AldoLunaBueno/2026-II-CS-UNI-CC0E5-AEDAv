/**
 * @file ansi_metrics.h
 * @ingroup visualization
 * @brief Métricas de texto de terminal (ancho visual Unicode 0/1/2 columnas ignorando secuencias ANSI CSI/OSC).
 */

#ifndef __ANSIMETRICS_H__
#define __ANSIMETRICS_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Ansi {

inline constexpr const char* RESET = "\033[0m";

/** @brief Retorna la longitud en bytes de un codepoint UTF-8 a partir de su primer byte. */
inline size_t utf8CharLen(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

/** @brief Decodifica el codepoint UTF-8 que inicia en `s[i]` y retorna `(codepoint, byte_len)`. */
inline std::pair<uint32_t, size_t> decodeUtf8At(std::string_view s, size_t i) {
    unsigned char b0 = static_cast<unsigned char>(s[i]);
    size_t len = std::min(utf8CharLen(b0), s.size() - i);
    if (len == 1) return {b0, 1};
    uint32_t cp = 0;
    if (len == 2)      cp = b0 & 0x1Fu;
    else if (len == 3) cp = b0 & 0x0Fu;
    else               cp = b0 & 0x07u;
    for (size_t k = 1; k < len; ++k) {
        unsigned char bk = static_cast<unsigned char>(s[i + k]);
        if ((bk & 0xC0u) != 0x80u) return {b0, 1};
        cp = (cp << 6) | (bk & 0x3Fu);
    }
    return {cp, len};
}

/**
 * @brief Retorna el ancho visual en celdas de terminal (`0`, `1` o `2`) de un codepoint Unicode.
 *
 * Independiente de `LC_CTYPE` (funciona incluso bajo locale `"C"`), cubriendo marcas combinantes (`0`)
 * y bloques East Asian Wide / Fullwidth / Emojis (`2`, como `📂`, `💾`, `🗑`), preservando ancho `1`
 * para trazos de cajas, bloques, flechas e iconos técnicos (`◆`, `■`, `□`, `✔`, `⚠`, `★`).
 */
inline size_t codepointWidth(uint32_t cp) {
    if (cp == 0 || cp < 0x20u || (cp >= 0x7Fu && cp < 0xA0u)) return 0;
    if ((cp >= 0x0300u && cp <= 0x036Fu) ||
        (cp >= 0x1AB0u && cp <= 0x1AFFu) ||
        (cp >= 0x1DC0u && cp <= 0x1DFFu) ||
        (cp >= 0x200Bu && cp <= 0x200Fu) ||
        (cp >= 0x20D0u && cp <= 0x20FFu) ||
        (cp >= 0xFE00u && cp <= 0xFE0Fu) ||
        (cp >= 0xFE20u && cp <= 0xFE2Fu)) {
        return 0;
    }
    if ((cp >= 0x1100u && cp <= 0x115Fu) ||
        cp == 0x231Au || cp == 0x231Bu ||
        cp == 0x2329u || cp == 0x232Au ||
        (cp >= 0x23E9u && cp <= 0x23F3u) ||
        (cp >= 0x23F8u && cp <= 0x23FAu) ||
        (cp >= 0x2E80u && cp <= 0x303Eu) ||
        (cp >= 0x3040u && cp <= 0x33BFu) ||
        (cp >= 0x3400u && cp <= 0x4DBFu) ||
        (cp >= 0x4E00u && cp <= 0xA4CFu) ||
        (cp >= 0xA960u && cp <= 0xA97Fu) ||
        (cp >= 0xAC00u && cp <= 0xD7A3u) ||
        (cp >= 0xF900u && cp <= 0xFAFFu) ||
        (cp >= 0xFE10u && cp <= 0xFE19u) ||
        (cp >= 0xFE30u && cp <= 0xFE6Fu) ||
        (cp >= 0xFF01u && cp <= 0xFF60u) ||
        (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
        (cp >= 0x1F000u && cp <= 0x1FAFFu) ||
        (cp >= 0x20000u && cp <= 0x3FFFDu)) {
        return 2;
    }
    return 1;
}

/** @brief Avanza `i` sobre una secuencia de escape ANSI CSI, OSC o ESC de 2 bytes si `s[i]` es ESC. */
inline bool skipCsiSeq(std::string_view s, size_t& i) {
    if (i >= s.size() || (s[i] != '\033' && s[i] != '\x1B')) return false;
    ++i;
    if (i >= s.size()) return true;
    if (s[i] == '[') {
        ++i;
        while (i < s.size() && (s[i] < '@' || s[i] > '~')) ++i;
        if (i < s.size()) ++i;
    } else if (s[i] == ']') {
        ++i;
        while (i < s.size()) {
            if (s[i] == '\a') { ++i; break; }
            if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '\\') { i += 2; break; }
            ++i;
        }
    } else if (s[i] >= '@' && s[i] <= '_') {
        ++i;
    }
    return true;
}

/**
 * @brief Calcula el ancho visual real en columnas de terminal ignorando secuencias ANSI CSI/OSC.
 */
inline size_t visible_length(std::string_view s) {
    size_t cols = 0, i = 0;
    while (i < s.size()) {
        if (skipCsiSeq(s, i)) continue;
        auto [cp, len] = decodeUtf8At(s, i);
        cols += codepointWidth(cp);
        i += len;
    }
    return cols;
}

/** @brief Extrae la subcadena correspondiente al intervalo de columnas visuales `[col_from, col_to)`. */
inline std::string utf8Slice(std::string_view str, size_t col_from, size_t col_to) {
    if (col_from >= col_to) return {};
    std::string res;
    size_t col = 0, i = 0;
    while (i < str.size() && col < col_to) {
        auto [cp, len] = decodeUtf8At(str, i);
        size_t w = codepointWidth(cp);
        if (w == 0) {
            if (col >= col_from && !res.empty()) res.append(str.substr(i, len));
            i += len;
            continue;
        }
        if (col + w <= col_from) {
            col += w;
            i += len;
            continue;
        }
        if (col < col_from) {
            // Glifo de ancho 2 partido a la izquierda: rellenar la mitad derecha con espacio
            res.push_back(' ');
            col += w;
            i += len;
            continue;
        }
        if (col + w > col_to) {
            // Glifo de ancho 2 que no cabe en la última columna: rellenar con espacio
            res.append(col_to - col, ' ');
            break;
        }
        res.append(str.substr(i, len));
        col += w;
        i += len;
    }
    return res;
}

/** @brief Obtiene el glifo UTF-8 en la columna visual `col_idx` (0-indexada). */
inline std::string utf8At(std::string_view str, size_t col_idx) {
    size_t col = 0, i = 0;
    while (i < str.size()) {
        auto [cp, len] = decodeUtf8At(str, i);
        size_t w = codepointWidth(cp);
        if (w > 0) {
            if (col_idx >= col && col_idx < col + w) {
                return (col_idx == col) ? std::string(str.substr(i, len)) : std::string(" ");
            }
            col += w;
        }
        i += len;
    }
    return {};
}

/** @brief Elimina secuencias de escape ANSI CSI/OSC dejando solo texto plano. */
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
    out.reserve(s.size() + 8);
    size_t cols = 0, i = 0;
    bool had_ansi = false;
    while (i < s.size() && cols < width) {
        size_t start = i;
        if (skipCsiSeq(s, i)) {
            out.append(s.substr(start, i - start));
            had_ansi = true;
            continue;
        }
        auto [cp, len] = decodeUtf8At(s, i);
        size_t w = codepointWidth(cp);
        if (cols + w > width) break;
        out.append(s.substr(i, len));
        i += len;
        cols += w;
    }
    if (had_ansi) out += RESET;
    if (cols < width) out.append(width - cols, ' ');
    return out;
}

} // namespace Ansi

using Ansi::visible_length;

#endif // __ANSIMETRICS_H__
