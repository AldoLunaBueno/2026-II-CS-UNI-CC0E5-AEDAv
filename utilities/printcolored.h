#ifndef __PRINTCOLORED_H__
#define __PRINTCOLORED_H__

#include <iostream>
#include <string_view>
#include "ansistyle.h"

// Obtener la secuencia ANSI para un color
inline std::string toAnsi(Color c) {
    if (c == Color::Reset) return "\033[0m";
    return "\033[" + to_sgr_code(c) + "m";
}

template <typename T, typename... Modifiers>
std::ostream &PrintColored(std::ostream &os, const T &value, Modifiers&&... mods) {
    bool is_console = (&os == &std::cout || &os == &std::cerr || &os == &std::clog);
    if (is_console) {
        os << colored(value, std::forward<Modifiers>(mods)...);
    } else {
        os << value;
    }
    os << std::endl;
    return os;
}

#endif // __PRINTCOLORED_H__
