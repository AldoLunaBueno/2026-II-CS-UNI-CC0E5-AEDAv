/**
 * @file clipped_graph_renderer.h
 * @ingroup visualization
 * @brief Renderizador 2D con recorte de viewport y desplazamiento (`ClippedGraphRenderer`) para `VisualGraph`.
 *
 * Permite proyectar un `VisualGraph` completo dentro de una ventana rectangular fija `(view_w x view_h)`
 * desplazada por `(scroll_x, scroll_y)`, recortando aristas y etiquetas UTF-8/ANSI sin desbordar los bordes.
 */

#ifndef __CLIPPED_GRAPH_RENDERER_H__
#define __CLIPPED_GRAPH_RENDERER_H__

#include "render.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

/**
 * @class ClippedGraphRenderer
 * @brief Renderiza un `VisualGraph` dentro de un viewport rectangular con recorte y scroll `(scroll_x, scroll_y)`.
 */
class ClippedGraphRenderer : public GraphRenderer {
    std::ostream&    m_os;
    ConsoleLineStyle m_style;
    TX   m_origin_x = 0;
    TX   m_origin_y = 0;
    TX   m_view_w   = 40;
    TX   m_view_h   = 8;
    TX   m_scroll_x = 0;
    TX   m_scroll_y = 0;
    bool m_standalone_flow = true;

    struct RasterCell {
        std::string glyph = " ";
        std::string sgr_prefix;
    };

    using Canvas = std::vector<std::vector<RasterCell>>;

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

    static size_t utf8Len(unsigned char c) {
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    }

    void putClippedGlyph(Canvas& canvas, TX world_x, TX world_y, const std::string& glyph, const std::string& sgr = "") const {
        TX lx = world_x - m_scroll_x;
        TX ly = world_y - m_scroll_y;
        if (lx < 0 || lx >= m_view_w || ly < 0 || ly >= m_view_h || glyph.empty()) return;
        canvas[static_cast<size_t>(ly)][static_cast<size_t>(lx)] = RasterCell{glyph, sgr};
    }

    void putClippedStyledLabel(Canvas& canvas, TX world_x, TX world_y, const std::string& label) const {
        TX ly = world_y - m_scroll_y;
        if (ly < 0 || ly >= m_view_h) return;

        std::string active_sgr;
        TX col = world_x;
        size_t i = 0;
        while (i < label.size()) {
            if (label[i] == '\033' || label[i] == '\x1B') {
                size_t start = i++;
                if (i < label.size() && label[i] == '[') {
                    ++i;
                    while (i < label.size() && (label[i] < '@' || label[i] > '~')) ++i;
                    if (i < label.size()) ++i;
                }
                std::string seq = label.substr(start, i - start);
                if (seq == "\033[0m" || seq == "\033[m") active_sgr.clear();
                else active_sgr += seq;
                continue;
            }
            size_t len = std::min(utf8Len(static_cast<unsigned char>(label[i])), label.size() - i);
            putClippedGlyph(canvas, col, world_y, label.substr(i, len), active_sgr);
            i += len;
            ++col;
        }
    }

    void drawEdgeClipped(Canvas& canvas,
                         TX x1, TX y1, TX len1,
                         TX x2, TX y2, TX len2,
                         bool horizontal_at_from,
                         const ConsoleGlyphs& glyphs,
                         bool directed) const {
        const std::string edge_sgr = "\033[90m";
        if (y1 == y2) {
            TX start_x = (x1 < x2) ? (x1 + len1) : (x2 + len2);
            TX end_x   = (x1 < x2) ? (x2 - 1) : (x1 - 1);
            for (TX x = start_x; x <= end_x; ++x) {
                bool is_arrow = directed && ((x1 < x2 && x == end_x) || (x2 < x1 && x == start_x));
                putClippedGlyph(canvas, x, y1, is_arrow ? glyphs.arrow : glyphs.h_dash, edge_sgr);
            }
            return;
        }

        TX col1 = x1 + len1 / 2;
        TX col2 = x2 + len2 / 2;
        if (col1 == col2 || x1 == x2) {
            TX y_start = std::min(y1, y2) + 1;
            TX y_end   = std::max(y1, y2) - 1;
            TX col     = (col1 == col2) ? col1 : (x1 + len1 / 2);
            for (TX y = y_start; y <= y_end; ++y) putClippedGlyph(canvas, col, y, glyphs.v_bar, edge_sgr);
            return;
        }

        TX h_x   = horizontal_at_from ? x1 : x2;
        TX h_y   = horizontal_at_from ? y1 : y2;
        TX h_len = horizontal_at_from ? len1 : len2;

        TX d_x   = horizontal_at_from ? x2 : x1;
        TX d_y   = horizontal_at_from ? y2 : y1;
        TX d_len = horizontal_at_from ? len2 : len1;

        TX diff_y   = std::abs(d_y - h_y);
        TX step_x   = (d_x > h_x) ? 1 : -1;
        TX step_y   = (d_y > h_y) ? 1 : -1;
        TX corner_x = (step_x > 0) ? (d_x - diff_y) : (d_x + d_len + diff_y - 1);

        if (step_x > 0) {
            TX start_x = h_x + h_len;
            if (corner_x >= start_x) {
                for (TX x = start_x; x < corner_x; ++x) putClippedGlyph(canvas, x, h_y, glyphs.h_dash, edge_sgr);
                putClippedGlyph(canvas, corner_x, h_y, (step_y > 0) ? glyphs.corner_down_left : glyphs.corner_up_left, edge_sgr);
            }
        } else {
            TX end_x = h_x - 1;
            if (corner_x <= end_x) {
                putClippedGlyph(canvas, corner_x, h_y, (step_y > 0) ? glyphs.corner_down_right : glyphs.corner_up_right, edge_sgr);
                for (TX x = corner_x + 1; x <= end_x; ++x) {
                    bool is_arrow = (!horizontal_at_from && directed && x == end_x);
                    putClippedGlyph(canvas, x, h_y, is_arrow ? glyphs.arrow : glyphs.h_dash, edge_sgr);
                }
            }
        }

        const auto& diag_glyph = (step_x == step_y) ? glyphs.diag_down : glyphs.diag_up;
        for (TX i = 1; i < diff_y; ++i) {
            putClippedGlyph(canvas, corner_x + i * step_x, h_y + i * step_y, diag_glyph, edge_sgr);
        }
    }

public:
    ClippedGraphRenderer(std::ostream& os,
                         TX view_w, TX view_h,
                         TX scroll_x = 0, TX scroll_y = 0,
                         TX origin_x = 0, TX origin_y = 0,
                         bool standalone_flow = true,
                         ConsoleLineStyle style = ConsoleLineStyle::Rounded)
        : m_os(os), m_style(style),
          m_origin_x(origin_x), m_origin_y(origin_y),
          m_view_w(std::max<TX>(1, view_w)), m_view_h(std::max<TX>(1, view_h)),
          m_scroll_x(std::max<TX>(0, scroll_x)), m_scroll_y(std::max<TX>(0, scroll_y)),
          m_standalone_flow(standalone_flow) {}

