#ifndef __CONTAINER_CORE__
#define __CONTAINER_CORE__
#include <mutex>
#include "../../types.h"

template <typename Traits>
class IterableCore {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node*;
    using Delim             = typename Node::Delim;
    using ForwardIterator   = typename Traits::ForwardIterator;

    virtual ~IterableCore() = default;

    virtual std::recursive_mutex& mutex() const = 0;
    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end()   const = 0;

    virtual bool empty() const {
        std::lock_guard lock(this->mutex());
        return this->begin() == this->end();
    }
};

template <typename Traits>
class ContainerCore : virtual public IterableCore<Traits> {
public:
    using Parent = IterableCore<Traits>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::Delim;
    using typename Parent::ForwardIterator;
protected:
    mutable std::recursive_mutex m_mutex; // mutex recursivo para sincronización y composición segura

    void copy_from(const ContainerCore& other) {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        for (const auto& node : other)
            this->push_back(node.getValue(), node.getRef());
    }

    template <typename StealFunc>
    void move_from(ContainerCore& other, StealFunc steal_fn) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        steal_fn();
    }
public:
    ContainerCore() = default;
    virtual ~ContainerCore() = default;

    // asignación explícita sin copiar mutex
    ContainerCore(const ContainerCore&) {}
    ContainerCore& operator=(const ContainerCore&) { return *this; }

    std::recursive_mutex& mutex() const override { return m_mutex; }

    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref = 0) = 0;
    virtual void rebuild() {}
};

#endif