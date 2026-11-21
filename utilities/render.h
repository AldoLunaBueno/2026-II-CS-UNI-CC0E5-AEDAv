/**
 * @file render.h
 * @ingroup visualization
 * @brief Subsistema de rasterizado 2D en consola (`gotoxy`, `ConsoleGraphRenderer`) para `VisualGraph`.
 */

#ifndef __RENDER_H__
#define __RENDER_H__

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "../types.h"
#include "ansi_metrics.h"
#include "gotoxy.h"
#include "layouts/visual_graph.h"


/**
 * @class GraphRenderer
 * @brief Interfaz abstracta para renderizadores de `VisualGraph`.
 *
 * @image html render_pipeline.svg "Pipeline de renderizado ASCII: Contenedor -> Layout -> VisualGraph -> ConsoleGraphRenderer"
 */
class GraphRenderer {
public:
    virtual ~GraphRenderer() = default;
    /** @brief Renderiza `graph` hacia un flujo de salida. */
    virtual std::ostream& render(const VisualGraph&) = 0;
};

/** @brief Placeholder para futuro renderizador vectorial 2D en SVG. */
class SvgGraphRenderer : public GraphRenderer {};

/** @brief Conjunto de glifos UTF-8/ASCII utilizados por `ConsoleGraphRenderer` para trazar aristas. */
struct ConsoleGlyphs {
    std::string h_dash            = "─";
    std::string v_bar             = "│";
    std::string corner_down_right = "╭"; // ╭
    std::string corner_down_left  = "╮"; // ╮
    std::string corner_up_right   = "╰"; // ╰
    std::string corner_up_left    = "╯"; // ╯
    std::string diag_up           = "╱"; // ╱
    std::string diag_down         = "╲"; // ╲
    std::string arrow             = "►"; // ►
};

/** @brief Estilos de trazo de línea para el renderizador de consola. */
enum class ConsoleLineStyle {
    Rounded,       /**< Esquinas curvas/codos UTF-8 (`╭ ╮ ╰ ╯`) con diagonales continuas (`╱ ╲`) y flechas (`►`). */
    Sharp,         /**< Esquinas rectas UTF-8 (`┌ ┐ └ ┘`) con diagonales continuas (`╱ ╲`) y flechas (`►`). */
    DivisionSlash, /**< Usa `U+2215 (∕)` y `U+2216 (∖)` en las uniones. */
    Ascii          /**< ASCII clásico (`-`, `/`, `\`, `>`). */
};

/**
 * @class ConsoleGraphRenderer
 * @brief Renderizador concreto que dibuja un `VisualGraph` directamente en consola usando `gotoxy`.
 */
class ConsoleGraphRenderer : public GraphRenderer {
    std::ostream& m_os;
    ConsoleLineStyle m_style;

    void put_at(TX x, TX y, const std::string& glyph) {
        if (x < 0 || y < 0 || glyph.empty()) return;
        gotoxy(m_os, x, y);
        m_os << glyph;
    }

    void draw_edge(TX x1, TX y1, TX len1,
                   TX x2, TX y2, TX len2,
                   bool horizontal_at_from,
                   const ConsoleGlyphs& glyphs,
                   bool directed = false) {
        // Caso 1: Conexión horizontal pura
        if (y1 == y2) {
            TX start_x = (x1 < x2) ? (x1 + len1) : (x2 + len2);
            TX end_x   = (x1 < x2) ? (x2 - 1) : (x1 - 1);
            for (TX x = start_x; x <= end_x; ++x) {
                bool is_arrow = directed && ((x1 < x2 && x == end_x) || (x2 < x1 && x == start_x));
                put_at(x, y1, is_arrow ? glyphs.arrow : glyphs.h_dash);
            }
            return;
        }

        // Caso 2: Conexión vertical pura (mismo centro o mismo origen X)
        TX col1 = x1 + len1 / 2;
        TX col2 = x2 + len2 / 2;
        if (col1 == col2 || x1 == x2) {
            TX y_start = std::min(y1, y2) + 1;
            TX y_end   = std::max(y1, y2) - 1;
            TX col     = (col1 == col2) ? col1 : (x1 + len1 / 2);
            for (TX y = y_start; y <= y_end; ++y) put_at(col, y, glyphs.v_bar);
            return;
        }

        // Caso 3: Combinación horizontal y diagonal a 45°
        TX h_x   = horizontal_at_from ? x1 : x2;
        TX h_y   = horizontal_at_from ? y1 : y2;
        TX h_len = horizontal_at_from ? len1 : len2;

        TX d_x   = horizontal_at_from ? x2 : x1;
        TX d_y   = horizontal_at_from ? y2 : y1;
        TX d_len = horizontal_at_from ? len2 : len1;

        TX diff_y = std::abs(d_y - h_y);
        TX step_x = (d_x > h_x) ? 1 : -1;
        TX step_y = (d_y > h_y) ? 1 : -1;
        TX corner_x = (step_x > 0) ? (d_x - diff_y) : (d_x + d_len + diff_y - 1);

        if (step_x > 0) {
            TX start_x = h_x + h_len;
            if (corner_x >= start_x) {
                for (TX x = start_x; x < corner_x; ++x) put_at(x, h_y, glyphs.h_dash);
                put_at(corner_x, h_y, (step_y > 0) ? glyphs.corner_down_left : glyphs.corner_up_left);
            }
        } else {
            TX end_x = h_x - 1;
            if (corner_x <= end_x) {
                put_at(corner_x, h_y, (step_y > 0) ? glyphs.corner_down_right : glyphs.corner_up_right);
                for (TX x = corner_x + 1; x <= end_x; ++x) {
                    bool is_arrow = (!horizontal_at_from && directed && x == end_x);
                    put_at(x, h_y, is_arrow ? glyphs.arrow : glyphs.h_dash);
                }
            }
        }

        const auto& diag_glyph = (step_x == step_y) ? glyphs.diag_down : glyphs.diag_up;
        for (TX i = 1; i < diff_y; ++i) {
            put_at(corner_x + i * step_x, h_y + i * step_y, diag_glyph);
        }
    }

public:
    explicit ConsoleGraphRenderer(std::ostream& os = std::cout, ConsoleLineStyle style = ConsoleLineStyle::Rounded)
        : m_os(os), m_style(style) {}

