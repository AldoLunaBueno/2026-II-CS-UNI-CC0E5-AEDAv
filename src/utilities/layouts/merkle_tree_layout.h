/**
 * @file merkle_tree_layout.h
 * @ingroup layouts
 * @brief Layout especializado (`MerkleTreeLayout`) para árboles de Merkle con hojas de datos vinculadas.
 */

#ifndef __LAYOUTS_MERKLE_TREE_LAYOUT_H__
#define __LAYOUTS_MERKLE_TREE_LAYOUT_H__

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <utility>
#include "tree_layout.h"

/** @brief Roles semánticos para nodos de un árbol de Merkle (incluyendo clones y datos crudos). */
enum class MerkleRole { Root, Internal, Leaf, CloneLeaf, Data, CloneData };

/** @brief Formateador por defecto para `MerkleTreeLayout`: abrevia hashes y envuelve datos entre corchetes. */
struct DefaultMerkleFormatter {
    size_t hash_len = 8;

    std::string operator()(const std::string& text, MerkleRole role) const {
        if (role == MerkleRole::Data)      return "[" + text + "]";
        if (role == MerkleRole::CloneData) return "[" + text + " (copia)]";
        return (text.size() > hash_len) ? text.substr(0, hash_len) : text;
    }
};

/**
 * @class MerkleTreeLayout
 * @brief Proyecta un `MerkleTree` abreviando hashes y conectando verticalmente cada hoja con su dato original.
 * @tparam Formatter Functor de formateo de nodos; por defecto proyecta texto plano sin colorear.
 */
template <typename Formatter = DefaultMerkleFormatter>
class MerkleTreeLayout : public BaseLayout<MerkleTreeLayout<Formatter>, Formatter> {
public:
    using Role = MerkleRole;
    using Base = BaseLayout<MerkleTreeLayout<Formatter>, Formatter>;
    friend class BaseLayout<MerkleTreeLayout<Formatter>, Formatter>;

private:
    TX m_cell_width = 3;

    struct MerkleNode : DraftNode<std::string, MerkleRole> {
        TX depth = 0;
        TX left_id = -1;
        TX right_id = -1;
        TX parent_id = -1;
        TX data_node_id = -1;
    };

    using Draft = LayoutDraft<MerkleNode>;

public:
    explicit MerkleTreeLayout(size_t hash_len = 8, TX cell_w = 3, bool is_directed = false)
        : Base(DefaultMerkleFormatter{hash_len}, is_directed), m_cell_width(cell_w) {}
    explicit MerkleTreeLayout(Formatter f, TX cell_w = 3, bool is_directed = false)
        : Base(std::move(f), is_directed), m_cell_width(cell_w) {}

private:
    template <typename MerkleTreeType>
    Draft extract_topology_impl(const MerkleTreeType& tree) const;

    void compute_coordinates_impl(Draft& draft) const;
};

// ============================================================================
// Implementación fuera de clase: MerkleTreeLayout
// ============================================================================

