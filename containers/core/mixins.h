#ifndef __MIXINS__
#define __MIXINS__
#include "../../foreach.h"
#include "container_core.h"
#include <ostream>
#include <istream>

// Mixins de consulta

template <typename Traits>
class Callable : virtual public IterableCore<Traits> {
public:
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) const {
        std::lock_guard lock(this->mutex());
        return ::call(this->begin(), this->end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename Traits>
class Searchable : virtual public Callable<Traits> {
public:
    using Parent    = Callable<Traits>;
    using Node      = typename Parent::Node;

    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args&&... args) const {
        return this->call(std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename Traits>
class Printable : virtual public IterableCore<Traits> {
    friend std::ostream& operator<<(std::ostream& os, const Printable& iterable) {
        std::lock_guard lock(iterable.mutex());
        std::string sep = "";
        os << "[";
        for (const auto& e : iterable) { os << sep << e; sep = ","; };
        return os << "]";
    }
};

// Mixins de contenedor

template <typename Traits>
class Applicable : virtual public ContainerCore<Traits>,
                   virtual public Callable<Traits> {
public:
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::lock_guard lock(this->mutex());
        this->call(std::forward<Func>(func), std::forward<Args>(args)...);
        this->rebuild();
    }
};

template <typename Traits>
class Reversible : virtual public ContainerCore<Traits> {
public:
    using BackwardIterator = typename Traits::BackwardIterator;
    using Node             = typename Traits::Node;

    virtual BackwardIterator rbegin() const = 0;
    virtual BackwardIterator rend()   const = 0;

    auto reversed() const {
        std::lock_guard lock(this->mutex());
        using ReverseView = typename Traits::template View<Node, BackwardIterator>;
        return ReverseView(this->mutex(), rbegin(), rend());
    }

    // Igual que call(), pero recorriendo el container en sentido inverso
    // (rbegin()/rend()) en vez de hacia adelante.
    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) const {
        return reversed().call(std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename Traits>
class Readable : virtual public ContainerCore<Traits> {
    friend std::istream& operator>>(std::istream& is, Readable& container) {
        std::lock_guard lock(container.mutex());
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
};

// 2. Mixin dibujable para contenedores
template <typename Traits>
class Drawable : virtual public ContainerCore<Traits> {
public:
    template <typename Self, typename Layout, typename Renderer>
    decltype(auto) draw(this Self&& self, Layout&& layout, Renderer&& renderer) {
        auto graph = [&]() {
            std::lock_guard lock(self.mutex());
            return layout(self);
        }();
        return renderer.render(graph);
    }
};

#endif