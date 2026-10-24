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
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template <typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};


template <template <typename> typename T, typename Traits>
concept IsContainerCore = std::derived_from<T<Traits>, ContainerCore<Traits>>;

template <typename Traits, template <typename> typename... Mixins>
    requires (IsContainerCore<Mixins, Traits> && ...)
class GeneralContainer :
    virtual public ContainerCore<Traits>,
    virtual public Mixins<Traits>... {
public:
    using Core              = ContainerCore<Traits>;
    using value_type        = typename Core::value_type;
    using Node              = typename Core::Node;
    using Delim             = typename Core::Delim;
    using ForwardIterator   = typename Core::ForwardIterator;
public:
    GeneralContainer() : Mixins<Traits>()... {}
    using Mixins<Traits>::Mixins...;

    virtual bool empty() const { return this->begin() == this->end(); }
    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref) = 0;

    // Persistencia
    friend std::ostream& operator<<(std::ostream& os, GeneralContainer& container) {
        auto first = true;
        os << "[";
        container.ApplyFunction([&os, &first](const Node& node){
            if (!first) os << ",";
            os << node;
            first = false;
        });
        return os << "]";
    }

    friend std::istream &operator >>(std::istream& is, GeneralContainer& container) {
        Delim d;
        Node node;
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

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }
};

#endif // __GENERAL_CONTAINER_H__