template <typename Formatter>
template <typename MerkleTreeType>
typename MerkleTreeLayout<Formatter>::Draft
MerkleTreeLayout<Formatter>::extract_topology_impl(const MerkleTreeType& tree) const {
    Draft draft;
    draft.margin_w = 2;

    auto root = tree.getRoot();
    if (!root) return draft;

    const auto& raw_data = tree.getRawData();
    size_t num_data = raw_data.size();

    // Calcular la profundidad de las hojas de transacciones
    TX max_tree_depth = 0;
    {
        auto curr = root;
        while (curr && (curr->m_pChild[0] || curr->m_pChild[1])) {
            curr = curr->m_pChild[0] ? curr->m_pChild[0] : curr->m_pChild[1];
            ++max_tree_depth;
        }
    }

    // 1. Extraer recursivamente los nodos hash del árbol en pre-orden
    TX max_depth = 0;
    TX next_id = 0;

    auto extract_hashes = [&](auto& self, auto node_ptr, TX d, TX parent_id) -> TX {
        if (!node_ptr) return -1;
        TX my_id = next_id++;
        if (d > max_depth) max_depth = d;

        bool is_root = (parent_id == -1);
        bool is_leaf = (!node_ptr->m_pChild[0] && !node_ptr->m_pChild[1] && d == max_tree_depth);

        MerkleRole role = is_root ? MerkleRole::Root
                        : (is_leaf ? MerkleRole::Leaf : MerkleRole::Internal);

        MerkleNode node;
        node.id = my_id;
        node.payload = node_ptr->getValue();
        node.role = role;
        node.depth = d;
        node.parent_id = parent_id;

        draft.nodes.push_back(std::move(node));
        size_t curr_idx = draft.nodes.size() - 1;

        if (node_ptr->m_pChild[0]) {
            draft.nodes[curr_idx].left_id = self(self, node_ptr->m_pChild[0], d + 1, my_id);
        }
        if (node_ptr->m_pChild[1]) {
            draft.nodes[curr_idx].right_id = self(self, node_ptr->m_pChild[1], d + 1, my_id);
        }
        return my_id;
    };
    extract_hashes(extract_hashes, root, 0, -1);

    // 2. Identificar hojas en orden In-Order (de izquierda a derecha)
    std::vector<TX> leaf_ids;
    auto collect_leaves = [&](auto& self, TX id) -> void {
        if (id == -1) return;
        auto* n = find_node(draft.nodes, id);
        if (!n) return;
        if (n->depth == max_tree_depth) {
            leaf_ids.push_back(id);
            return;
        }
        self(self, n->left_id);
        self(self, n->right_id);
    };
    collect_leaves(collect_leaves, 0);

    // 3. Crear nodos de datos y vincularlos a cada hoja (asignando roles Leaf/CloneLeaf y Data/CloneData)
    std::vector<std::pair<TX, TX>> vertical_edges;
    for (size_t i = 0; i < leaf_ids.size(); ++i) {
        TX leaf_id = leaf_ids[i];
        bool is_clone = (i >= num_data);
        std::string d_text = is_clone ? (num_data > 0 ? raw_data.back().getValue() : "") : raw_data[i].getValue();

        auto* leaf_node = find_node(draft.nodes, leaf_id);
        if (leaf_node && is_clone) {
            leaf_node->role = MerkleRole::CloneLeaf;
        }

        MerkleRole data_role = is_clone ? MerkleRole::CloneData : MerkleRole::Data;
        TX data_id = next_id++;
        TX leaf_depth = leaf_node ? leaf_node->depth : max_depth;

        if (leaf_node) leaf_node->data_node_id = data_id;
        MerkleNode data_node;
        data_node.id = data_id;
        data_node.payload = std::move(d_text);
        data_node.role = data_role;
        data_node.depth = leaf_depth + 1;
        data_node.parent_id = leaf_id;
        draft.nodes.push_back(std::move(data_node));
        vertical_edges.push_back({leaf_id, data_id});
    }

    // 4. Registrar aristas en el borrador
    for (const auto& item : draft.nodes) {
        if (item.left_id != -1) {
            draft.edges.push_back(VisualEdge{item.id, item.left_id});
        }
        if (item.right_id != -1) {
            draft.edges.push_back(VisualEdge{item.id, item.right_id});
        }
    }
    for (const auto& ve : vertical_edges) {
        draft.edges.push_back(VisualEdge{ve.first, ve.second});
    }

    draft.max_depth = max_depth;
    return draft;
}

template <typename Formatter>
void MerkleTreeLayout<Formatter>::compute_coordinates_impl(Draft& draft) const {
    if (draft.nodes.empty()) return;

    // 1. Asignar coordenadas Y iniciales
    for (auto& n : draft.nodes) {
        if (n.role == MerkleRole::Data || n.role == MerkleRole::CloneData) {
            auto* parent_leaf = find_node(draft.nodes, n.parent_id);
            n.y = (parent_leaf ? parent_leaf->depth : draft.max_depth) * m_cell_width + 2;
        } else {
            n.y = n.depth * m_cell_width;
        }
    }

    // 2. Calcular coordenadas X con expansión de contornos
    layout_binary_contours(draft.nodes, 0, m_cell_width, [](auto& nodes, auto& node, TX label_len, SubtreeLayout& res) {
        if (node.data_node_id != -1) {
            if (auto* dnode = find_node(nodes, node.data_node_id)) {
                TX d_len = static_cast<TX>(visible_length(dnode->label));
                TX center = std::max(label_len, d_len) / 2;
                node.x = center - label_len / 2;
                dnode->x = center - d_len / 2;

                res.node_ids.push_back(node.id);
                res.node_ids.push_back(node.data_node_id);

                res.contours.push_back({node.x, node.x + label_len});
                res.contours.push_back({dnode->x, dnode->x + d_len});
                return;
            }
        }
        node.x = 0;
        res.node_ids.push_back(node.id);
        res.contours.push_back({0, label_len});
    });
}

// Deduction guides para MerkleTreeLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
MerkleTreeLayout(F) -> MerkleTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
MerkleTreeLayout(F, TX, bool) -> MerkleTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_MERKLE_TREE_LAYOUT_H__
