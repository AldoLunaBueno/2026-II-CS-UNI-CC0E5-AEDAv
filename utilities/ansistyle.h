#ifndef __ANSISTYLE_H__
#define __ANSISTYLE_H__

#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iostream>

namespace Ansi {

// Atributos y estilos tipográficos (SGR - Select Graphic Rendition)
// Referencia: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
enum class Style : uint8_t {
    Reset           = 0,
    Bold            = 1,
    Dim             = 2,
    Italic          = 3,
    Underline       = 4,
    Blink           = 5,
    RapidBlink      = 6,
    Reverse         = 7,
    Hidden          = 8,
    Strike          = 9,
    DoubleUnderline = 21,
    Overline        = 53
};

// Colores de texto (Foreground) estándar y de alta intensidad (Bright)
enum class Color : uint8_t {
    Reset         = 0,
    Black         = 30,
    Red           = 31,
    Green         = 32,
    Yellow        = 33,
    Blue          = 34,
    Magenta       = 35,
    Cyan          = 36,
    White         = 37,
    Default       = 39,
    BrightBlack   = 90, // Gray
    BrightRed     = 91,
    BrightGreen   = 92,
    BrightYellow  = 93,
    BrightBlue    = 94,
    BrightMagenta = 95,
    BrightCyan    = 96,
    BrightWhite   = 97
};

// Colores de fondo (Background) estándar y de alta intensidad (Bright)
enum class BgColor : uint8_t {
    Default       = 49,
    Black         = 40,
    Red           = 41,
    Green         = 42,
    Yellow        = 43,
    Blue          = 44,
    Magenta       = 45,
    Cyan          = 46,
    White         = 47,
    BrightBlack   = 100, // Bg Gray
    BrightRed     = 101,
    BrightGreen   = 102,
    BrightYellow  = 103,
    BrightBlue    = 104,
    BrightMagenta = 105,
    BrightCyan    = 106,
    BrightWhite   = 107
};

// Color indexado de 8 bits (paleta de 256 colores)
struct Color8 {
    uint8_t code = 0;
    bool is_bg = false;
    constexpr Color8(uint8_t c, bool bg = false) : code(c), is_bg(bg) {}
};

// Color Truecolor de 24 bits (RGB)
struct Rgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    bool is_bg = false;
    constexpr Rgb(uint8_t red, uint8_t green, uint8_t blue, bool bg = false)
        : r(red), g(green), b(blue), is_bg(bg) {}
};

// Espacio de nombres con constructores convenientes Fg / Bg
namespace Fg {
    constexpr Color Reset         = Color::Reset;
    constexpr Color Black         = Color::Black;
    constexpr Color Red           = Color::Red;
    constexpr Color Green         = Color::Green;
    constexpr Color Yellow        = Color::Yellow;
    constexpr Color Blue          = Color::Blue;
    constexpr Color Magenta       = Color::Magenta;
    constexpr Color Cyan          = Color::Cyan;
    constexpr Color White         = Color::White;
    constexpr Color Default       = Color::Default;
    constexpr Color BrightBlack   = Color::BrightBlack;
    constexpr Color Gray          = Color::BrightBlack;
    constexpr Color BrightRed     = Color::BrightRed;
    constexpr Color BrightGreen   = Color::BrightGreen;
    constexpr Color BrightYellow  = Color::BrightYellow;
    constexpr Color BrightBlue    = Color::BrightBlue;
    constexpr Color BrightMagenta = Color::BrightMagenta;
    constexpr Color BrightCyan    = Color::BrightCyan;
    constexpr Color BrightWhite   = Color::BrightWhite;

    constexpr Ansi::Color8 Color8(uint8_t code) { return Ansi::Color8(code, false); }
    constexpr Ansi::Rgb Rgb(uint8_t r, uint8_t g, uint8_t b) { return Ansi::Rgb(r, g, b, false); }
    inline Ansi::Rgb Hex(std::string_view hex) {
        if (!hex.empty() && hex[0] == '#') hex.remove_prefix(1);
        uint32_t val = 0;
        for (char c : hex) {
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        }
        return Ansi::Rgb((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF, false);
    }
}

namespace Bg {
    constexpr BgColor Default       = BgColor::Default;
    constexpr BgColor Black         = BgColor::Black;
    constexpr BgColor Red           = BgColor::Red;
    constexpr BgColor Green         = BgColor::Green;
    constexpr BgColor Yellow        = BgColor::Yellow;
    constexpr BgColor Blue          = BgColor::Blue;
    constexpr BgColor Magenta       = BgColor::Magenta;
    constexpr BgColor Cyan          = BgColor::Cyan;
    constexpr BgColor White         = BgColor::White;
    constexpr BgColor BrightBlack   = BgColor::BrightBlack;
    constexpr BgColor Gray          = BgColor::BrightBlack;
    constexpr BgColor BrightRed     = BgColor::BrightRed;
    constexpr BgColor BrightGreen   = BgColor::BrightGreen;
    constexpr BgColor BrightYellow  = BgColor::BrightYellow;
    constexpr BgColor BrightBlue    = BgColor::BrightBlue;
    constexpr BgColor BrightMagenta = BgColor::BrightMagenta;
    constexpr BgColor BrightCyan    = BgColor::BrightCyan;
    constexpr BgColor BrightWhite   = BgColor::BrightWhite;

