#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include "ContainerCore.h"
#include "../types.h"

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

    virtual bool empty() const { return this->begin() == this->end(); }
    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref) = 0;
};

// Persistencia
template <typename Traits, template <typename> typename... Mixins>
std::ostream& operator<<(std::ostream& os, GeneralContainer<Traits, Mixins...>& container) {
    std::string sep = "";
    os << "[";
    for (const auto& e : container) { os << sep << e; sep = ","; };
    return os << "]";
}

template <typename Traits, template <typename> typename... Mixins>
std::istream& operator>>(std::istream& is, GeneralContainer<Traits, Mixins...>& container) {
    typename Traits::Node::Delim d;
    typename Traits::Node node;
    container.clear();

    is >> d;
    if (is >> d && d != ']'){
        is.unget();
        while(is >> node >> d){
            container.push_back(node.getValue(), node.getRef());
            if (d == ']') break;
        }    
    }

    return is; 
}

#endif // __GENERAL_CONTAINER_H__