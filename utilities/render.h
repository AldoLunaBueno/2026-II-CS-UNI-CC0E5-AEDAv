#ifndef __RENDER_H__
#define __RENDER_H__

#include <iostream>
#include "../containers/vector.h"
#include "../types.h"

struct VisualGraph;

// Mixin: dibujable
template <typename Traits>
class Drawable : virtual public ContainerCore<Traits> {
public:
    virtual VisualGraph sketch() const = 0;

    template <typename Renderer>
    decltype(auto) draw(Renderer&& renderer) const {
        std::lock_guard lock(this->m_mutex);
        return renderer.render(this->sketch());
    }
};

template <typename T>
using SimpleVector = BaseVector<VectorAscTraits<T>, Callable>;

// Tipos de aristas
enum class VisualEdgeType {
    LeftChild,
    RightChild
};

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
    VisualEdgeType type;
};

// Geometría y topología del grafo
struct VisualGraph {
    SimpleVector<VisualNode> nodes;
    SimpleVector<VisualEdge> edges;
    TX width = 0;       // ancho del layout
    TX height = 0;      // alto del layout
    TX cell_width = 3;  // Ancho de celda usado para el espaciado y ramificaciones
};

// Renderizador abstracto de grafos
class GraphRenderer {
public:
    virtual ~GraphRenderer() = default;
    virtual std::ostream& render(const VisualGraph&) = 0;
};

// TODO: Renderizador de grafos 2D
class SvgGraphRenderer : public GraphRenderer {};

// TODO: Generalizar a renderizador de cualquier grafo
// Renderizador de árboles binarios en consola
class ConsoleGraphRenderer : public GraphRenderer {
    std::ostream& m_os;
public:
    explicit ConsoleGraphRenderer(std::ostream& os = std::cout) : m_os(os) {}

    std::ostream& render(const VisualGraph& graph) {
        if (graph.nodes.empty()) return m_os;

        const CW RIGHT_B = '_';
        const CW ANGLE_B = '\\';
        const CW DOWN_B  = '|';
        const CW SPACE   = ' ';

        TX cell_width = graph.cell_width > 0 ? graph.cell_width : cell_width;
        TX width = graph.width;
        TX height = graph.height;

        // asegurar dimensiones válidas para el lienzo
        for (const auto& item : graph.nodes) {
            const auto& node = item.getValue();
            TX node_right = node.x + node.label.size();
            if (node_right > width) width = node_right;
            if (node.y + 1 > height) height = node.y + 1;
        }

        // matriz de caracteres para el lienzo de consola
        SimpleVector<std::string> canvas = {};
        for (std::size_t i=0; i < height; i++) {
            canvas.push_back(std::string(width, SPACE));
        } 

        // dibujar conexiones
        for (const auto& item : graph.edges) {
            const auto& edge = item.getValue();
            VisualNode from_node;
            VisualNode to_node;
            bool found_from = false;
            bool found_to = false;

            for (const auto& item : graph.nodes) {
                const auto& node = item.getValue();
                if (node.id == edge.from_id) { from_node = node; found_from = true; }
                if (node.id == edge.to_id)   { to_node = node;   found_to = true; }
            }

            if (!found_from || !found_to) continue;

            if (edge.type == VisualEdgeType::LeftChild) {
                // rama vertical hacia el hijo izquierdo
                TX x = from_node.x;
                for (TX row = from_node.y + 1; row < to_node.y; row++) {
                    if (row < height && x < width) {
                        canvas[row].value()[x] = DOWN_B;
                    }
                }
            } else if (edge.type == VisualEdgeType::RightChild) {
                // barra horizontal hacia la rama derecha
                TX y_parent = from_node.y;
                TX x_parent = from_node.x;
                TX x_child  = to_node.x;

                TX bar_start = x_parent + from_node.label.size();
                TX bar_end   = x_child - cell_width;

                for (TX col = bar_start; col <= bar_end; col++) {
                    if (y_parent < height && col < width) {
                        canvas[y_parent].value()[col] = RIGHT_B;
                    }
                }

                // rama diagonal descendente hacia el hijo derecho
                for (TX i = 0; i < cell_width - 1; i++) {
                    TX row = y_parent + 1 + i;
                    TX col = x_child - cell_width + 1 + i;
                    if (row < height && col < width) {
                        canvas[row].value()[col] = ANGLE_B;
                    }
                }
            }
        }

        // colocar las etiquetas de los nodos sobre el lienzo
        for (const auto& item : graph.nodes) {
            const auto& node = item.getValue();
            if (node.y < height && node.x < width) {
                canvas[node.y].value().replace(node.x, node.label.size(), node.label);
            }
        }

        // canvas -> stream
        for (auto& row_node : canvas) {
            auto& row_str = row_node.value();
            size_t end_pos = row_str.find_last_not_of(SPACE);
            row_str.erase(end_pos + 1);
            m_os << row_str << std::endl;
        }

        return m_os;
    }
};

#endif // __RENDERER_H__