    constexpr Ansi::Color8 Color8(uint8_t code) { return Ansi::Color8(code, true); }
    constexpr Ansi::Rgb Rgb(uint8_t r, uint8_t g, uint8_t b) { return Ansi::Rgb(r, g, b, true); }
    inline Ansi::Rgb Hex(std::string_view hex) {
        if (!hex.empty() && hex[0] == '#') hex.remove_prefix(1);
        uint32_t val = 0;
        for (char c : hex) {
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        }
        return Ansi::Rgb((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF, true);
    }
}

// Convierte un modificador en su código de parámetro SGR ANSI
inline std::string to_sgr_code(Style s) {
    return std::to_string(static_cast<int>(s));
}

inline std::string to_sgr_code(Color c) {
    return std::to_string(static_cast<int>(c));
}

inline std::string to_sgr_code(BgColor c) {
    return std::to_string(static_cast<int>(c));
}

inline std::string to_sgr_code(const Color8& c) {
    return (c.is_bg ? "48;5;" : "38;5;") + std::to_string(c.code);
}

inline std::string to_sgr_code(const Rgb& c) {
    return (c.is_bg ? "48;2;" : "38;2;") +
           std::to_string(c.r) + ";" +
           std::to_string(c.g) + ";" +
           std::to_string(c.b);
}

// Calcula el ancho visual de una cadena en columnas de terminal,
// omitiendo las secuencias de escape ANSI (CSI) y respetando codepoints UTF-8
inline size_t visible_length(std::string_view s) {
    size_t count = 0;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' || s[i] == '\x1B') {
            i++;
            if (i < s.size() && s[i] == '[') {
                i++;
                while (i < s.size() && (s[i] < '@' || s[i] > '~')) {
                    i++;
                }
                if (i < s.size()) i++;
            }
            continue;
        }

        // Manejo de caracteres multibyte UTF-8
        unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0x80) == 0) {
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            i += 4;
        } else {
            i += 1;
        }
        count++;
    }
    return count;
}

// Fluent Builder: styled(value)...
class StyledBuilder {
private:
    std::string m_text;
    std::vector<std::string> m_codes;

public:
    template <typename T>
    explicit StyledBuilder(const T& val) {
        std::ostringstream oss;
        oss << val;
        m_text = oss.str();
    }

    // Estilos tipográficos
    StyledBuilder& bold()             { m_codes.push_back("1"); return *this; }
    StyledBuilder& dim()              { m_codes.push_back("2"); return *this; }
    StyledBuilder& italic()           { m_codes.push_back("3"); return *this; }
    StyledBuilder& underline()        { m_codes.push_back("4"); return *this; }
    StyledBuilder& blink()            { m_codes.push_back("5"); return *this; }
    StyledBuilder& rapid_blink()      { m_codes.push_back("6"); return *this; }
    StyledBuilder& reverse()          { m_codes.push_back("7"); return *this; }
    StyledBuilder& hidden()           { m_codes.push_back("8"); return *this; }
    StyledBuilder& strike()           { m_codes.push_back("9"); return *this; }
    StyledBuilder& double_underline() { m_codes.push_back("21"); return *this; }
    StyledBuilder& overline()         { m_codes.push_back("53"); return *this; }

    // Colores de texto (4-bit estándar)
    StyledBuilder& black()         { m_codes.push_back("30"); return *this; }
    StyledBuilder& red()           { m_codes.push_back("31"); return *this; }
    StyledBuilder& green()         { m_codes.push_back("32"); return *this; }
    StyledBuilder& yellow()        { m_codes.push_back("33"); return *this; }
    StyledBuilder& blue()          { m_codes.push_back("34"); return *this; }
    StyledBuilder& magenta()       { m_codes.push_back("35"); return *this; }
    StyledBuilder& cyan()          { m_codes.push_back("36"); return *this; }
    StyledBuilder& white()         { m_codes.push_back("37"); return *this; }
    StyledBuilder& default_color() { m_codes.push_back("39"); return *this; }

    // Colores de texto de alta intensidad (Bright)
    StyledBuilder& bright_black()   { m_codes.push_back("90"); return *this; }
    StyledBuilder& gray()           { m_codes.push_back("90"); return *this; }
    StyledBuilder& bright_red()     { m_codes.push_back("91"); return *this; }
    StyledBuilder& bright_green()   { m_codes.push_back("92"); return *this; }
    StyledBuilder& bright_yellow()  { m_codes.push_back("93"); return *this; }
    StyledBuilder& bright_blue()    { m_codes.push_back("94"); return *this; }
    StyledBuilder& bright_magenta() { m_codes.push_back("95"); return *this; }
    StyledBuilder& bright_cyan()    { m_codes.push_back("96"); return *this; }
    StyledBuilder& bright_white()   { m_codes.push_back("97"); return *this; }

