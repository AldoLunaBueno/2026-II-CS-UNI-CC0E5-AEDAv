#ifndef __CANVAS2D_H__
#define __CANVAS2D_H__

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cmath>
#include "../types.h"
#include "ansistyle.h"

// Conjunto de glifos utilizados para el trazado de aristas y esquinas/codos
struct ConsoleGlyphs {
    std::string h_dash = "─";
    std::string v_bar  = "│";
    std::string corner_down_right = "╭"; // Codo abajo y derecha (╭)
    std::string corner_down_left  = "╮"; // Codo abajo e izquierda (╮)
    std::string corner_up_right   = "╰"; // Codo arriba y derecha (╰)
    std::string corner_up_left    = "╯"; // Codo arriba e izquierda (╯)
    std::string diag_up           = "╱"; // Diagonal continua (╱)
    std::string diag_down         = "╲"; // Diagonal continua (╲)
    std::string arrow             = "►"; // Cabecera de flecha (> o ►)
};

// Lienzo 2D generalista con direccionamiento libre en coordenadas (x, y)
// Soporta caracteres ASCII y glifos UTF-8 (ej. Box-Drawing '─', '╭', '╰', '╱', '╲') en celdas individuales
// sin desincronizar los índices de columna ni colapsar la salida en terminal.
class Canvas2D {
private:
    std::vector<std::vector<std::string>> m_grid;
    TX m_width = 0;
    TX m_height = 0;

    void ensure_capacity(TX x, TX y) {
        if (y >= m_height) {
            TX new_height = y + 1;
            m_grid.resize(new_height, std::vector<std::string>(m_width, " "));
            m_height = new_height;
        }
        if (x >= m_width) {
            TX new_width = x + 1;
            for (auto& row : m_grid) {
                row.resize(new_width, " ");
            }
            m_width = new_width;
        }
    }

public:
    Canvas2D(TX initial_w = 0, TX initial_h = 0) {
        if (initial_w > 0 && initial_h > 0) {
            ensure_capacity(initial_w - 1, initial_h - 1);
        }
    }

    TX width() const { return m_width; }
    TX height() const { return m_height; }

    void set(TX x, TX y, char ch) {
        set(x, y, std::string(1, ch));
    }

    void set(TX x, TX y, const std::string& glyph) {
        if (x < 0 || y < 0 || glyph.empty()) return;
        ensure_capacity(x, y);
        m_grid[y][x] = glyph;
    }

    void write(TX x, TX y, const std::string& text) {
        if (x < 0 || y < 0 || text.empty()) return;
        TX vis_len = static_cast<TX>(visible_length(text));
        if (vis_len <= 0) return;
        ensure_capacity(x + vis_len - 1, y);

        size_t i = 0;
        TX cur_x = x;
        std::string pending_ansi;

        while (i < text.size()) {
            if (text[i] == '\033' || text[i] == '\x1B') {
                size_t start = i++;
                if (i < text.size() && text[i] == '[') {
                    i++;
                    while (i < text.size() && (text[i] < '@' || text[i] > '~')) {
                        i++;
                    }
                    if (i < text.size()) i++;
                }
                pending_ansi += text.substr(start, i - start);
                continue;
            }

            // Carácter visible (soporte UTF-8 multibyte)
            size_t char_len = 1;
            unsigned char c = static_cast<unsigned char>(text[i]);
            if ((c & 0x80) == 0) char_len = 1;
            else if ((c & 0xE0) == 0xC0) char_len = 2;
            else if ((c & 0xF0) == 0xE0) char_len = 3;
            else if ((c & 0xF8) == 0xF0) char_len = 4;
            char_len = std::min(char_len, text.size() - i);

            std::string glyph = pending_ansi + text.substr(i, char_len);
            pending_ansi.clear();
            m_grid[y][cur_x] = glyph;

            i += char_len;
            cur_x++;
        }

        // Si quedó una secuencia de escape pendiente (ej. reset \033[0m al final), adjuntarla a la última celda escrita
        if (!pending_ansi.empty() && cur_x > x) {
            m_grid[y][cur_x - 1] += pending_ansi;
        }
    }

