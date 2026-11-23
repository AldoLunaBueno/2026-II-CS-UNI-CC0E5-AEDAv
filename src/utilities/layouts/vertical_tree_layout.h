/**
 * @file vertical_tree_layout.h
 * @ingroup layouts
 * @brief Layout vertical compacto de árbol binario (`VerticalTreeLayout`) mediante contornos dinámicos.
 */

#ifndef __LAYOUTS_VERTICAL_TREE_LAYOUT_H__
#define __LAYOUTS_VERTICAL_TREE_LAYOUT_H__

#include "tree_layout.h"

/**
 * @class VerticalTreeLayout
 * @brief Proyecta un árbol binario de arriba hacia abajo evitando colisiones mediante fusión de contornos.
 * @tparam Formatter Functor de formateo de nodos.
 */
template <typename Formatter = DefaultNodeFormatter>
class VerticalTreeLayout : public TreeLayout<VerticalTreeLayout<Formatter>, Formatter> {
    friend class BaseLayout<VerticalTreeLayout<Formatter>, Formatter>;
public:
    using Role = TreeRole;
    using Base = TreeLayout<VerticalTreeLayout<Formatter>, Formatter>;
    using Base::m_cell_width;

    explicit VerticalTreeLayout(TX cell_w = 3, bool is_directed = false) : Base(is_directed, cell_w) {}
    explicit VerticalTreeLayout(Formatter f, TX cell_w = 3, bool is_directed = false) : Base(std::move(f), is_directed, cell_w) {}

protected:
    template <typename Draft>
    void compute_coordinates_impl(Draft& draft) const;
};

// ============================================================================
// Implementación fuera de clase: VerticalTreeLayout
// ============================================================================

template <typename Formatter>
template <typename Draft>
void VerticalTreeLayout<Formatter>::compute_coordinates_impl(Draft& draft) const {
    auto& nodes = draft.nodes;
    if (nodes.empty()) return;

    for (auto& n : nodes) {
        n.y = n.depth * m_cell_width;
    }

    layout_binary_contours(nodes, 0, m_cell_width, [](auto&, auto& node, TX label_len, SubtreeLayout& res) {
        node.x = 0;
        res.node_ids.push_back(node.id);
        res.contours.push_back({0, label_len});
    });
}

// Deduction guides para VerticalTreeLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
VerticalTreeLayout(F) -> VerticalTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
VerticalTreeLayout(F, TX, bool) -> VerticalTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_VERTICAL_TREE_LAYOUT_H__
