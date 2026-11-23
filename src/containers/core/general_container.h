/**
 * @file general_container.h
 * @ingroup core
 * @brief Composición de contenedores y vistas mediante mixins y metaprogramación con conceptos C++20.
 */

#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include "container_core.h"
#include <concepts>
#include <functional>
#include <type_traits>

/**
 * @struct DefaultTraits
 * @brief Rasgos por defecto de ordenamiento y tipo de elemento para un contenedor.
 * @tparam T Tipo de los elementos almacenados.
 * @tparam _Compare Comparador binario (por ejemplo `std::less<T>`).
 */
template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type    = T;
    using Compare       = _Compare;
};

/** @brief Rasgos con orden ascendente (`std::less<T>`). */
template <typename T, typename _Compare = std::less<T>>
using AscendingTraits = DefaultTraits<T, _Compare>;

/** @brief Rasgos con orden descendente (`std::greater<T>`). */
template <typename T, typename _Compare = std::greater<T>>
using DescendingTraits = DefaultTraits<T, _Compare>;

/**
 * @brief Concepto `IsIterableMixin`: verifica que el mixin `M<Traits>` herede de `IterableCore<Traits>`.
 */
template <template <typename> typename M, typename Traits>
concept IsIterableMixin = std::derived_from<M<Traits>, IterableCore<Traits>>;

/**
 * @brief Concepto `IsViewMixin`: verifica que un mixin sea compatible con vistas de solo lectura (no requiere `ContainerCore`).
 */
template <template <typename> typename M, typename Traits>
concept IsViewMixin =
    IsIterableMixin<M, Traits> &&
    !std::derived_from<M<Traits>, ContainerCore<Traits>>;

/** @brief Marcador vacío utilizado cuando un mixin de mutación se filtra fuera de una vista. */
template <template <typename> typename M, typename Traits>
struct IgnoredMixin {};

/**
 * @typedef FilterViewMixin
 * @brief Selecciona `M<Traits>` si es un mixin de vista o `IgnoredMixin` en caso contrario.
 *
 * @image html mixin_filter.svg "Filtrado estático de mixins para BaseView"
 */
template <template <typename> typename M, typename Traits>
using FilterViewMixin = std::conditional_t<
    IsViewMixin<M, Traits>,
    M<Traits>,
    IgnoredMixin<M, Traits>
>;

/**
 * @struct ViewTraits
 * @brief Rasgos mínimos para instanciar una vista `BaseView` sobre un rango de iteradores.
 */
template <typename NodeT, typename Iter>
struct ViewTraits {
    using value_type       = typename NodeT::value_type;
    using Node             = NodeT;
    using ForwardIterator  = Iter;
    using BackwardIterator = Iter;
};

/**
 * @class BaseView
 * @brief Vista ligera y thread-safe sobre un rango `[begin, end)` que hereda únicamente los mixins de consulta.
 *
 * @tparam Traits Rasgos de la vista (`ViewTraits`).
 * @tparam Mixins Lista de mixins del contenedor original (filtrados por `FilterViewMixin`).
 */
template <typename Traits, template <typename> typename... Mixins>
    requires (IsIterableMixin<Mixins, Traits> && ...)
class BaseView :
    virtual public IterableCore<Traits>,
    virtual public FilterViewMixin<Mixins, Traits>... {
public:
    using Core = IterableCore<Traits>;
    using typename Core::value_type;
    using typename Core::Node;
    using typename Core::ForwardIterator;
private:
    std::recursive_mutex& m_rMutex;
    ForwardIterator       m_begin;
    ForwardIterator       m_end;
public:
    /**
     * @brief Construye una vista sobre el rango `[b, e)` compartiendo el mutex del contenedor.
     * @param mtx Referencia al mutex recursivo del contenedor propietario.
     * @param b Iterador de inicio.
     * @param e Iterador de fin.
     */
    BaseView(std::recursive_mutex& mtx, ForwardIterator b, ForwardIterator e)
        : m_rMutex(mtx), m_begin(b), m_end(e) {}

    std::recursive_mutex& mutex() const override { return m_rMutex; }
    ForwardIterator begin() const override { return m_begin; }
    ForwardIterator end()   const override { return m_end;   }
};

/**
 * @struct ContainerTraits
 * @brief Extiende `BaseTraits` añadiendo la definición de `View` parametrizada por los mixins del contenedor.
 */
template <typename BaseTraits, template <typename> typename... Mixins>
struct ContainerTraits : public BaseTraits {
    template <typename NodeT, typename Iter>
    using View = BaseView<ViewTraits<NodeT, Iter>, Mixins...>;
};

/**
 * @class GeneralContainer
 * @brief Contenedor genérico compuesto por `ContainerCore` y un conjunto variádico de mixins.
 *
 * @tparam BaseTraits Rasgos base (por ejemplo `BinaryTreeTraits`, `LinkedListTraits`, etc.).
 * @tparam Mixins Mixins de funcionalidad (`Callable`, `Printable`, `Drawable`, etc.).
 */
template <typename BaseTraits, template <typename> typename... Mixins>
    requires (IsIterableMixin<Mixins, ContainerTraits<BaseTraits, Mixins...>> && ...)
class GeneralContainer :
    virtual public ContainerCore<ContainerTraits<BaseTraits, Mixins...>>,
    virtual public Mixins<ContainerTraits<BaseTraits, Mixins...>>... {
public:
    using Traits = ContainerTraits<BaseTraits, Mixins...>;
    using Core   = ContainerCore<Traits>;
    using typename Core::value_type;
    using typename Core::Node;
    using typename Core::NodePtr;
    using typename Core::Delim;
    using typename Core::ForwardIterator;
public:
    GeneralContainer() : Mixins<Traits>()... {}
    using Mixins<Traits>::Mixins...;
};

#endif // __GENERAL_CONTAINER_H__