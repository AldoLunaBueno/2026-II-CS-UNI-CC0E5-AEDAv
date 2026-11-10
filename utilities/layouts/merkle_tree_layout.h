#ifndef __LAYOUTS_MERKLE_TREE_LAYOUT_H__
#define __LAYOUTS_MERKLE_TREE_LAYOUT_H__

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <utility>
#include "base_layout.h"

// Roles semánticos para nodos de Merkle Tree
enum class MerkleRole { Root, Internal, Leaf, CloneLeaf, Data, CloneData };

constexpr std::string_view to_css_class(MerkleRole r) {
    switch (r) {
        case MerkleRole::Root:      return "root";
        case MerkleRole::Internal:  return "internal";
        case MerkleRole::Leaf:      return "leaf";
        case MerkleRole::CloneLeaf: return "clone-leaf";
        case MerkleRole::Data:      return "data";
        case MerkleRole::CloneData: return "clone-data";
    }
    return "";
}

// ============================================================================
// Layout para Árbol de Merkle (MerkleTreeLayout)
// Dispone los hashes del árbol binario con longitud abreviada y conecta verticalmente
// cada hoja con su respectivo dato original (m_raw_data[i]).
// Por defecto proyecta texto plano sin colorear; el cliente inyecta estilos mediante MerkleRole.
// ============================================================================

template <typename Formatter = DefaultNodeFormatter>
class MerkleTreeLayout : public BaseLayout<Formatter> {
public:
    using Role = MerkleRole;
    using Base = BaseLayout<Formatter>;
    using Base::m_directed;
    using Base::format_node;
    using Base::normalize_and_bound;

private:
    TX m_cell_width = 3;
    size_t m_hash_len = 8;

    std::string format_hash(const std::string& raw_h, MerkleRole role) const {
        if constexpr (std::is_same_v<Formatter, DefaultNodeFormatter>) {
            return (raw_h.size() > m_hash_len) ? raw_h.substr(0, m_hash_len) : raw_h;
        } else {
            return this->format_node(raw_h, role);
        }
    }

    std::string format_data(const std::string& d_text, bool is_clone, MerkleRole role) const {
        if constexpr (std::is_same_v<Formatter, DefaultNodeFormatter>) {
            return is_clone ? ("[" + d_text + " (copia)]") : ("[" + d_text + "]");
        } else {
            return this->format_node(d_text, role);
        }
    }

    struct MerkleNode {
        TX id = -1;
        std::string raw_value;
        std::string label;
        TX x = 0;
        TX y = 0;
        TX depth = 0;
        TX left_id = -1;
        TX right_id = -1;
        TX parent_id = -1;
        TX data_node_id = -1;
        MerkleRole role = MerkleRole::Internal;
    };