    /** @brief Rasteriza el `VisualGraph` recortado a `view_w x view_h` y retorna las filas listas con SGR ANSI. */
    std::vector<std::string> renderToLines(const VisualGraph& graph) const {
        std::vector<std::string> lines(static_cast<size_t>(m_view_h), std::string(static_cast<size_t>(m_view_w), ' '));
        if (graph.nodes.empty() || m_view_w <= 0 || m_view_h <= 0) return lines;

        Canvas canvas(static_cast<size_t>(m_view_h), std::vector<RasterCell>(static_cast<size_t>(m_view_w)));
        ConsoleGlyphs glyphs = getGlyphs();

        std::vector<VisualNode> node_list;
        node_list.reserve(graph.nodes.size());
        for (const auto& item_n : graph.nodes) node_list.push_back(item_n.getValue());

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
                    if (const auto* n = find_node(other_id)) neighbors.push_back(*n);
                }
            }
            return neighbors;
        };

        auto is_valley = [](const VisualNode& n, const std::vector<VisualNode>& nbrs) {
            if (nbrs.size() < 2) return false;
            for (const auto& nbr : nbrs) if (nbr.y >= n.y) return false;
            return true;
        };

        // 1. Aristas recortadas al viewport
        for (const auto& item_e : graph.edges) {
            const auto& edge = item_e.getValue();
            const auto* from_node = find_node(edge.from_id);
            const auto* to_node   = find_node(edge.to_id);
            if (!from_node || !to_node) continue;

            auto from_nbrs = get_neighbors(from_node->id);
            auto to_nbrs   = get_neighbors(to_node->id);

            bool horizontal_at_from = true;
            if (is_valley(*from_node, from_nbrs)) horizontal_at_from = true;
            else if (is_valley(*to_node, to_nbrs)) horizontal_at_from = false;
            else {
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

            drawEdgeClipped(canvas,
                            from_node->x, from_node->y, static_cast<TX>(visible_length(from_node->label)),
                            to_node->x, to_node->y, static_cast<TX>(visible_length(to_node->label)),
                            horizontal_at_from, glyphs, graph.directed);
        }

        // 2. Etiquetas de nodos recortadas preservando colores ANSI
        for (const auto& node : node_list) {
            putClippedStyledLabel(canvas, node.x, node.y, node.label);
        }

        // 3. Serializar cada fila agrupando SGR consecutivos
        for (TX y = 0; y < m_view_h; ++y) {
            std::ostringstream row_ss;
            std::string active_sgr;
            for (TX x = 0; x < m_view_w; ++x) {
                const auto& cell = canvas[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (cell.sgr_prefix != active_sgr) {
                    if (!active_sgr.empty()) row_ss << "\033[0m";
                    if (!cell.sgr_prefix.empty()) row_ss << cell.sgr_prefix;
                    active_sgr = cell.sgr_prefix;
                }
                row_ss << cell.glyph;
            }
            if (!active_sgr.empty()) row_ss << "\033[0m";
            lines[static_cast<size_t>(y)] = row_ss.str();
        }
        return lines;
    }

    std::ostream& render(const VisualGraph& graph) override {
        auto lines = renderToLines(graph);
        if (m_standalone_flow) {
            gotoxy_origin(m_os, m_view_h);
        }
        for (TX y = 0; y < m_view_h; ++y) {
            gotoxy(m_os, m_origin_x, m_origin_y + y);
            m_os << lines[static_cast<size_t>(y)];
        }
        if (m_standalone_flow) {
            gotoxy(m_os, 0, m_view_h);
        }
        return m_os;
    }
};

#endif // __CLIPPED_GRAPH_RENDERER_H__
