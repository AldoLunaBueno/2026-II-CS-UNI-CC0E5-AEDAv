#ifndef __CONTAINER_CORE__
#define __CONTAINER_CORE__
#include <mutex>
#include "../types.h"

template <typename Traits>
class ContainerCore {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using NodePtr           = Node*;
    using Delim             = Node::Delim;
    using ForwardIterator   = Traits::ForwardIterator;
protected:
    mutable std::mutex m_mutex; // mutex para sincronización
public:
    ContainerCore() = default;
    // para que ContainerCore sea asignable se debe hacer explícito:
    ContainerCore(const ContainerCore&) {} // no copia mutex
    ContainerCore& operator=(const ContainerCore&) { return *this; } // no asigna mutex

    virtual bool empty() const { return this->begin() == this->end(); }
    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref) = 0;

    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end() const = 0;
};

#endif