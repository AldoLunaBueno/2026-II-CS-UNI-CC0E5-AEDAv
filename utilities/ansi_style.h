#ifndef __ANSISTYLE_H__
#define __ANSISTYLE_H__

#include <string>
#include <string_view>
#include <sstream>
#include <cstdint>
#include <type_traits>
#include <iostream>

namespace Ansi {

// Atributos y estilos tipográficos (SGR - Select Graphic Rendition)
enum class Style : uint8_t {
    Reset = 0, Bold = 1, Dim = 2, Italic = 3, Underline = 4, Blink = 5,
    RapidBlink = 6, Reverse = 7, Hidden = 8, Strike = 9, DoubleUnderline = 21, Overline = 53
};

// Colores de texto (Foreground) estándar y brillantes (Bright)
enum class Color : uint8_t {
    Reset = 0, Black = 30, Red = 31, Green = 32, Yellow = 33, Blue = 34, Magenta = 35, Cyan = 36, White = 37, Default = 39,
    BrightBlack = 90, BrightRed = 91, BrightGreen = 92, BrightYellow = 93, BrightBlue = 94, BrightMagenta = 95, BrightCyan = 96, BrightWhite = 97
};

// Colores de fondo (Background) estándar y brillantes (Bright)
enum class BgColor : uint8_t {
    Default = 49, Black = 40, Red = 41, Green = 42, Yellow = 43, Blue = 44, Magenta = 45, Cyan = 46, White = 47,
    BrightBlack = 100, BrightRed = 101, BrightGreen = 102, BrightYellow = 103, BrightBlue = 104, BrightMagenta = 105, BrightCyan = 106, BrightWhite = 107
};

// Paleta indexada de 8 bits (256 colores) y Truecolor de 24 bits (RGB)
struct Color8Val { uint8_t code = 0; bool is_bg = false; };
struct RgbColor  { uint8_t r = 0, g = 0, b = 0; bool is_bg = false; };

// Parser hexadecimal (#RRGGBB o RRGGBB)
inline RgbColor parse_hex(std::string_view h, bool bg = false) {
    if (!h.empty() && h[0] == '#') h.remove_prefix(1);
    uint32_t v = 0;
    for (char c : h) v = (v << 4) | (c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
    return {static_cast<uint8_t>((v >> 16) & 0xFF), static_cast<uint8_t>((v >> 8) & 0xFF), static_cast<uint8_t>(v & 0xFF), bg};
}

// Constructores y constantes de color para Foreground (Fg)
namespace Fg {
    using enum Color;
    constexpr Color Gray = Color::BrightBlack;
    constexpr Color8Val Color8(uint8_t c) { return {c, false}; }
    constexpr Color8Val color8(uint8_t c) { return {c, false}; }
    constexpr RgbColor  Rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, false}; }
    constexpr RgbColor  rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, false}; }
    inline RgbColor     Hex(std::string_view h) { return parse_hex(h, false); }
    inline RgbColor     hex(std::string_view h) { return parse_hex(h, false); }
}

// Constructores y constantes de color para Background (Bg)
namespace Bg {
    using enum BgColor;
    constexpr BgColor Gray = BgColor::BrightBlack;
    constexpr Color8Val Color8(uint8_t c) { return {c, true}; }
    constexpr Color8Val color8(uint8_t c) { return {c, true}; }
    constexpr RgbColor  Rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, true}; }
    constexpr RgbColor  rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, true}; }
    inline RgbColor     Hex(std::string_view h) { return parse_hex(h, true); }
    inline RgbColor     hex(std::string_view h) { return parse_hex(h, true); }
}

// Conversión a código SGR ANSI
template <typename E> requires std::is_enum_v<E>
inline std::string to_sgr_code(E e) { return std::to_string(static_cast<int>(e)); }
inline std::string to_sgr_code(const Color8Val& c) { return (c.is_bg ? "48;5;" : "38;5;") + std::to_string(c.code); }
inline std::string to_sgr_code(const RgbColor& c) {
    return (c.is_bg ? "48;2;" : "38;2;") + std::to_string(c.r) + ";" + std::to_string(c.g) + ";" + std::to_string(c.b);
}

// Ancho visual en terminal ignorando secuencias ANSI y codepoints UTF-8
inline size_t visible_length(std::string_view s) {
    size_t count = 0, i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' || s[i] == '\x1B') {
            i++;
            if (i < s.size() && s[i] == '[') {
                i++;
                while (i < s.size() && (s[i] < '@' || s[i] > '~')) i++;
                if (i < s.size()) i++;
            }
            continue;
        }
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0x80) == 0) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        count++;
    }
    return count;
}

// Fluent Builder: styled(value)...
class StyledBuilder {
private:
    std::string m_text;
    std::string m_sgr;

public:
    template <typename T>
    explicit StyledBuilder(const T& val) {
        if constexpr (std::is_convertible_v<T, std::string>) m_text = val;
        else { std::ostringstream oss; oss << val; m_text = oss.str(); }
    }

    StyledBuilder& append(std::string_view code) {
        if (!m_sgr.empty()) m_sgr += ';';
        m_sgr += code;
        return *this;
    }

