#ifndef __LAYOUTS_H__
#define __LAYOUTS_H__

#include <sstream>
#include <string>
#include <algorithm>
#include <vector>
#include <cmath>
#include <type_traits>
#include "../types.h"
#include "render.h"
#include "ansistyle.h"

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

    // Invoca el formateador pasando el nodo por referencia constante (inmutable / copia por referencia)
    // o el valor si la lambda está especializada en él. Retorna un std::string listo para el renderizador.
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

// Clase base abstracta para layouts de árboles (Strategy Pattern + Template Method)
template <typename Formatter = DefaultNodeFormatter>
class TreeLayout : public BaseLayout<Formatter> {
public:
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
            graph.nodes.push_back(VisualNode{item.id, std::move(item.label), item.x, item.y});
        }

        return graph;
    }

protected:
    TX m_cell_width = 3;

    explicit TreeLayout(bool directed = false, TX cell_w = 3)
        : Base(directed), m_cell_width(cell_w) {}

    explicit TreeLayout(Formatter f, bool directed = false, TX cell_w = 3)
        : Base(std::move(f), directed), m_cell_width(cell_w) {}

    // Estructura agnóstica intermedia de nodo, extendiendo VisualNode
    struct LayoutNode : VisualNode {
        TX depth = 0;
        TX left_id = -1;
        TX right_id = -1;
        TX parent_id = -1;

        LayoutNode(TX id_, std::string label_, TX depth_, TX parent_id_ = -1)
            : VisualNode{id_, std::move(label_), 0, 0}, depth(depth_), parent_id(parent_id_) {}
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

    // Helper recursivo para extraer la estructura del árbol
    template <typename NodePtr>
    void extract_nodes(NodePtr node, TX d, TX& max_d, std::vector<LayoutNode>& nodes, TX& next_id, TX parent_id = -1) const {
        if (!node) return;
        TX my_id = next_id++;
        std::string label = this->format_node(*node);
        nodes.emplace_back(my_id, std::move(label), d, parent_id);
        if (d > max_d) max_d = d;

        TX current_idx = static_cast<TX>(nodes.size()) - 1;
        if (node->m_pChild[0]) {
            nodes[current_idx].left_id = next_id;
            extract_nodes(node->m_pChild[0], d + 1, max_d, nodes, next_id, my_id);
        }
        if (node->m_pChild[1]) {
            nodes[current_idx].right_id = next_id;
            extract_nodes(node->m_pChild[1], d + 1, max_d, nodes, next_id, my_id);
        }
    }

    virtual void compute_coordinates(std::vector<LayoutNode>& nodes, TX max_depth) const = 0;
};

// Disposición vertical compacta de árbol binario con contornos dinámicos
template <typename Formatter = DefaultNodeFormatter>
class VerticalTreeLayout : public TreeLayout<Formatter> {
public:
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


// Disposición horizontal compacta con centrado dinámico en bifurcaciones
template <typename Formatter = DefaultNodeFormatter>
class HorizontalTreeLayout : public TreeLayout<Formatter> {
public:
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


// Layout para listas enlazadas circulares (CircularLinkedList / CircularDoubleLinkedList)
// Dispone los nodos formando un contorno elíptico cerrado suave con curvatura continua.
template <typename Formatter = DefaultNodeFormatter>
class CircularListLayout : public BaseLayout<Formatter> {
    TX m_span_x = 2; // Espaciado horizontal
    TX m_span_y = 2; // Desnivel vertical
public:
    using Base = BaseLayout<Formatter>;
    using Base::m_directed;
    using Base::format_node;
    using Base::normalize_and_bound;

    explicit CircularListLayout(bool is_directed = false, TX span_x = 2, TX span_y = 2)
        : Base(is_directed), m_span_x(span_x), m_span_y(span_y) {}

    explicit CircularListLayout(Formatter f, bool is_directed = false, TX span_x = 2, TX span_y = 2)
        : Base(std::move(f), is_directed), m_span_x(span_x), m_span_y(span_y) {}

    template <typename F>
    auto with_formatter(F&& f) const {
        return CircularListLayout<std::decay_t<F>>(std::forward<F>(f), m_directed, m_span_x, m_span_y);
    }

    template <typename Container>
    VisualGraph operator()(const Container& container) const {
        VisualGraph graph;
        graph.directed = m_directed;

        // 1. Extraer etiquetas usando la función format_node pasando el nodo por referencia de solo lectura
        std::vector<std::string> labels;
        for (const auto& item : container) {
            labels.push_back(this->format_node(item));
        }

        size_t N = labels.size();
        if (N == 0) return graph;

        // Casos degenerados
        if (N == 1) {
            TX w = static_cast<TX>(visible_length(labels[0]));
            graph.nodes.push_back(VisualNode{0, std::move(labels[0]), 0, 0});
            graph.width = w;
            graph.height = 1;
            return graph;
        }

        if (N == 2) {
            TX x1 = static_cast<TX>(visible_length(labels[0])) + 3;
            TX len1 = static_cast<TX>(visible_length(labels[1]));
            graph.nodes.push_back(VisualNode{0, std::move(labels[0]), 0, 0});
            graph.nodes.push_back(VisualNode{1, std::move(labels[1]), x1, 0});
            graph.edges.push_back(VisualEdge{0, 1});
            graph.edges.push_back(VisualEdge{1, 0});
            graph.width = x1 + len1;
            graph.height = 1;
            return graph;
        }

        // Distribución elíptica regular para N >= 3
        TX max_label_len = 1;
        for (const auto& l : labels) {
            max_label_len = std::max(max_label_len, static_cast<TX>(visible_length(l)));
        }

        double ang_step = 2.0 * M_PI / static_cast<double>(N);
        double sin_step = std::sin(ang_step);
        TX min_step_x = max_label_len + 2;

        TX Rx = static_cast<TX>(std::max<double>(static_cast<double>(N) * 0.8, std::ceil(min_step_x / std::max(sin_step, 0.05))));
        TX Ry = static_cast<TX>(std::max<double>(static_cast<double>(N) * 0.35, 2.0));

        std::vector<VisualNode> raw_nodes;
        raw_nodes.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            raw_nodes.push_back(VisualNode{static_cast<TX>(i), std::move(labels[i]), 0, 0});
        }

        size_t K = N / 2;
        for (size_t k = 0; k <= K; ++k) {
            double angle = static_cast<double>(k) * ang_step;
            TX offset_x = static_cast<TX>(std::round(Rx * std::sin(angle)));
            TX pos_y = static_cast<TX>(std::round(Ry * (1.0 - std::cos(angle))));

            raw_nodes[k].x = Rx + offset_x;
            raw_nodes[k].y = pos_y;

            if (k > 0 && k < N - k) {
                size_t left_idx = N - k;
                raw_nodes[left_idx].x = Rx - offset_x;
                raw_nodes[left_idx].y = pos_y;
            }
        }

        normalize_and_bound(raw_nodes, graph.width, graph.height, 0);
        for (auto& rn : raw_nodes) {
            graph.nodes.push_back(std::move(rn));
        }

        for (size_t i = 0; i < N; ++i) {
            graph.edges.push_back(VisualEdge{static_cast<TX>(i), static_cast<TX>((i + 1) % N)});
        }

        return graph;
    }
};

// Deduction guides para CircularListLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
CircularListLayout(F) -> CircularListLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
CircularListLayout(F, bool, TX, TX) -> CircularListLayout<std::decay_t<F>>;

#endif // __LAYOUTS_H__
