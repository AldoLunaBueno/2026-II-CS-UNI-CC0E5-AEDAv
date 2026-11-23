/**
 * @file tree_layout.h
 * @ingroup layouts
 * @brief Layout base CRTP (`TreeLayout`) y roles semánticos (`TreeRole`) para árboles binarios.
 */

#ifndef __LAYOUTS_TREE_LAYOUT_H__
#define __LAYOUTS_TREE_LAYOUT_H__

#include "base_layout.h"

/** @brief Roles semánticos asignados a cada nodo durante la proyección de un árbol binario. */
enum class TreeRole { Root, Leaf, Left, Right };

/** @brief Resultado parcial de layout por contornos para un subárbol binario. */
struct SubtreeLayout {
    std::vector<TX> node_ids;
    std::vector<std::pair<TX, TX>> contours; // {min_x, max_x} por nivel de profundidad
};

/**
 * @brief Calcula coordenadas X de un subárbol binario fusionando contornos dinámicos.
 * @tparam NodeVec Vector de nodos con campos `id`, `left_id`, `right_id`, `x`, `label`.
 * @tparam LeafFn Callback `(NodeVec&, NodeT&, TX label_len, SubtreeLayout&) -> void` para posicionar hojas.
 */
template <typename NodeVec, typename LeafFn>
SubtreeLayout layout_binary_contours(NodeVec& nodes, TX node_id, TX cell_width, LeafFn&& on_leaf) {
    SubtreeLayout res;
    if (node_id == -1) return res;

    auto* node = find_node(nodes, node_id);
    if (!node) return res;

    TX label_len = static_cast<TX>(visible_length(node->label));

    // Caso 1: Hoja
    if (node->left_id == -1 && node->right_id == -1) {
        on_leaf(nodes, *node, label_len, res);
        return res;
    }

    // Caso 2: Ambos hijos
    if (node->left_id != -1 && node->right_id != -1) {
        SubtreeLayout left_res = layout_binary_contours(nodes, node->left_id, cell_width, on_leaf);
        SubtreeLayout right_res = layout_binary_contours(nodes, node->right_id, cell_width, on_leaf);

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
        TX needed_branch_span = 2 * (std::max(left_label_len, label_len) + cell_width - 1);
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
        SubtreeLayout left_res = layout_binary_contours(nodes, node->left_id, cell_width, on_leaf);
        auto* left_node = find_node(nodes, node->left_id);
        TX left_label_len = static_cast<TX>(visible_length(left_node->label));
        node->x = left_node->x + left_label_len + cell_width - 1;

        res.node_ids.push_back(node_id);
        for (TX l_id : left_res.node_ids) res.node_ids.push_back(l_id);

        res.contours.push_back({node->x, node->x + label_len});
        for (const auto& c : left_res.contours) {
            res.contours.push_back(c);
        }
        return res;
    }

    // Caso 4: Solo hijo derecho
    SubtreeLayout right_res = layout_binary_contours(nodes, node->right_id, cell_width, on_leaf);
    node->x = 0;
    TX shift = label_len + cell_width - 1;
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

/**
 * @class TreeLayout
 * @brief Clase base CRTP para layouts de árboles binarios.
 *
 * Implementa `extract_topology()` (Etapa 1) para cualquier árbol binario y delega
 * `compute_coordinates()` (Etapa 3) al tipo derivado `Derived`.
 */
template <typename Derived, typename Formatter = DefaultNodeFormatter>
class TreeLayout : public BaseLayout<Derived, Formatter> {
    friend class BaseLayout<Derived, Formatter>;
public:
    using Role = TreeRole;
    using Base = BaseLayout<Derived, Formatter>;

    virtual ~TreeLayout() = default;

    TX getCellWidth() const { return m_cell_width; }
    void setCellWidth(TX w) { m_cell_width = w; }

protected:
    TX m_cell_width = 3;

    explicit TreeLayout(bool directed = false, TX cell_w = 3) : Base(directed), m_cell_width(cell_w) {}
    explicit TreeLayout(Formatter f, bool directed = false, TX cell_w = 3) : Base(std::move(f), directed), m_cell_width(cell_w) {}

    /** @brief Nodo intermedio para árboles binarios con referencias de subárbol y profundidad. */
    template <typename NodePtr>
    struct LayoutNode : DraftNode<NodePtr, TreeRole> {
        TX depth = 0;
        TX left_id = -1;
        TX right_id = -1;
        TX parent_id = -1;
    };

    /** @brief Extrae recursivamente la topología del árbol binario y asigna roles semánticos. */
    template <typename NodePtr, typename NodeVec>
    void extract_nodes(NodePtr node, TX d, TX& max_d, NodeVec& nodes, TX& next_id, TX parent_id = -1, TX child_branch = 0) const;

    /** @brief Implementación de la Etapa 1: extrae topología y aristas del árbol sin formatear etiquetas aún. */
    template <typename Tree>
    auto extract_topology_impl(const Tree& tree) const;
};

// ============================================================================
// Implementación fuera de clase: TreeLayout
// ============================================================================

template <typename Derived, typename Formatter>
template <typename NodePtr, typename NodeVec>
void TreeLayout<Derived, Formatter>::extract_nodes(NodePtr node, TX d, TX& max_d, NodeVec& nodes, TX& next_id, TX parent_id, TX child_branch) const {
    if (!node) return;
    TX my_id = next_id++;
    if (d > max_d) max_d = d;

    bool is_root = (parent_id == -1);
    bool has_left = (node->m_pChild[0] != nullptr);
    bool has_right = (node->m_pChild[1] != nullptr);
    bool is_leaf = (!has_left && !has_right);

    TreeRole role = is_root ? TreeRole::Root : (is_leaf ? TreeRole::Leaf : (child_branch < 0 ? TreeRole::Left : TreeRole::Right));

    using NodeT = typename NodeVec::value_type;
    NodeT entry;
    entry.id = my_id;
    entry.payload = node;
    entry.role = role;
    entry.depth = d;
    entry.parent_id = parent_id;
    nodes.push_back(std::move(entry));

    TX current_idx = static_cast<TX>(nodes.size()) - 1;
    if (node->m_pChild[0]) {
        nodes[current_idx].left_id = next_id;
        extract_nodes(node->m_pChild[0], d + 1, max_d, nodes, next_id, my_id, -1);
    }
    if (node->m_pChild[1]) {
        nodes[current_idx].right_id = next_id;
        extract_nodes(node->m_pChild[1], d + 1, max_d, nodes, next_id, my_id, 1);
    }
}

template <typename Derived, typename Formatter>
template <typename Tree>
auto TreeLayout<Derived, Formatter>::extract_topology_impl(const Tree& tree) const {
    using NodePtr = decltype(tree.getRoot());
    using Draft = LayoutDraft<LayoutNode<NodePtr>>;
    Draft draft;
    draft.margin_w = 2;

    auto root = tree.getRoot();
    if (!root) return draft;

    TX next_id = 0;
    extract_nodes(root, 0, draft.max_depth, draft.nodes, next_id);

    for (const auto& item : draft.nodes) {
        if (item.left_id != -1)  draft.edges.push_back(VisualEdge{item.id, item.left_id});
        if (item.right_id != -1) draft.edges.push_back(VisualEdge{item.id, item.right_id});
    }
    return draft;
}

#endif // __LAYOUTS_TREE_LAYOUT_H__
