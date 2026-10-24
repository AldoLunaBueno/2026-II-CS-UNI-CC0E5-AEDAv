#ifndef __MIXINS__
#define __MIXINS__
#include "../foreach.h"
#include "ContainerCore.h"

template <typename Traits>
class Callable : virtual public ContainerCore<Traits>  {
public:
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) {
        lock_guard<mutex> lock(this->m_mutex);
        return ::call(this->begin(), this->end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename Traits>
class Applicable : virtual public Callable<Traits> {
public:
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) { this->call(func, std::forward<Args>(args)...); }
};

template <typename Traits>
class Searchable : virtual public Callable<Traits>  {
public:
    using Parent    = Callable<Traits>;
    using Node      = typename Parent::Node;

    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args&&... args) { return this->call(func, std::forward<Args>(args)...); }
};

template <typename Traits>
class ReverseCallable : virtual public ContainerCore<Traits>  {
public:
    using BackwardIterator = typename Traits::BackwardIterator;
public:
    // Igual que call(), pero recorriendo el container en sentido inverso
    // (rbegin()/rend()) en vez de hacia adelante.
    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) {
        lock_guard<mutex> lock(this->m_mutex);
        return ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
    }

    virtual BackwardIterator rbegin() const = 0;
    virtual BackwardIterator rend() const = 0;
};

#endif