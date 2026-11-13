#ifndef __LAYOUTS_BASE_LAYOUT_H__
#define __LAYOUTS_BASE_LAYOUT_H__

#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <type_traits>
#include "../../types.h"
#include "../render.h"
#include "../ansi_style.h"

// Formateador por defecto para cualquier tipo de nodo o valor
struct DefaultNodeFormatter {
    template <typename Node>
    std::string operator()(const Node& node) const {
        std::ostringstream oss;
        if constexpr (requires { node.getValue(); }) {
            oss << node.getValue();
        } else {
            oss << node;
        }
        return oss.str();
    }
};

// Clase base abstracta generalista para todos los layouts
// Abstrae la dirección del grafo (directed), la lógica del formateador lambda y normalización 2D
template <typename Formatter = DefaultNodeFormatter>
class BaseLayout {
protected:
    Formatter m_formatter{};
    bool m_directed = false;

    // Conversión genérica auxiliar a std::string sin copias redundantes
    template <typename V>
    static std::string to_string_val(V&& v) {
        if constexpr (std::is_convertible_v<V, std::string>) {
            return std::forward<V>(v);
        } else {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }
    }

    // Normaliza las coordenadas X a 0 y calcula las dimensiones visuales (bounding box)
    template <typename NodeContainer>
    static void normalize_and_bound(NodeContainer& nodes, TX& out_w, TX& out_h, TX margin_w = 0) {
        if (nodes.empty()) {
            out_w = 0;
            out_h = 0;
            return;
        }
        TX min_x = nodes[0].x;
        for (const auto& n : nodes) {
            if (n.x < min_x) min_x = n.x;
        }
        TX max_w = 0, max_h = 0;
        for (auto& n : nodes) {
            n.x -= min_x;
            TX right = n.x + static_cast<TX>(visible_length(n.label));
            if (right > max_w) max_w = right;
            if (n.y + 1 > max_h) max_h = n.y + 1;
        }
        out_w = max_w + margin_w;
        out_h = max_h;
    }

public:
    BaseLayout() = default;
    explicit BaseLayout(bool is_directed)
        : m_formatter{}, m_directed(is_directed) {}
    explicit BaseLayout(Formatter f, bool is_directed = false)
        : m_formatter(std::move(f)), m_directed(is_directed) {}
    virtual ~BaseLayout() = default;

    bool isDirected() const { return m_directed; }
    void setDirected(bool dir) { m_directed = dir; }

    const Formatter& getFormatter() const { return m_formatter; }
    void setFormatter(Formatter f) { m_formatter = std::move(f); }

    // Invoca el formateador pasando el nodo y su rol semántico si la lambda lo requiere
    template <typename Node, typename Role>
    std::string format_node(const Node& node, Role role) const {
        if constexpr (std::is_invocable_v<Formatter, const Node&, Role>) {
            return to_string_val(m_formatter(node, role));
        } else if constexpr (requires { node.getValue(); } && std::is_invocable_v<Formatter, decltype(node.getValue()), Role>) {
            return to_string_val(m_formatter(node.getValue(), role));
        } else {
            return format_node(node);
        }
    }

    // Invoca el formateador pasando solo el nodo por referencia constante o su valor
    template <typename Node>
    std::string format_node(const Node& node) const {
        if constexpr (std::is_invocable_v<Formatter, const Node&>) {
            return to_string_val(m_formatter(node));
        } else if constexpr (requires { node.getValue(); } && std::is_invocable_v<Formatter, decltype(node.getValue())>) {
            return to_string_val(m_formatter(node.getValue()));
        } else if constexpr (requires { node.getValue(); }) {
            return to_string_val(node.getValue());
        } else {
            return to_string_val(node);
        }
    }
};

#endif // __LAYOUTS_BASE_LAYOUT_H__
