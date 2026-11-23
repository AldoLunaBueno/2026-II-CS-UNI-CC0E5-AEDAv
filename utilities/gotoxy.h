/**
 * @file gotoxy.h
 * @brief Utilidades de posicionamiento del cursor y control de pantalla en consola mediante secuencias ANSI.
 */

#ifndef __GOTOXY_H__
#define __GOTOXY_H__

#include <iostream>
#include <string_view>
#include "../types.h"

namespace Ansi {

inline constexpr char ESC = '\033';

// Pantalla alternativa, ajuste de línea, cursor y sincronización de fotogramas
inline constexpr const char* ALT_SCREEN_ON   = "\033[?1049h";
inline constexpr const char* ALT_SCREEN_OFF  = "\033[?1049l";
inline constexpr const char* AUTOWRAP_ON     = "\033[?7h";
inline constexpr const char* AUTOWRAP_OFF    = "\033[?7l";
inline constexpr const char* CURSOR_SHOW     = "\033[?25h";
inline constexpr const char* CURSOR_HIDE     = "\033[?25l";
inline constexpr const char* CURSOR_SAVE     = "\033[s";
inline constexpr const char* CURSOR_RESTORE  = "\033[u";
inline constexpr const char* CURSOR_HOME     = "\033[1;1H";
inline constexpr const char* CLEAR_HOME_SAVE = "\033[2J\033[1;1H\033[s";
inline constexpr const char* SYNC_BEGIN      = "\033[?2026h";
inline constexpr const char* SYNC_END        = "\033[?2026l";

/** @brief Limpia toda la pantalla, posiciona en `(1, 1)` y guarda ese origen para `gotoxy()`. */
inline void clearAndSaveOrigin(std::ostream& os) {
    os << CLEAR_HOME_SAVE;
}

/** @brief Envuelve un fotograma en modo sincronizado (DEC 2026) y vacía el flujo para evitar parpadeo. */
inline void flushSynchronizedFrame(std::ostream& os, std::string_view frame) {
    os << SYNC_BEGIN << frame << SYNC_END << std::flush;
}

} // namespace Ansi

/**
 * @brief Reserva `height` líneas en el flujo y guarda la esquina superior izquierda como origen `(0, 0)`.
 * @param os Flujo de salida de consola.
 * @param height Cantidad de filas a reservar.
 */
inline void gotoxy_origin(std::ostream& os, TX height) {
    if (height <= 0) return;
    for (TX i = 0; i < height; ++i) os << '\n';
    os << "\033[" << height << "A\033[1G" << Ansi::CURSOR_SAVE;
}

/**
 * @brief Posiciona el cursor en la coordenada `(x, y)` relativa al origen guardado (`\033[s`).
 * @param os Flujo de salida de consola.
 * @param x Coordenada horizontal (0-indexada).
 * @param y Coordenada vertical (0-indexada).
 */
inline void gotoxy(std::ostream& os, TX x, TX y) {
    if (x < 0 || y < 0) return;
    os << Ansi::CURSOR_RESTORE;
    if (y > 0) os << "\033[" << y << "B";
    os << "\033[" << (x + 1) << "G";
}

/**
 * @brief Sobrecarga de `gotoxy` que opera directamente sobre `std::cout`.
 * @param x Coordenada horizontal (0-indexada).
 * @param y Coordenada vertical (0-indexada).
 */
inline void gotoxy(TX x, TX y) {
    gotoxy(std::cout, x, y);
}

#endif // __GOTOXY_H__