    // Colores de fondo (4-bit estándar)
    StyledBuilder& bg_black()         { m_codes.push_back("40"); return *this; }
    StyledBuilder& bg_red()           { m_codes.push_back("41"); return *this; }
    StyledBuilder& bg_green()         { m_codes.push_back("42"); return *this; }
    StyledBuilder& bg_yellow()        { m_codes.push_back("43"); return *this; }
    StyledBuilder& bg_blue()          { m_codes.push_back("44"); return *this; }
    StyledBuilder& bg_magenta()       { m_codes.push_back("45"); return *this; }
    StyledBuilder& bg_cyan()          { m_codes.push_back("46"); return *this; }
    StyledBuilder& bg_white()         { m_codes.push_back("47"); return *this; }
    StyledBuilder& bg_default_color() { m_codes.push_back("49"); return *this; }

    // Colores de fondo de alta intensidad (Bright)
    StyledBuilder& bg_bright_black()   { m_codes.push_back("100"); return *this; }
    StyledBuilder& bg_gray()           { m_codes.push_back("100"); return *this; }
    StyledBuilder& bg_bright_red()     { m_codes.push_back("101"); return *this; }
    StyledBuilder& bg_bright_green()   { m_codes.push_back("102"); return *this; }
    StyledBuilder& bg_bright_yellow()  { m_codes.push_back("103"); return *this; }
    StyledBuilder& bg_bright_blue()    { m_codes.push_back("104"); return *this; }
    StyledBuilder& bg_bright_magenta() { m_codes.push_back("105"); return *this; }
    StyledBuilder& bg_bright_cyan()    { m_codes.push_back("106"); return *this; }
    StyledBuilder& bg_bright_white()   { m_codes.push_back("107"); return *this; }

    // 256 Colores (8-bit)
    StyledBuilder& color8(uint8_t code)    { m_codes.push_back("38;5;" + std::to_string(code)); return *this; }
    StyledBuilder& bg_color8(uint8_t code) { m_codes.push_back("48;5;" + std::to_string(code)); return *this; }

    // 24-bit Truecolor (RGB)
    StyledBuilder& rgb(uint8_t r, uint8_t g, uint8_t b) {
        m_codes.push_back("38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b));
        return *this;
    }
    StyledBuilder& bg_rgb(uint8_t r, uint8_t g, uint8_t b) {
        m_codes.push_back("48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b));
        return *this;
    }
    StyledBuilder& hex(std::string_view hex_str) {
        Rgb c = Fg::Hex(hex_str);
        return rgb(c.r, c.g, c.b);
    }
    StyledBuilder& bg_hex(std::string_view hex_str) {
        Rgb c = Bg::Hex(hex_str);
        return bg_rgb(c.r, c.g, c.b);
    }

    // Agregar modificadores mediante tipos enum/struct
    StyledBuilder& add(Style s)         { m_codes.push_back(to_sgr_code(s)); return *this; }
    StyledBuilder& add(Color c)         { m_codes.push_back(to_sgr_code(c)); return *this; }
    StyledBuilder& add(BgColor c)       { m_codes.push_back(to_sgr_code(c)); return *this; }
    StyledBuilder& add(const Color8& c) { m_codes.push_back(to_sgr_code(c)); return *this; }
    StyledBuilder& add(const Rgb& c)    { m_codes.push_back(to_sgr_code(c)); return *this; }

    // Genera la cadena envuelta en secuencias ANSI y finalizada en reset (\033[0m)
    std::string str() const {
        if (m_codes.empty()) return m_text;
        std::string res = "\033[";
        for (size_t i = 0; i < m_codes.size(); ++i) {
            if (i > 0) res += ";";
            res += m_codes[i];
        }
        res += "m" + m_text + "\033[0m";
        return res;
    }

    operator std::string() const { return str(); }

    friend std::ostream& operator<<(std::ostream& os, const StyledBuilder& b) {
        return os << b.str();
    }
};

// Función de entrada para el builder fluido
template <typename T>
inline StyledBuilder styled(const T& val) {
    return StyledBuilder(val);
}

// Función variádica colored(val, mods...)
template <typename T, typename... Modifiers>
inline std::string colored(const T& val, Modifiers&&... mods) {
    StyledBuilder builder(val);
    (builder.add(std::forward<Modifiers>(mods)), ...);
    return builder.str();
}

} // namespace Ansi

// Exportar al espacio global de utilidades los tipos y funciones más frecuentes
using Ansi::Style;
using Ansi::Color;
using Ansi::BgColor;
using Ansi::Fg::Rgb;
using Ansi::Fg::Color8;
using Ansi::styled;
using Ansi::colored;
using Ansi::visible_length;

#endif // __ANSISTYLE_H__
