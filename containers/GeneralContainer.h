#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include "ContainerCore.h"
#include <concepts>
#include <functional>
#include <type_traits>

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type    = T;
    using Compare       = _Compare;
};

template <typename T, typename _Compare = std::less<T>>
using AscendingTraits = DefaultTraits<T, _Compare>;

template <typename T, typename _Compare = std::greater<T>>
using DescendingTraits = DefaultTraits<T, _Compare>;

template <template <typename> typename M, typename Traits>
concept IsIterableMixin = std::derived_from<M<Traits>, IterableCore<Traits>>;

template <template <typename> typename M, typename Traits>
concept IsViewMixin =
    IsIterableMixin<M, Traits> &&
    !std::derived_from<M<Traits>, ContainerCore<Traits>>;

template <template <typename> typename M, typename Traits>
struct IgnoredMixin {};

template <template <typename> typename M, typename Traits>
using FilterViewMixin = std::conditional_t<
    IsViewMixin<M, Traits>,
    M<Traits>,
    IgnoredMixin<M, Traits>
>;

template <typename NodeT, typename Iter>
struct ViewTraits {
    using value_type       = typename NodeT::value_type;
    using Node             = NodeT;
    using ForwardIterator  = Iter;
    using BackwardIterator = Iter;
};

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
    BaseView(std::recursive_mutex& mtx, ForwardIterator b, ForwardIterator e)
        : m_rMutex(mtx), m_begin(b), m_end(e) {}

    std::recursive_mutex& mutex() const override { return m_rMutex; }
    ForwardIterator begin() const override { return m_begin; }
    ForwardIterator end()   const override { return m_end;   }
};

template <typename BaseTraits, template <typename> typename... Mixins>
struct ContainerTraits : public BaseTraits {
    template <typename NodeT, typename Iter>
    using View = BaseView<ViewTraits<NodeT, Iter>, Mixins...>;
};

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