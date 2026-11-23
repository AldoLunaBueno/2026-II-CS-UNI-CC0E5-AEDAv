/**
 * @file print_colored.h
 * @ingroup visualization
 * @brief Funciones auxiliares para impresión en consola con secuencias ANSI condicionales.
 */

#ifndef __PRINTCOLORED_H__
#define __PRINTCOLORED_H__

#include <iostream>
#include <string_view>
#include "ansi_style.h"

/**
 * @brief Convierte un `Color` a su secuencia de escape ANSI SGR.
 * @param c Color de primer plano.
 * @return Secuencia de escape lista para imprimir en terminal.
 */
inline std::string toAnsi(Color c) {
    if (c == Color::Reset) return "\033[0m";
    return "\033[" + to_sgr_code(c) + "m";
}

/**
 * @brief Imprime `value` en `os`, aplicando estilos ANSI solo si el flujo es consola (`cout`/`cerr`/`clog`).
 * @tparam T Tipo del valor a imprimir.
 * @tparam Modifiers Modificadores de estilo/color compatibles con `Ansi::colored()`.
 */
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
