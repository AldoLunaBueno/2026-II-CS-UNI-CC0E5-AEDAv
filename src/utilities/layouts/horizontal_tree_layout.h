/**
 * @file horizontal_tree_layout.h
 * @ingroup layouts
 * @brief Layout horizontal compacto de árbol binario (`HorizontalTreeLayout`) de izquierda a derecha.
 */

#ifndef __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__
#define __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__

#include "tree_layout.h"

/**
 * @class HorizontalTreeLayout
 * @brief Proyecta un árbol binario de izquierda a derecha asignando filas In-Order y centrando padres.
 * @tparam Formatter Functor de formateo de nodos.
 */
template <typename Formatter = DefaultNodeFormatter>
class HorizontalTreeLayout : public TreeLayout<HorizontalTreeLayout<Formatter>, Formatter> {
    friend class BaseLayout<HorizontalTreeLayout<Formatter>, Formatter>;
public:
    using Role = TreeRole;
    using Base = TreeLayout<HorizontalTreeLayout<Formatter>, Formatter>;

    explicit HorizontalTreeLayout(bool is_directed = true) : Base(is_directed) {}
    explicit HorizontalTreeLayout(Formatter f, bool is_directed = true) : Base(std::move(f), is_directed) {}

protected:
    template <typename Draft>
    void compute_coordinates_impl(Draft& draft) const;
};

// ============================================================================
// Implementación fuera de clase: HorizontalTreeLayout
// ============================================================================

template <typename Formatter>
template <typename Draft>
void HorizontalTreeLayout<Formatter>::compute_coordinates_impl(Draft& draft) const {
    auto& nodes = draft.nodes;
    TX max_depth = draft.max_depth;
    if (nodes.empty()) return;

    TX current_row = 0;
    auto assign_in_order_y = [&](auto& self, TX id) -> void {
        if (id == -1) return;
        auto* node_inf = find_node(nodes, id);
        if (!node_inf) return;

        self(self, node_inf->left_id);
        node_inf->y = current_row++;
        self(self, node_inf->right_id);
    };
    assign_in_order_y(assign_in_order_y, 0);

    auto center_parents = [&](auto& self, TX id) -> void {
        if (id == -1) return;
        auto* node_inf = find_node(nodes, id);
        if (!node_inf) return;

        self(self, node_inf->left_id);
        self(self, node_inf->right_id);

        if (node_inf->left_id != -1 && node_inf->right_id != -1) {
            auto* left_inf = find_node(nodes, node_inf->left_id);
            auto* right_inf = find_node(nodes, node_inf->right_id);
            if (left_inf && right_inf) {
                TX mid = (left_inf->y + right_inf->y) / 2;
                if (mid > left_inf->y && mid < right_inf->y) {
                    node_inf->y = mid;
                }
            }
        }
    };
    center_parents(center_parents, 0);

    std::vector<TX> level_x(max_depth + 1, 0);
    for (TX d = 0; d < max_depth; ++d) {
        TX max_span = 0;
        for (const auto& p : nodes) {
            if (p.depth == d) {
                for (TX child_id : {p.left_id, p.right_id}) {
                    if (child_id != -1) {
                        auto* c = find_node(nodes, child_id);
                        if (c) {
                            TX dy = std::abs(c->y - p.y);
                            TX span = static_cast<TX>(visible_length(p.label)) + dy + (this->m_directed ? 2 : 1);
                            if (span > max_span) max_span = span;
                        }
                    }
                }
            }
        }
        if (max_span == 0) max_span = 4;
        level_x[d + 1] = level_x[d] + max_span;
    }

    for (auto& item : nodes) {
        item.x = level_x[item.depth];
    }
}

// Deduction guides para HorizontalTreeLayout
template <typename F>
requires (!std::is_same_v<std::decay_t<F>, bool>)
HorizontalTreeLayout(F) -> HorizontalTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_same_v<std::decay_t<F>, bool>)
HorizontalTreeLayout(F, bool) -> HorizontalTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__
