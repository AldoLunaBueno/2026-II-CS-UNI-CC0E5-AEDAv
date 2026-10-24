#ifndef __CONTAINER_CORE__
#define __CONTAINER_CORE__
#include <mutex>

template <typename Traits>
class ContainerCore {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using Delim             = Node::Delim;
    using ForwardIterator   = Traits::ForwardIterator;
protected:
    mutable std::mutex m_mutex; // mutex para sincronización
public:
    ContainerCore() = default;
    // para que ContainerCore sea asignable se debe hacer explícito:
    ContainerCore(const ContainerCore&) {} // no copia mutex
    ContainerCore& operator=(const ContainerCore&) { return *this; } // no asigna mutex

    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end() const = 0;
};

#endif