    ConsoleGlyphs getGlyphs() const {
        ConsoleGlyphs g;
        if (m_style == ConsoleLineStyle::Sharp) {
            g.corner_down_right = "┌";
            g.corner_down_left  = "┐";
            g.corner_up_right   = "└";
            g.corner_up_left    = "┘";
        } else if (m_style == ConsoleLineStyle::DivisionSlash) {
            g.corner_down_right = g.corner_up_left  = g.diag_up   = "∕";
            g.corner_down_left  = g.corner_up_right = g.diag_down = "∖";
        } else if (m_style == ConsoleLineStyle::Ascii) {
            g.h_dash = "-"; g.v_bar = "|"; g.arrow = ">";
            g.corner_down_right = g.corner_up_left  = g.diag_up   = "/";
            g.corner_down_left  = g.corner_up_right = g.diag_down = "\\";
        }
        return g;
    }

    std::ostream& render(const VisualGraph& graph) override {
        if (graph.nodes.empty()) return m_os;

        ConsoleGlyphs glyphs = getGlyphs();

        std::vector<VisualNode> node_list;
        node_list.reserve(graph.nodes.size());
        TX height = graph.height;
        for (const auto& item_n : graph.nodes) {
            const auto& node = item_n.getValue();
            node_list.push_back(node);
            if (node.y + 1 > height) height = node.y + 1;
        }
        if (height <= 0) return m_os;

        // Reservar espacio vertical en el flujo y guardar el origen (0, 0) del grafo
        gotoxy_origin(m_os, height);

        auto find_node = [&](TX id) -> const VisualNode* {
            for (const auto& node : node_list) {
                if (node.id == id) return &node;
            }
            return nullptr;
        };

        auto get_neighbors = [&](TX node_id) {
            std::vector<VisualNode> neighbors;
            for (const auto& item_e : graph.edges) {
                const auto& edge = item_e.getValue();
                TX other_id = (edge.from_id == node_id) ? edge.to_id :
                              (edge.to_id == node_id)   ? edge.from_id : -1;
                if (other_id != -1) {
                    if (const auto* n = find_node(other_id)) {
                        neighbors.push_back(*n);
                    }
                }
            }
            return neighbors;
        };

        auto is_valley = [](const VisualNode& n, const std::vector<VisualNode>& nbrs) {
            if (nbrs.size() < 2) return false;
            for (const auto& nbr : nbrs) if (nbr.y >= n.y) return false;
            return true;
        };

        // 1. Trazar aristas con regla agnóstica de Crestas y Valles
        for (const auto& item_e : graph.edges) {
            const auto& edge = item_e.getValue();
            const auto* from_node = find_node(edge.from_id);
            const auto* to_node   = find_node(edge.to_id);
            if (!from_node || !to_node) continue;

            auto from_nbrs = get_neighbors(from_node->id);
            auto to_nbrs   = get_neighbors(to_node->id);

            bool horizontal_at_from = true;
            if (is_valley(*from_node, from_nbrs)) {
                horizontal_at_from = true;
            } else if (is_valley(*to_node, to_nbrs)) {
                horizontal_at_from = false;
            } else {
                TX outgoing_right = 0;
                for (const auto& item_other_e : graph.edges) {
                    const auto& other_e = item_other_e.getValue();
                    if (other_e.from_id == from_node->id) {
                        if (const auto* cand = find_node(other_e.to_id)) {
                            if (cand->x > from_node->x && cand->y != from_node->y) outgoing_right++;
                        }
                    }
                }
                horizontal_at_from = (outgoing_right >= 2) ? false : (from_node->y < to_node->y);
            }

            draw_edge(
                from_node->x, from_node->y, static_cast<TX>(visible_length(from_node->label)),
                to_node->x, to_node->y, static_cast<TX>(visible_length(to_node->label)),
                horizontal_at_from, glyphs, graph.directed
            );
        }

        // 2. Colocar las etiquetas de los nodos directamente en consola
        for (const auto& node : node_list) {
            put_at(node.x, node.y, node.label);
        }

        // Posicionar el cursor debajo del grafo para continuar el flujo normal
        gotoxy(m_os, 0, height);
        return m_os;
    }
};

#endif // __RENDER_H__
