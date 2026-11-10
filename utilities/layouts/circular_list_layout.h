#ifndef __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__
#define __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__

#include <cmath>
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include "base_layout.h"

// Roles semánticos para nodos de listas circulares
enum class CircularListRole { Head, Tail, Node };

constexpr std::string_view to_css_class(CircularListRole r) {
    switch (r) {
        case CircularListRole::Head: return "head";
        case CircularListRole::Tail: return "tail";
        case CircularListRole::Node: return "node";
    }
    return "";
}

// Layout para listas enlazadas circulares (CircularLinkedList / CircularDoubleLinkedList)
// Dispone los nodos formando un contorno elíptico cerrado suave con curvatura continua.
template <typename Formatter = DefaultNodeFormatter>
class CircularListLayout : public BaseLayout<Formatter> {
    TX m_span_x = 2; // Espaciado horizontal
    TX m_span_y = 2; // Desnivel vertical
public:
    using Role = CircularListRole;
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

        // 1. Extraer etiquetas y roles semánticos
        std::vector<std::string> labels;
        std::vector<std::string> css_classes;
        size_t total = 0;
        if constexpr (requires { container.size(); }) {
            total = container.size();
        }
        size_t idx = 0;
        for (const auto& item : container) {
            CircularListRole role = (idx == 0) ? CircularListRole::Head
                                  : ((total > 0 && idx == total - 1) ? CircularListRole::Tail : CircularListRole::Node);

            labels.push_back(this->format_node(item, role));
            css_classes.emplace_back(to_css_class(role));
            idx++;
        }

        size_t N = labels.size();
        if (N == 0) return graph;

        // Casos degenerados
        if (N == 1) {
            TX w = static_cast<TX>(visible_length(labels[0]));
            graph.nodes.push_back(VisualNode{0, std::move(labels[0]), 0, 0, std::move(css_classes[0])});
            graph.width = w;
            graph.height = 1;
            return graph;
        }

        if (N == 2) {
            TX x1 = static_cast<TX>(visible_length(labels[0])) + 3;
            TX len1 = static_cast<TX>(visible_length(labels[1]));
            graph.nodes.push_back(VisualNode{0, std::move(labels[0]), 0, 0, std::move(css_classes[0])});
            graph.nodes.push_back(VisualNode{1, std::move(labels[1]), x1, 0, std::move(css_classes[1])});
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
            raw_nodes.push_back(VisualNode{static_cast<TX>(i), std::move(labels[i]), 0, 0, std::move(css_classes[i])});
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

#endif // __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__
