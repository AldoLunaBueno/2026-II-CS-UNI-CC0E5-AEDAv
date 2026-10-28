#ifndef __CONTAINER_CORE__
#define __CONTAINER_CORE__
#include <mutex>
#include <utility>
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
    mutable std::recursive_mutex m_mutex; // mutex recursivo para sincronización y composición segura

    void copy_from(const ContainerCore& other) {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        for (const auto& node : other)
            this->push_back(node.getValue(), node.getRef());
    }

    template <typename Derived, typename StealFunc>
    void move_from(Derived& other, StealFunc steal_fn) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        steal_fn();
    }
public:
    ContainerCore() = default;
    virtual ~ContainerCore() = default;

    // para que ContainerCore sea asignable se debe hacer explícito (no copia/mueve el mutex):
    ContainerCore(const ContainerCore&) {}
    ContainerCore& operator=(const ContainerCore&) { return *this; }

    virtual bool empty() const {
        std::lock_guard lock(this->m_mutex);
        return this->begin() == this->end();
    }
    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref) = 0;

    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end() const = 0;
};

#endif