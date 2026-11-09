#ifndef __RENDER_H__
#define __RENDER_H__

#include <iostream>
#include "../containers/vector.h"
#include "../types.h"
#include "canvas.h"

// Tipos de aristas
enum class VisualEdgeType {
    LeftChild,
    RightChild,
    General
};

template <typename T>
using SimpleVector = BaseVector<VectorAscTraits<T>, Callable>;

// Nodo en plano XY
struct VisualNode {
    TX id;
    std::string label;
    TX x = 0; // coordenada X
    TX y = 0; // coordenada Y
};

// Conexión entre nodos
struct VisualEdge {
    TX from_id;
    TX to_id;
    VisualEdgeType type = VisualEdgeType::General;
};

// Geometría y topología del grafo
struct VisualGraph {
    SimpleVector<VisualNode> nodes;
    SimpleVector<VisualEdge> edges;
    TX width = 0;       // ancho del layout
    TX height = 0;      // alto del layout
    TX cell_width = 3;  // Ancho de celda usado para el espaciado y ramificaciones
    bool directed = false;
};

// Mixin: dibujable
template <typename Traits>
class Drawable : virtual public ContainerCore<Traits> {
public:
    template <typename Self, typename Layout, typename Renderer>
    decltype(auto) draw(this Self&& self, Layout&& layout, Renderer&& renderer) {
        VisualGraph graph;
        {
            std::lock_guard lock(self.mutex());
            graph = layout(self);
        }
        return renderer.render(graph);
    }
};

// Renderizador abstracto de grafos
class GraphRenderer {
public:
    virtual ~GraphRenderer() = default;
    virtual std::ostream& render(const VisualGraph&) = 0;
};

// TODO: Renderizador de grafos 2D
class SvgGraphRenderer : public GraphRenderer {};

// Estilo de línea para el renderizado en consola
enum class ConsoleLineStyle {
    Rounded,       // Esquinas curvas/codos UTF-8 (╭ ╮ ╰ ╯) con diagonales continuas (╱ ╲) y flechas (►) [Por defecto]
    Sharp,         // Esquinas rectas UTF-8 (┌ ┐ └ ┘) con diagonales continuas (╱ ╲) y flechas (►)
    DivisionSlash, // Usa U+2215 (∕) y U+2216 (∖) en las uniones
    BoxDrawing = Rounded, // Alias compatible hacia atrás
    Ascii          // ASCII clásico (-, /, \, >)
};

// Renderizador generalista de grafos en consola
class ConsoleGraphRenderer : public GraphRenderer {
    std::ostream& m_os;
    ConsoleLineStyle m_style;
public:
    explicit ConsoleGraphRenderer(std::ostream& os = std::cout, ConsoleLineStyle style = ConsoleLineStyle::Rounded)
        : m_os(os), m_style(style) {}

    ConsoleGlyphs getGlyphs() const {
        ConsoleGlyphs g;
        if (m_style == ConsoleLineStyle::DivisionSlash) {
            g.h_dash = "─";
            g.v_bar  = "│";
            g.corner_down_right = "∕"; // U+2215 (Division slash)
            g.corner_down_left  = "∖"; // U+2216 (Set minus / mirror)
            g.corner_up_right   = "∖"; // U+2216 (Set minus / mirror)
            g.corner_up_left    = "∕"; // U+2215 (Division slash)
            g.diag_up           = "∕"; // U+2215
            g.diag_down         = "∖"; // U+2216
            g.arrow             = "►";
        } else if (m_style == ConsoleLineStyle::Rounded) {
            g.h_dash = "─";
            g.v_bar  = "│";
            g.corner_down_right = "╭";
            g.corner_down_left  = "╮";
            g.corner_up_right   = "╰";
            g.corner_up_left    = "╯";
            g.diag_up           = "╱";
            g.diag_down         = "╲";
            g.arrow             = "►";
        } else if (m_style == ConsoleLineStyle::Sharp) {
            g.h_dash = "─";
            g.v_bar  = "│";
            g.corner_down_right = "┌";
            g.corner_down_left  = "┐";
            g.corner_up_right   = "└";
            g.corner_up_left    = "┘";
            g.diag_up           = "╱";
            g.diag_down         = "╲";
            g.arrow             = "►";
        } else {
            g.h_dash = "-";
            g.v_bar  = "|";
            g.corner_down_right = "/";
            g.corner_down_left  = "\\";
            g.corner_up_right   = "\\";
            g.corner_up_left    = "/";
            g.diag_up           = "/";
            g.diag_down         = "\\";
            g.arrow             = ">";
        }
        return g;
    }