    // Traza una arista generalista entre dos nodos con cajas delimitadoras (x1, y1, len1) y (x2, y2, len2)
    // Conecta fluidamente diagonales, tramos ortogonales y codos (╭, ╮, ╰, ╯) sin escaleras ni colisiones.
    void draw_edge(TX x1, TX y1, TX len1,
                   TX x2, TX y2, TX len2,
                   bool horizontal_at_from,
                   const ConsoleGlyphs& glyphs,
                   bool directed = false) {
        // Caso 1: Conexión horizontal pura
        if (y1 == y2) {
            if (x1 < x2) {
                TX start_x = x1 + len1;
                TX end_x   = x2 - 1;
                for (TX x = start_x; x < end_x; ++x) {
                    set(x, y1, glyphs.h_dash);
                }
                if (end_x >= start_x) {
                    set(end_x, y1, directed ? glyphs.arrow : glyphs.h_dash);
                }
            } else if (x2 < x1) {
                TX start_x = x2 + len2;
                TX end_x   = x1 - 1;
                for (TX x = start_x + (directed ? 1 : 0); x <= end_x; ++x) {
                    set(x, y1, glyphs.h_dash);
                }
                if (directed && start_x <= end_x) {
                    set(start_x, y1, glyphs.arrow);
                }
            }
            return;
        }

        // Caso 2: Conexión vertical pura (columnas alineadas o solapadas)
        if (x1 == x2) {
            TX y_start = std::min(y1, y2) + 1;
            TX y_end   = std::max(y1, y2) - 1;
            TX col     = x1 + len1 / 2;
            for (TX y = y_start; y <= y_end; ++y) {
                set(col, y, glyphs.v_bar);
            }
            return;
        }

        // Caso 3: Combinación de tramo horizontal y diagonal a 45°
        TX h_x   = horizontal_at_from ? x1 : x2;
        TX h_y   = horizontal_at_from ? y1 : y2;
        TX h_len = horizontal_at_from ? len1 : len2;

        TX d_x   = horizontal_at_from ? x2 : x1;
        TX d_y   = horizontal_at_from ? y2 : y1;
        TX d_len = horizontal_at_from ? len2 : len1;

        // Cuadrante 1: h_node arriba-izquierda, d_node abajo-derecha
        if (h_y < d_y && h_x < d_x) {
            TX diff_y = d_y - h_y;
            TX corner_x = d_x - diff_y;
            TX start_x = h_x + h_len;
            if (corner_x >= start_x) {
                for (TX x = start_x; x < corner_x; ++x) {
                    set(x, h_y, glyphs.h_dash);
                }
                set(corner_x, h_y, glyphs.corner_down_left);
            }
            for (TX i = 1; i < diff_y; ++i) {
                set(corner_x + i, h_y + i, glyphs.diag_down);
            }
        }
        // Cuadrante 2: h_node arriba-derecha, d_node abajo-izquierda
        else if (h_y < d_y && h_x > d_x) {
            TX diff_y = d_y - h_y;
            TX corner_x = d_x + d_len + diff_y - 1;
            TX end_x = h_x - 1;
            if (corner_x <= end_x) {
                set(corner_x, h_y, glyphs.corner_down_right);
                for (TX x = corner_x + 1; x <= end_x; ++x) {
                    if (!horizontal_at_from && directed && x == end_x) {
                        set(x, h_y, glyphs.arrow);
                    } else {
                        set(x, h_y, glyphs.h_dash);
                    }
                }
            }
            for (TX i = 1; i < diff_y; ++i) {
                set(corner_x - i, h_y + i, glyphs.diag_up);
            }
        }
        // Cuadrante 3: h_node abajo-izquierda, d_node arriba-derecha
        else if (h_y > d_y && h_x < d_x) {
            TX diff_y = h_y - d_y;
            TX corner_x = d_x - diff_y;
            TX start_x = h_x + h_len;
            if (corner_x >= start_x) {
                for (TX x = start_x; x < corner_x; ++x) {
                    set(x, h_y, glyphs.h_dash);
                }
                set(corner_x, h_y, glyphs.corner_up_left);
            }
            for (TX i = 1; i < diff_y; ++i) {
                set(corner_x + i, h_y - i, glyphs.diag_up);
            }
        }
        // Cuadrante 4: h_node abajo-derecha, d_node arriba-izquierda
        else if (h_y > d_y && h_x > d_x) {
            TX diff_y = h_y - d_y;
            TX corner_x = d_x + d_len + diff_y - 1;
            TX end_x = h_x - 1;
            if (corner_x <= end_x) {
                set(corner_x, h_y, glyphs.corner_up_right);
                for (TX x = corner_x + 1; x <= end_x; ++x) {
                    if (!horizontal_at_from && directed && x == end_x) {
                        set(x, h_y, glyphs.arrow);
                    } else {
                        set(x, h_y, glyphs.h_dash);
                    }
                }
            }
            for (TX i = 1; i < diff_y; ++i) {
                set(corner_x - i, h_y - i, glyphs.diag_down);
            }
        }
    }

    // Renderiza el lienzo emitiendo saltos de línea (\n) reales
    std::ostream& render(std::ostream& os) const {
        for (const auto& row : m_grid) {
            int last_idx = -1;
            for (int c = static_cast<int>(row.size()) - 1; c >= 0; --c) {
                if (!row[c].empty() && row[c] != " ") {
                    last_idx = c;
                    break;
                }
            }
            if (last_idx >= 0) {
                for (int c = 0; c <= last_idx; ++c) {
                    os << (row[c].empty() ? " " : row[c]);
                }
            }
            os << '\n';
        }
        return os;
    }
};

#endif // __CANVAS2D_H__
