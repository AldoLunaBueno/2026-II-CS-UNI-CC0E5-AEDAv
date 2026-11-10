#ifndef __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__
#define __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__

#include "tree_layout.h"

// Disposición horizontal compacta con centrado dinámico en bifurcaciones
template <typename Formatter = DefaultNodeFormatter>
class HorizontalTreeLayout : public TreeLayout<Formatter> {
public:
    using Role = TreeRole;
    using Base = TreeLayout<Formatter>;
    using typename Base::LayoutNode;
    using Base::find_node;

    explicit HorizontalTreeLayout(bool is_directed = true)
        : Base(is_directed) {}

    explicit HorizontalTreeLayout(Formatter f, bool is_directed = true)
        : Base(std::move(f), is_directed) {}

    template <typename F>
    auto with_formatter(F&& f) const {
        return HorizontalTreeLayout<std::decay_t<F>>(std::forward<F>(f), this->m_directed);
    }

protected:
    void compute_coordinates(std::vector<LayoutNode>& nodes, TX max_depth) const override {
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
};

// Deduction guides para HorizontalTreeLayout
template <typename F>
requires (!std::is_same_v<std::decay_t<F>, bool>)
HorizontalTreeLayout(F) -> HorizontalTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_same_v<std::decay_t<F>, bool>)
HorizontalTreeLayout(F, bool) -> HorizontalTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_HORIZONTAL_TREE_LAYOUT_H__
