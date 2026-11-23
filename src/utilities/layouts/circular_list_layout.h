/**
 * @file circular_list_layout.h
 * @ingroup layouts
 * @brief Layout elíptico CRTP para listas circulares (`CircularListLayout`) y roles `CircularListRole`.
 */

#ifndef __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__
#define __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__

#include <cmath>
#include <vector>
#include <algorithm>
#include <type_traits>
#include "base_layout.h"

/** @brief Roles semánticos para nodos de listas circulares (`Head`, `Tail`, `Node`). */
enum class CircularListRole { Head, Tail, Node };

/**
 * @class CircularListLayout
 * @brief Dispone los nodos de una lista circular formando un contorno elíptico cerrado.
 * @tparam Formatter Functor de formateo de nodos.
 */
template <typename Formatter = DefaultNodeFormatter>
class CircularListLayout : public BaseLayout<CircularListLayout<Formatter>, Formatter> {
    friend class BaseLayout<CircularListLayout<Formatter>, Formatter>;
    TX m_span_x = 2; // Espaciado horizontal
    TX m_span_y = 2; // Desnivel vertical
public:
    using Role = CircularListRole;
    using Base = BaseLayout<CircularListLayout<Formatter>, Formatter>;

    explicit CircularListLayout(bool is_directed = false, TX span_x = 2, TX span_y = 2)
        : Base(is_directed), m_span_x(span_x), m_span_y(span_y) {}
    explicit CircularListLayout(Formatter f, bool is_directed = false, TX span_x = 2, TX span_y = 2)
        : Base(std::move(f), is_directed), m_span_x(span_x), m_span_y(span_y) {}

protected:
    /** @brief Implementación de la Etapa 1: extrae los nodos y aristas circulares sin formatear aún las etiquetas. */
    template <typename Container>
    auto extract_topology_impl(const Container& container) const;

    /** @brief Implementación de la Etapa 3: calcula las coordenadas 2D de la elipse usando las etiquetas ya formateadas. */
    template <typename Draft>
    void compute_coordinates_impl(Draft& draft) const;
};

// ============================================================================
// Implementación fuera de clase: CircularListLayout
// ============================================================================

template <typename Formatter>
template <typename Container>
auto CircularListLayout<Formatter>::extract_topology_impl(const Container& container) const {
    using NodePtr = const std::decay_t<decltype(*container.begin())>*;
    using NodeT = DraftNode<NodePtr, CircularListRole>;
    using Draft = LayoutDraft<NodeT>;

    Draft draft;
    size_t total = 0;
    if constexpr (requires { container.size(); }) {
        total = container.size();
    }

    TX idx = 0;
    for (const auto& item : container) {
        CircularListRole role = (idx == 0) ? CircularListRole::Head
                              : ((total > 0 && static_cast<size_t>(idx) == total - 1) ? CircularListRole::Tail : CircularListRole::Node);
        NodeT entry;
        entry.id = idx++;
        entry.payload = &item;
        entry.role = role;
        draft.nodes.push_back(std::move(entry));
    }

    size_t N = draft.nodes.size();
    if (N >= 2) {
        for (size_t i = 0; i < N; ++i) {
            draft.edges.push_back(VisualEdge{static_cast<TX>(i), static_cast<TX>((i + 1) % N)});
        }
    }
    return draft;
}

template <typename Formatter>
template <typename Draft>
void CircularListLayout<Formatter>::compute_coordinates_impl(Draft& draft) const {
    auto& nodes = draft.nodes;
    size_t N = nodes.size();
    if (N <= 1) return;

    if (N == 2) {
        nodes[0].x = 0;
        nodes[0].y = 0;
        nodes[1].x = static_cast<TX>(visible_length(nodes[0].label)) + 3;
        nodes[1].y = 0;
        return;
    }

    TX max_label_len = 1;
    for (const auto& n : nodes) {
        max_label_len = std::max(max_label_len, static_cast<TX>(visible_length(n.label)));
    }

    double ang_step = 2.0 * M_PI / static_cast<double>(N);
    double sin_step = std::sin(ang_step);
    TX min_step_x = max_label_len + 2;

    TX Rx = static_cast<TX>(std::max<double>(static_cast<double>(N) * 0.8, std::ceil(min_step_x / std::max(sin_step, 0.05))));
    TX Ry = static_cast<TX>(std::max<double>(static_cast<double>(N) * 0.35, 2.0));

    size_t K = N / 2;
    for (size_t k = 0; k <= K; ++k) {
        double angle = static_cast<double>(k) * ang_step;
        TX offset_x = static_cast<TX>(std::round(Rx * std::sin(angle)));
        TX pos_y = static_cast<TX>(std::round(Ry * (1.0 - std::cos(angle))));

        nodes[k].x = Rx + offset_x;
        nodes[k].y = pos_y;

        if (k > 0 && k < N - k) {
            size_t left_idx = N - k;
            nodes[left_idx].x = Rx - offset_x;
            nodes[left_idx].y = pos_y;
        }
    }
}

// Deduction guides para CircularListLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
CircularListLayout(F) -> CircularListLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
CircularListLayout(F, bool, TX, TX) -> CircularListLayout<std::decay_t<F>>;

#endif // __LAYOUTS_CIRCULAR_LIST_LAYOUT_H__
