#ifndef __LAYOUTS_VERTICAL_TREE_LAYOUT_H__
#define __LAYOUTS_VERTICAL_TREE_LAYOUT_H__

#include "tree_layout.h"

// Disposición vertical compacta de árbol binario con contornos dinámicos
template <typename Formatter = DefaultNodeFormatter>
class VerticalTreeLayout : public TreeLayout<Formatter> {
public:
    using Role = TreeRole;
    using Base = TreeLayout<Formatter>;
    using typename Base::LayoutNode;
    using Base::m_cell_width;
    using Base::find_node;

    explicit VerticalTreeLayout(TX cell_w = 3, bool is_directed = false)
        : Base(is_directed, cell_w) {}

    explicit VerticalTreeLayout(Formatter f, TX cell_w = 3, bool is_directed = false)
        : Base(std::move(f), is_directed, cell_w) {}

    template <typename F>
    auto with_formatter(F&& f) const {
        return VerticalTreeLayout<std::decay_t<F>>(std::forward<F>(f), m_cell_width, this->m_directed);
    }

private:
    struct SubtreeLayout {
        std::vector<TX> node_ids;
        std::vector<std::pair<TX, TX>> contours; // {min_x, max_x} por nivel de profundidad
    };

    SubtreeLayout layout_subtree(std::vector<LayoutNode>& nodes, TX node_id) const {
        SubtreeLayout res;
        if (node_id == -1) return res;

        auto* node = find_node(nodes, node_id);
        if (!node) return res;

        TX label_len = static_cast<TX>(visible_length(node->label));

        // Caso 1: Hoja
        if (node->left_id == -1 && node->right_id == -1) {
            node->x = 0;
            res.node_ids.push_back(node_id);
            res.contours.push_back({0, label_len});
            return res;
        }

        // Caso 2: Ambos hijos
        if (node->left_id != -1 && node->right_id != -1) {
            SubtreeLayout left_res = layout_subtree(nodes, node->left_id);
            SubtreeLayout right_res = layout_subtree(nodes, node->right_id);

            auto* left_node = find_node(nodes, node->left_id);
            auto* right_node = find_node(nodes, node->right_id);
            TX left_label_len = static_cast<TX>(visible_length(left_node->label));

            // 1. Separación horizontal según contornos de los subárboles
            TX min_gap = 2;
            TX S = 0;
            size_t common_depth = std::min(left_res.contours.size(), right_res.contours.size());
            for (size_t d = 0; d < common_depth; ++d) {
                TX gap_needed = left_res.contours[d].second + min_gap - right_res.contours[d].first;
                if (gap_needed > S) {
                    S = gap_needed;
                }
            }

            // 2. Separación para que las ramas diagonales del padre no colisionen
            TX needed_branch_span = 2 * (std::max(left_label_len, label_len) + m_cell_width - 1);
            TX curr_span = (right_node->x + S) - left_node->x;
            if (curr_span < needed_branch_span) {
                S += (needed_branch_span - curr_span);
            }

            // Desplazar todos los nodos del subárbol derecho
            for (TX r_id : right_res.node_ids) {
                auto* rn = find_node(nodes, r_id);
                if (rn) rn->x += S;
            }

            // Centrar el padre exactamente sobre sus dos hijos
            node->x = (left_node->x + right_node->x) / 2;

            // Fusionar contornos
            res.node_ids.push_back(node_id);
            for (TX l_id : left_res.node_ids) res.node_ids.push_back(l_id);
            for (TX r_id : right_res.node_ids) res.node_ids.push_back(r_id);

            res.contours.push_back({node->x, node->x + label_len});
            size_t max_d = std::max(left_res.contours.size(), right_res.contours.size());
            for (size_t d = 0; d < max_d; ++d) {
                TX min_x = (d < left_res.contours.size()) ? left_res.contours[d].first : (right_res.contours[d].first + S);
                TX max_x = (d < right_res.contours.size()) ? (right_res.contours[d].second + S) : left_res.contours[d].second;
                res.contours.push_back({min_x, max_x});
            }
            return res;
        }

        // Caso 3: Solo hijo izquierdo
        if (node->left_id != -1) {
            SubtreeLayout left_res = layout_subtree(nodes, node->left_id);
            auto* left_node = find_node(nodes, node->left_id);
            TX left_label_len = static_cast<TX>(visible_length(left_node->label));
            node->x = left_node->x + left_label_len + m_cell_width - 1;

            res.node_ids.push_back(node_id);
            for (TX l_id : left_res.node_ids) res.node_ids.push_back(l_id);

            res.contours.push_back({node->x, node->x + label_len});
            for (const auto& c : left_res.contours) {
                res.contours.push_back(c);
            }
            return res;
        }

        // Caso 4: Solo hijo derecho
        SubtreeLayout right_res = layout_subtree(nodes, node->right_id);
        node->x = 0;
        TX shift = label_len + m_cell_width - 1;
        for (TX r_id : right_res.node_ids) {
            auto* rn = find_node(nodes, r_id);
            if (rn) rn->x += shift;
        }

        res.node_ids.push_back(node_id);
        for (TX r_id : right_res.node_ids) res.node_ids.push_back(r_id);

        res.contours.push_back({node->x, node->x + label_len});
        for (const auto& c : right_res.contours) {
            res.contours.push_back({c.first + shift, c.second + shift});
        }
        return res;
    }

protected:
    void compute_coordinates(std::vector<LayoutNode>& nodes, TX /*max_depth*/) const override {
        if (nodes.empty()) return;

        for (auto& n : nodes) {
            n.y = n.depth * m_cell_width;
        }

        layout_subtree(nodes, 0);
    }
};

// Deduction guides para VerticalTreeLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
VerticalTreeLayout(F) -> VerticalTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
VerticalTreeLayout(F, TX, bool) -> VerticalTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_VERTICAL_TREE_LAYOUT_H__
