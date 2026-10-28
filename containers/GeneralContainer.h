#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include "ContainerCore.h"

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type    = T;
    using Compare       = _Compare;
};

template <typename T, typename _Compare = std::less<T>>
using AscendingTraits = DefaultTraits<T, _Compare>;

template <typename T, typename _Compare = std::greater<T>>
using DescendingTraits = DefaultTraits<T, _Compare>;

template <template <typename> typename T, typename Traits>
concept IsContainerCore = std::derived_from<T<Traits>, ContainerCore<Traits>>;

template <typename Traits, template <typename> typename... Mixins>
    requires (IsContainerCore<Mixins, Traits> && ...)
class GeneralContainer :
    virtual public ContainerCore<Traits>,
    virtual public Mixins<Traits>... {
public:
    using Core = ContainerCore<Traits>;
    using typename Core::value_type;
    using typename Core::Node;
    using typename Core::Delim;
    using typename Core::ForwardIterator;
public:
    GeneralContainer() : Mixins<Traits>()... {}
    using Mixins<Traits>::Mixins...;
};

#endif // __GENERAL_CONTAINER_H__