    std::string getDashGlyph() const {
        return getGlyphs().h_dash;
    }

    std::ostream& render(const VisualGraph& graph) override {
        if (graph.nodes.empty()) return m_os;

        Canvas2D canvas;
        ConsoleGlyphs glyphs = getGlyphs();

        // Helper para obtener los vecinos conectados a un nodo en el plano
        auto get_neighbors = [&](TX node_id) {
            std::vector<VisualNode> neighbors;
            for (const auto& item_e : graph.edges) {
                const auto& edge = item_e.getValue();
                TX other_id = -1;
                if (edge.from_id == node_id) other_id = edge.to_id;
                else if (edge.to_id == node_id) other_id = edge.from_id;

                if (other_id != -1) {
                    for (const auto& item_n : graph.nodes) {
                        const auto& node = item_n.getValue();
                        if (node.id == other_id) {
                            neighbors.push_back(node);
                            break;
                        }
                    }
                }
            }
            return neighbors;
        };

        // 1. Trazar aristas con la regla agnóstica de Crestas y Valles
        for (const auto& item_e : graph.edges) {
            const auto& edge = item_e.getValue();
            VisualNode from_node, to_node;
            bool found_from = false, found_to = false;

            for (const auto& item_n : graph.nodes) {
                const auto& node = item_n.getValue();
                if (node.id == edge.from_id) { from_node = node; found_from = true; }
                if (node.id == edge.to_id)   { to_node = node;   found_to = true; }
            }

            if (!found_from || !found_to) continue;

            // Decisión agnóstica: ¿el tramo horizontal corresponde a from_node o to_node?
            bool horizontal_at_from = true;

            auto from_nbrs = get_neighbors(from_node.id);
            auto to_nbrs   = get_neighbors(to_node.id);

            // Condición de Valle (convergencia inferior en bucle/circuito cerrado):
            // Grado >= 2 y todos los vecinos conectados en Y están por encima (y < y_nodo)
            bool from_is_valley = (from_nbrs.size() >= 2);
            for (const auto& n : from_nbrs) {
                if (n.y >= from_node.y) { from_is_valley = false; break; }
            }

            bool to_is_valley = (to_nbrs.size() >= 2);
            for (const auto& n : to_nbrs) {
                if (n.y >= to_node.y) { to_is_valley = false; break; }
            }

            if (from_is_valley) {
                horizontal_at_from = true;
            } else if (to_is_valley) {
                horizontal_at_from = false;
            } else {
                // Comprobación de ramificación hacia la derecha en layouts horizontales:
                // Si el nodo emite >= 2 aristas hacia la derecha en diferentes filas Y,
                // las ramas no pueden compartir la misma horizontal a la salida (colisionarían).
                // En ese caso, la horizontal y codo se colocan en el nodo destino.
                int outgoing_right = 0;
                for (const auto& item_other_e : graph.edges) {
                    const auto& other_e = item_other_e.getValue();
                    if (other_e.from_id == from_node.id) {
                        for (const auto& item_n : graph.nodes) {
                            const auto& cand = item_n.getValue();
                            if (cand.id == other_e.to_id && cand.x > from_node.x && cand.y != from_node.y) {
                                outgoing_right++;
                                break;
                            }
                        }
                    }
                }

                if (outgoing_right >= 2) {
                    horizontal_at_from = false;
                } else {
                    // Por defecto en geometrías verticales (árboles binarios verticales, crestas poligonales):
                    // El nodo superior (menor Y) contiene el tramo horizontal y los codos simétricos
                    horizontal_at_from = (from_node.y < to_node.y);
                }
            }

            canvas.draw_edge(
                from_node.x, from_node.y, static_cast<TX>(visible_length(from_node.label)),
                to_node.x, to_node.y, static_cast<TX>(visible_length(to_node.label)),
                horizontal_at_from, glyphs, graph.directed
            );
        }

        // 2. Colocar las etiquetas de los nodos sobre el lienzo
        for (const auto& item_n : graph.nodes) {
            const auto& node = item_n.getValue();
            canvas.write(node.x, node.y, node.label);
        }

        return canvas.render(m_os);
    }
};

#include "layouts.h"

#endif // __RENDER_H__