    static MerkleNode* find_mnode(std::vector<MerkleNode>& nodes, TX id) {
        if (id >= 0 && static_cast<size_t>(id) < nodes.size() && nodes[id].id == id) {
            return &nodes[id];
        }
        for (auto& item : nodes) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    struct SubtreeLayout {
        std::vector<TX> node_ids;
        std::vector<std::pair<TX, TX>> contours; // {min_x, max_x} por nivel de profundidad
    };

    SubtreeLayout layout_subtree(std::vector<MerkleNode>& nodes, TX node_id) const {
        SubtreeLayout res;
        if (node_id == -1) return res;

        auto* node = find_mnode(nodes, node_id);
        if (!node) return res;

        TX label_len = static_cast<TX>(visible_length(node->label));

        // Caso 1: Hoja del árbol Merkle (posiblemente con un nodo de datos vinculado debajo)
        if (node->left_id == -1 && node->right_id == -1) {
            if (node->data_node_id != -1) {
                auto* dnode = find_mnode(nodes, node->data_node_id);
                if (dnode) {
                    TX d_len = static_cast<TX>(visible_length(dnode->label));
                    TX center = std::max(label_len, d_len) / 2;
                    node->x  = center - label_len / 2;
                    dnode->x = center - d_len / 2;

                    res.node_ids.push_back(node_id);
                    res.node_ids.push_back(node->data_node_id);

                    res.contours.push_back({node->x, node->x + label_len});
                    res.contours.push_back({dnode->x, dnode->x + d_len});
                    return res;
                }
            }
            node->x = 0;
            res.node_ids.push_back(node_id);
            res.contours.push_back({0, label_len});
            return res;
        }

        // Caso 2: Ambos hijos (árbol binario)
        if (node->left_id != -1 && node->right_id != -1) {
            SubtreeLayout left_res = layout_subtree(nodes, node->left_id);
            SubtreeLayout right_res = layout_subtree(nodes, node->right_id);

            auto* left_node = find_mnode(nodes, node->left_id);
            auto* right_node = find_mnode(nodes, node->right_id);
            TX left_label_len = static_cast<TX>(visible_length(left_node->label));

            // 1. Separación horizontal según contornos de subárboles (incluyendo datos en profundidad)
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

            // Desplazar todos los nodos del subárbol derecho (hashes y datos asociados)
            for (TX r_id : right_res.node_ids) {
                auto* rn = find_mnode(nodes, r_id);
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

        // Casos degenerados con un solo hijo
        if (node->left_id != -1) {
            SubtreeLayout left_res = layout_subtree(nodes, node->left_id);
            auto* left_node = find_mnode(nodes, node->left_id);
            TX left_label_len = static_cast<TX>(visible_length(left_node->label));
            node->x = left_node->x + left_label_len + m_cell_width - 1;

            res.node_ids.push_back(node_id);
            for (TX l_id : left_res.node_ids) res.node_ids.push_back(l_id);

            res.contours.push_back({node->x, node->x + label_len});
            for (const auto& c : left_res.contours) res.contours.push_back(c);
            return res;
        }

        SubtreeLayout right_res = layout_subtree(nodes, node->right_id);
        node->x = 0;
        TX shift = label_len + m_cell_width - 1;
        for (TX r_id : right_res.node_ids) {
            auto* rn = find_mnode(nodes, r_id);
            if (rn) rn->x += shift;
        }
        res.node_ids.push_back(node_id);
        for (TX r_id : right_res.node_ids) res.node_ids.push_back(r_id);
        res.contours.push_back({node->x, node->x + label_len});
        for (const auto& c : right_res.contours) res.contours.push_back({c.first + shift, c.second + shift});
        return res;
    }

public:
    explicit MerkleTreeLayout(size_t hash_len = 8, TX cell_w = 3, bool is_directed = false)
        : Base(is_directed), m_cell_width(cell_w), m_hash_len(hash_len) {}

    explicit MerkleTreeLayout(Formatter f, size_t hash_len = 8, TX cell_w = 3, bool is_directed = false)
        : Base(std::move(f), is_directed), m_cell_width(cell_w), m_hash_len(hash_len) {}

    template <typename F>
    auto with_formatter(F&& f) const {
        return MerkleTreeLayout<std::decay_t<F>>(std::forward<F>(f), m_hash_len, m_cell_width, this->m_directed);
    }

    auto with_hash_length(size_t len) const {
        auto copy = *this;
        copy.m_hash_len = len;
        return copy;
    }

    template <typename MerkleTreeType>
    VisualGraph operator()(const MerkleTreeType& tree) const {
        VisualGraph graph;
        graph.directed = this->m_directed;

        auto root = tree.getRoot();
        if (!root) return graph;

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
        std::vector<MerkleNode> nodes;
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

            std::string raw_h = node_ptr->getValue();
            std::string label = this->format_hash(raw_h, role);

            nodes.push_back({
                .id = my_id,
                .raw_value = std::move(raw_h),
                .label = std::move(label),
                .depth = d,
                .parent_id = parent_id,
                .role = role
            });
            size_t curr_idx = nodes.size() - 1;

            if (node_ptr->m_pChild[0]) {
                nodes[curr_idx].left_id = self(self, node_ptr->m_pChild[0], d + 1, my_id);
            }
            if (node_ptr->m_pChild[1]) {
                nodes[curr_idx].right_id = self(self, node_ptr->m_pChild[1], d + 1, my_id);
            }
            return my_id;
        };
        extract_hashes(extract_hashes, root, 0, -1);

        // 2. Identificar hojas en orden In-Order (de izquierda a derecha)
        std::vector<TX> leaf_ids;
        auto collect_leaves = [&](auto& self, TX id) -> void {
            if (id == -1) return;
            auto* n = find_mnode(nodes, id);
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
            std::string d_text = is_clone ? (num_data > 0 ? raw_data.back() : "") : raw_data[i];

            auto* leaf_node = find_mnode(nodes, leaf_id);
            if (leaf_node && is_clone) {
                leaf_node->role = MerkleRole::CloneLeaf;
                leaf_node->label = this->format_hash(leaf_node->raw_value, leaf_node->role);
            }

            MerkleRole data_role = is_clone ? MerkleRole::CloneData : MerkleRole::Data;
            std::string d_label = this->format_data(d_text, is_clone, data_role);

            TX data_id = next_id++;
            TX leaf_depth = leaf_node ? leaf_node->depth : max_depth;

            if (leaf_node) leaf_node->data_node_id = data_id;
            nodes.push_back({
                .id = data_id,
                .raw_value = d_text,
                .label = std::move(d_label),
                .depth = leaf_depth + 1,
                .parent_id = leaf_id,
                .role = data_role
            });
            vertical_edges.push_back({leaf_id, data_id});
        }

        // 4. Asignar coordenadas Y iniciales
        for (auto& n : nodes) {
            if (n.role == MerkleRole::Data || n.role == MerkleRole::CloneData) {
                auto* parent_leaf = find_mnode(nodes, n.parent_id);
                n.y = (parent_leaf ? parent_leaf->depth : max_depth) * m_cell_width + 2;
            } else {
                n.y = n.depth * m_cell_width;
            }
        }

        // 5. Calcular coordenadas X con expansión de contornos
        layout_subtree(nodes, 0);

        // 6. Normalizar coordenadas y bounding box
        normalize_and_bound(nodes, graph.width, graph.height, 2);

        // 7. Poblar graph.nodes y graph.edges
        for (auto& item : nodes) {
            if (item.left_id != -1) {
                graph.edges.push_back(VisualEdge{item.id, item.left_id});
            }
            if (item.right_id != -1) {
                graph.edges.push_back(VisualEdge{item.id, item.right_id});
            }
            graph.nodes.push_back(VisualNode{
                .id = item.id,
                .label = std::move(item.label),
                .x = item.x,
                .y = item.y,
                .css_class = std::string(to_css_class(item.role))
            });
        }

        for (const auto& ve : vertical_edges) {
            graph.edges.push_back(VisualEdge{ve.first, ve.second});
        }

        return graph;
    }
};

// Deduction guides para MerkleTreeLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
MerkleTreeLayout(F) -> MerkleTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
MerkleTreeLayout(F, size_t, TX, bool) -> MerkleTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_MERKLE_TREE_LAYOUT_H__
