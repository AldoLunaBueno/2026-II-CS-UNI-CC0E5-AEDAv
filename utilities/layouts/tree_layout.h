#ifndef __LAYOUTS_TREE_LAYOUT_H__
#define __LAYOUTS_TREE_LAYOUT_H__

#include <string_view>
#include "base_layout.h"

// Roles semánticos para nodos de árbol binario
enum class TreeRole { Root, Leaf, Left, Right };

constexpr std::string_view to_css_class(TreeRole r) {
    switch (r) {
        case TreeRole::Root:  return "root";
        case TreeRole::Leaf:  return "leaf";
        case TreeRole::Left:  return "left";
        case TreeRole::Right: return "right";
    }
    return "";
}

// Clase base abstracta para layouts de árboles (Strategy Pattern + Template Method)
template <typename Formatter = DefaultNodeFormatter>
class TreeLayout : public BaseLayout<Formatter> {
public:
    using Role = TreeRole;
    using Base = BaseLayout<Formatter>;
    using Base::m_directed;
    using Base::format_node;
    using Base::normalize_and_bound;

    virtual ~TreeLayout() = default;

    TX getCellWidth() const { return m_cell_width; }
    void setCellWidth(TX w) { m_cell_width = w; }

    // Template Method: extrae topología, delega cálculo de coordenadas y normaliza el grafo
    template <typename Tree>
    VisualGraph operator()(const Tree& tree) const {
        VisualGraph graph;
        graph.directed = m_directed;

        auto root = tree.getRoot();
        if (!root) return graph;

        std::vector<LayoutNode> nodes;
        TX max_depth = 0;
        TX next_id = 0;
        extract_nodes(root, 0, max_depth, nodes, next_id);

        compute_coordinates(nodes, max_depth);
        normalize_and_bound(nodes, graph.width, graph.height, 2);

        for (auto& item : nodes) {
            if (item.left_id != -1) {
                graph.edges.push_back(VisualEdge{item.id, item.left_id});
            }
            if (item.right_id != -1) {
                graph.edges.push_back(VisualEdge{item.id, item.right_id});
            }
            graph.nodes.push_back(VisualNode{item.id, std::move(item.label), item.x, item.y, std::string(to_css_class(item.role))});
        }

        return graph;
    }

protected:
    TX m_cell_width = 3;

    explicit TreeLayout(bool directed = false, TX cell_w = 3)
        : Base(directed), m_cell_width(cell_w) {}

    explicit TreeLayout(Formatter f, bool directed = false, TX cell_w = 3)
        : Base(std::move(f), directed), m_cell_width(cell_w) {}

    // Estructura agregada de nodo intermedio de layout
    struct LayoutNode {
        TX id = -1;
        std::string label;
        TX x = 0;
        TX y = 0;
        TX depth = 0;
        TX left_id = -1;
        TX right_id = -1;
        TX parent_id = -1;
        TreeRole role = TreeRole::Leaf;
    };

    // Búsqueda O(1) basada en ID indexado con fallback seguro
    static LayoutNode* find_node(std::vector<LayoutNode>& nodes, TX id) {
        if (id >= 0 && static_cast<size_t>(id) < nodes.size() && nodes[id].id == id) {
            return &nodes[id];
        }
        for (auto& item : nodes) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    static const LayoutNode* find_node(const std::vector<LayoutNode>& nodes, TX id) {
        if (id >= 0 && static_cast<size_t>(id) < nodes.size() && nodes[id].id == id) {
            return &nodes[id];
        }
        for (const auto& item : nodes) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    // Helper recursivo para extraer la estructura del árbol y asignar roles semánticos
    template <typename NodePtr>
    void extract_nodes(NodePtr node, TX d, TX& max_d, std::vector<LayoutNode>& nodes, TX& next_id, TX parent_id = -1, int child_branch = 0) const {
        if (!node) return;
        TX my_id = next_id++;
        if (d > max_d) max_d = d;

        bool is_root = (parent_id == -1);
        bool has_left = (node->m_pChild[0] != nullptr);
        bool has_right = (node->m_pChild[1] != nullptr);
        bool is_leaf = (!has_left && !has_right);

        TreeRole role = is_root ? TreeRole::Root : (is_leaf ? TreeRole::Leaf : (child_branch < 0 ? TreeRole::Left : TreeRole::Right));

        std::string label = this->format_node(*node, role);
        nodes.push_back({
            .id = my_id,
            .label = std::move(label),
            .depth = d,
            .parent_id = parent_id,
            .role = role
        });

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

    virtual void compute_coordinates(std::vector<LayoutNode>& nodes, TX max_depth) const = 0;
};

#endif // __LAYOUTS_TREE_LAYOUT_H__