    // Estilos tipográficos
    StyledBuilder& bold()             { return append("1"); }     StyledBuilder& dim()        { return append("2"); }
    StyledBuilder& italic()           { return append("3"); }     StyledBuilder& underline()  { return append("4"); }
    StyledBuilder& blink()            { return append("5"); }     StyledBuilder& rapid_blink(){ return append("6"); }
    StyledBuilder& reverse()          { return append("7"); }     StyledBuilder& hidden()     { return append("8"); }
    StyledBuilder& strike()           { return append("9"); }     StyledBuilder& overline()   { return append("53"); }
    StyledBuilder& double_underline() { return append("21"); }

    // Colores de texto (FG)
    StyledBuilder& black()   { return append("30"); }   StyledBuilder& red()     { return append("31"); }
    StyledBuilder& green()   { return append("32"); }   StyledBuilder& yellow()  { return append("33"); }
    StyledBuilder& blue()    { return append("34"); }   StyledBuilder& magenta() { return append("35"); }
    StyledBuilder& cyan()    { return append("36"); }   StyledBuilder& white()   { return append("37"); }
    StyledBuilder& default_color() { return append("39"); }
    StyledBuilder& pure_black()    { return rgb(0, 0, 0); }

    // Colores de texto brillantes (Bright FG)
    StyledBuilder& gray()           { return append("90"); }   StyledBuilder& bright_black()   { return append("90"); }
    StyledBuilder& bright_red()     { return append("91"); }   StyledBuilder& bright_green()   { return append("92"); }
    StyledBuilder& bright_yellow()  { return append("93"); }   StyledBuilder& bright_blue()    { return append("94"); }
    StyledBuilder& bright_magenta() { return append("95"); }   StyledBuilder& bright_cyan()    { return append("96"); }
    StyledBuilder& bright_white()   { return append("97"); }

    // Colores de fondo (BG)
    StyledBuilder& bg_black()   { return append("40"); }   StyledBuilder& bg_red()     { return append("41"); }
    StyledBuilder& bg_green()   { return append("42"); }   StyledBuilder& bg_yellow()  { return append("43"); }
    StyledBuilder& bg_blue()    { return append("44"); }   StyledBuilder& bg_magenta() { return append("45"); }
    StyledBuilder& bg_cyan()    { return append("46"); }   StyledBuilder& bg_white()   { return append("47"); }
    StyledBuilder& bg_default_color() { return append("49"); }
    StyledBuilder& bg_pure_black()    { return bg_rgb(0, 0, 0); }

    // Colores de fondo brillantes (Bright BG)
    StyledBuilder& bg_gray()           { return append("100"); }  StyledBuilder& bg_bright_black()   { return append("100"); }
    StyledBuilder& bg_bright_red()     { return append("101"); }  StyledBuilder& bg_bright_green()   { return append("102"); }
    StyledBuilder& bg_bright_yellow()  { return append("103"); }  StyledBuilder& bg_bright_blue()    { return append("104"); }
    StyledBuilder& bg_bright_magenta() { return append("105"); }  StyledBuilder& bg_bright_cyan()    { return append("106"); }
    StyledBuilder& bg_bright_white()   { return append("107"); }

    // 256 Colores (8-bit) y Truecolor (24-bit RGB / Hex)
    StyledBuilder& color8(uint8_t c)    { return append("38;5;" + std::to_string(c)); }
    StyledBuilder& bg_color8(uint8_t c) { return append("48;5;" + std::to_string(c)); }
    StyledBuilder& rgb(uint8_t r, uint8_t g, uint8_t b)    { return append("38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b)); }
    StyledBuilder& bg_rgb(uint8_t r, uint8_t g, uint8_t b) { return append("48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b)); }
    StyledBuilder& hex(std::string_view h)    { RgbColor c = Fg::Hex(h); return rgb(c.r, c.g, c.b); }
    StyledBuilder& bg_hex(std::string_view h) { RgbColor c = Bg::Hex(h); return bg_rgb(c.r, c.g, c.b); }

    // Modificadores por enum / struct
    template <typename M>
    StyledBuilder& add(const M& mod) { return append(to_sgr_code(mod)); }

    std::string str() const {
        return m_sgr.empty() ? m_text : "\033[" + m_sgr + "m" + m_text + "\033[0m";
    }

    operator std::string() const { return str(); }
    friend std::ostream& operator<<(std::ostream& os, const StyledBuilder& b) { return os << b.str(); }
};

template <typename T>
inline StyledBuilder styled(const T& val) { return StyledBuilder(val); }

template <typename T, typename... Modifiers>
inline std::string colored(const T& val, Modifiers&&... mods) {
    StyledBuilder builder(val);
    (builder.add(std::forward<Modifiers>(mods)), ...);
    return builder.str();
}

} // namespace Ansi

// Exportar al espacio global de utilidades
using Ansi::Style;
using Ansi::Color;
using Ansi::BgColor;
using Ansi::RgbColor;
using Ansi::Color8Val;
using Ansi::Fg::Rgb;
using Ansi::Fg::Color8;
using Ansi::styled;
using Ansi::colored;
using Ansi::visible_length;

#endif // __ANSISTYLE_H__
