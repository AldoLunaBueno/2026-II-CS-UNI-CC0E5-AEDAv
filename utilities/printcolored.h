#ifndef __PRINTCOLORED_H__
#define __PRINTCOLORED_H__

#include <iostream>
#include <string_view>

// Colores básicos soportados por ANSI
enum class Color { Reset, Red, Green, Yellow, Blue, Magenta, Cyan, White };

// Obtener la secuencia ANSI
constexpr std::string_view toAnsi(Color c) {
    switch (c) {
        case Color::Red:     return "\033[31m";
        case Color::Green:   return "\033[32m";
        case Color::Yellow:  return "\033[33m";
        case Color::Blue:    return "\033[34m";
        case Color::Magenta: return "\033[35m";
        case Color::Cyan:    return "\033[36m";
        case Color::White:   return "\033[37m";
        default:             return "\033[0m";
    }
}

template <typename T>
std::ostream &PrintColored(std::ostream &os, const T &value, Color color, bool newline = true) {
    // Si no va a una consola, omitimos el color
    bool is_console = (&os == &std::cout || &os == &std::cerr || &os == &std::clog);
    if (is_console) os << toAnsi(color) << value << toAnsi(Color::Reset);
    else os << value;
    if (newline) os << std::endl;
    return os;
}

#endif // __PRINTCOLORED_H__