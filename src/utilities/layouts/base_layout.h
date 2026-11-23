/**
 * @file base_layout.h
 * @ingroup layouts
 * @brief Clase base `BaseLayout` y formateador por defecto `DefaultNodeFormatter` para proyección 2D.
 */

#ifndef __LAYOUTS_BASE_LAYOUT_H__
#define __LAYOUTS_BASE_LAYOUT_H__

#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <type_traits>
#include "../../types.h"
#include "../render.h"
#include "../ansi_metrics.h"

/** @brief Formateador por defecto que convierte cualquier nodo o valor a `std::string`. */
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

/** @brief Nodo base intermedio con carga útil (`payload`), rol semántico, etiqueta y posición 2D. */
template <typename Payload, typename Role>
struct DraftNode {
    TX id = -1;
    Payload payload{};
    Role role{};
    std::string label;
    TX x = 0;
    TX y = 0;
};

/** @brief Contenedor intermedio de nodos y aristas compartido por todas las etapas del layout. */
template <typename NodeT>
struct LayoutDraft {
    std::vector<NodeT> nodes;
    std::vector<VisualEdge> edges;
    TX max_depth = 0;
    TX margin_w = 0;
};

/** @brief Búsqueda O(1) basada en ID indexado en un vector de nodos, con fallback lineal seguro. */
template <typename NodeT>
NodeT* find_node(std::vector<NodeT>& nodes, TX id) {
    if (id >= 0 && static_cast<size_t>(id) < nodes.size() && nodes[id].id == id) {
        return &nodes[id];
    }
    for (auto& item : nodes) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

template <typename NodeT>
const NodeT* find_node(const std::vector<NodeT>& nodes, TX id) {
    if (id >= 0 && static_cast<size_t>(id) < nodes.size() && nodes[id].id == id) {
        return &nodes[id];
    }
    for (const auto& item : nodes) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

/**
 * @class BaseLayout
 * @brief Clase base CRTP que impone el pipeline de 4 etapas para proyección a `VisualGraph`.
 *
 * 1. `extract_topology(container)` (delega en `Derived::extract_topology_impl`)
 * 2. `format_labels(draft)` (`BaseLayout`)
 * 3. `compute_coordinates(draft)` (delega en `Derived::compute_coordinates_impl`)
 * 4. `finalize_graph(draft)` (`BaseLayout`)
 *
 * @tparam Derived Tipo derivado concreto (CRTP).
 * @tparam Formatter Functor o lambda encargada de generar la etiqueta visual de cada nodo.
 *
 * @image html layout_hierarchy.svg "Arquitectura modular de layouts 2D y roles"
 */
template <typename Derived, typename Formatter = DefaultNodeFormatter>
class BaseLayout {
protected:
    Formatter m_formatter{};
    bool m_directed = false;

    const Derived& derived() const { return static_cast<const Derived&>(*this); }

    /** @brief Convierte un valor genérico a `std::string` evitando copias si ya es convertible. */
    template <typename V>
    static std::string to_string_val(V&& v);

    /** @brief Desplaza las coordenadas `x` para iniciar en 0 y calcula el bounding box (`out_w`, `out_h`). */
    template <typename NodeContainer>
    static void normalize_and_bound(NodeContainer& nodes, TX& out_w, TX& out_h, TX margin_w = 0);

    /** @brief Etapa 1: extrae topología y aristas del contenedor (delega en `Derived::extract_topology_impl`). */
    template <typename Container>
    auto extract_topology(const Container& container) const { return derived().extract_topology_impl(container); }

    /** @brief Etapa 2: genera `n.label` para cada nodo del borrador usando `format_node()`. */
    template <typename Draft>
    void format_labels(Draft& draft) const;

    /** @brief Etapa 3: calcula las coordenadas 2D de cada nodo (delega en `Derived::compute_coordinates_impl`). */
    template <typename Draft>
    void compute_coordinates(Draft& draft) const { derived().compute_coordinates_impl(draft); }

    /** @brief Etapa 4: normaliza coordenadas y construye el `VisualGraph` final. */
    template <typename Draft>
    VisualGraph finalize_graph(Draft& draft) const;

public:
    BaseLayout() = default;
    explicit BaseLayout(bool is_directed) : m_formatter{}, m_directed(is_directed) {}
    explicit BaseLayout(Formatter f, bool is_directed = false) : m_formatter(std::move(f)), m_directed(is_directed) {}
    virtual ~BaseLayout() = default;

    bool isDirected() const { return m_directed; }
    void setDirected(bool dir) { m_directed = dir; }

    const Formatter& getFormatter() const { return m_formatter; }
    void setFormatter(Formatter f) { m_formatter = std::move(f); }

    /** @brief Template Method CRTP que ejecuta las 4 etapas del layout en orden fijo. */
    template <typename Container>
    VisualGraph operator()(const Container& container) const;

    /**
     * @brief Formatea un nodo invocando `m_formatter` con `(node, role)` o `(node)` según su firma.
     */
    template <typename Node, typename Role>
    std::string format_node(const Node& node, Role role) const;
};

// ============================================================================
// Implementación fuera de clase: BaseLayout
// ============================================================================

template <typename Derived, typename Formatter>
template <typename V>
std::string BaseLayout<Derived, Formatter>::to_string_val(V&& v) {
    if constexpr (std::is_convertible_v<V, std::string>) {
        return std::forward<V>(v);
    } else {
        std::ostringstream oss;
        oss << v;
        return oss.str();
    }
}

template <typename Derived, typename Formatter>
template <typename NodeContainer>
void BaseLayout<Derived, Formatter>::normalize_and_bound(NodeContainer& nodes, TX& out_w, TX& out_h, TX margin_w) {
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

template <typename Derived, typename Formatter>
template <typename Draft>
void BaseLayout<Derived, Formatter>::format_labels(Draft& draft) const {
    for (auto& n : draft.nodes) {
        if constexpr (std::is_pointer_v<decltype(n.payload)>) {
            n.label = format_node(*n.payload, n.role);
        } else {
            n.label = format_node(n.payload, n.role);
        }
    }
}

template <typename Derived, typename Formatter>
template <typename Draft>
VisualGraph BaseLayout<Derived, Formatter>::finalize_graph(Draft& draft) const {
    VisualGraph graph;
    graph.directed = m_directed;
    normalize_and_bound(draft.nodes, graph.width, graph.height, draft.margin_w);
    for (auto& n : draft.nodes) {
        graph.nodes.push_back(VisualNode{n.id, std::move(n.label), n.x, n.y});
    }
    for (const auto& e : draft.edges) {
        graph.edges.push_back(e);
    }
    return graph;
}

template <typename Derived, typename Formatter>
template <typename Container>
VisualGraph BaseLayout<Derived, Formatter>::operator()(const Container& container) const {
    auto draft = extract_topology(container);
    if (draft.nodes.empty()) {
        VisualGraph empty_graph;
        empty_graph.directed = m_directed;
        return empty_graph;
    }
    format_labels(draft);
    compute_coordinates(draft);
    return finalize_graph(draft);
}

template <typename Derived, typename Formatter>
template <typename Node, typename Role>
std::string BaseLayout<Derived, Formatter>::format_node(const Node& node, Role role) const {
    if constexpr (std::is_invocable_v<Formatter, const Node&, Role>) {
        return to_string_val(m_formatter(node, role));
    } else if constexpr (std::is_invocable_v<Formatter, const Node&>) {
        return to_string_val(m_formatter(node));
    } else {
        return DefaultNodeFormatter{}(node);
    }
}

#endif // __LAYOUTS_BASE_LAYOUT_H__
