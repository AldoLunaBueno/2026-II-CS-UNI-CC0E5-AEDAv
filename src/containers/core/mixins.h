/**
 * @file mixins.h
 * @ingroup core
 * @brief Mixins de consulta, mutación, serialización y visualización para `GeneralContainer`.
 */

#ifndef __MIXINS__
#define __MIXINS__
#include "../../foreach.h"
#include "container_core.h"
#include <ostream>
#include <istream>

// Mixins de consulta

/**
 * @class Callable
 * @brief Mixin de consulta que permite aplicar funciones o predicados sobre el iterable.
 */
template <typename Traits>
class Callable : virtual public IterableCore<Traits> {
public:
    /**
     * @brief Ejecuta `func` sobre los elementos del iterable de forma thread-safe.
     * @tparam Func Tipo de función o predicado.
     * @tparam Args Tipos de argumentos adicionales.
     * @return El resultado de `::call()`.
     */
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) const {
        std::lock_guard lock(this->mutex());
        return ::call(this->begin(), this->end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/**
 * @class Searchable
 * @brief Mixin de consulta que agrega búsqueda del primer nodo que cumple un predicado.
 */
template <typename Traits>
class Searchable : virtual public Callable<Traits> {
public:
    using Parent    = Callable<Traits>;
    using Node      = typename Parent::Node;

    /**
     * @brief Busca y retorna la referencia al primer nodo que satisface `func`.
     * @throws std::runtime_error Si ningún elemento cumple el predicado.
     */
    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args&&... args) const {
        return this->call(std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/**
 * @class Printable
 * @brief Mixin de consulta que habilita la impresión `[n1,n2,...]` mediante `operator<<`.
 */
template <typename Traits>
class Printable : virtual public IterableCore<Traits> {
    /** @brief Serializa todos los elementos del iterable en un flujo de salida. */
    friend std::ostream& operator<<(std::ostream& os, const Printable& iterable) {
        std::lock_guard lock(iterable.mutex());
        std::string sep = "";
        os << "[";
        for (const auto& e : iterable) { os << sep << e; sep = ","; };
        return os << "]";
    }
};

// Mixins de contenedor

/**
 * @class Applicable
 * @brief Mixin de mutación que aplica una función a cada elemento y reconstruye invariantes con `rebuild()`.
 */
template <typename Traits>
class Applicable : virtual public ContainerCore<Traits>,
                   virtual public Callable<Traits> {
public:
    /**
     * @brief Aplica `func` a todos los elementos bajo bloqueo y llama a `rebuild()`.
     */
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::lock_guard lock(this->mutex());
        this->call(std::forward<Func>(func), std::forward<Args>(args)...);
        this->rebuild();
    }
};

/**
 * @class Reversible
 * @brief Mixin de contenedor que provee iteración inversa (`rbegin()`, `rend()`, `reversed()`, `rcall()`).
 */
template <typename Traits>
class Reversible : virtual public ContainerCore<Traits> {
public:
    using BackwardIterator = typename Traits::BackwardIterator;
    using Node             = typename Traits::Node;

    /** @brief Retorna un iterador al inicio del recorrido inverso. */
    virtual BackwardIterator rbegin() const = 0;
    /** @brief Retorna un iterador al final del recorrido inverso. */
    virtual BackwardIterator rend()   const = 0;

    /**
     * @brief Retorna una vista de solo lectura en sentido inverso.
     * @return Instancia de `BaseView` sobre `[rbegin(), rend())`.
     */
    auto reversed() const {
        std::lock_guard lock(this->mutex());
        using ReverseView = typename Traits::template View<Node, BackwardIterator>;
        return ReverseView(this->mutex(), rbegin(), rend());
    }

    /**
     * @brief Igual que `call()`, pero recorriendo el contenedor en sentido inverso (`rbegin()`/`rend()`).
     */
    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) const {
        return reversed().call(std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/**
 * @class Readable
 * @brief Mixin de contenedor que habilita la deserialización desde un flujo de entrada con formato `[n1,n2,...]`.
 */
template <typename Traits>
class Readable : virtual public ContainerCore<Traits> {
    /** @brief Lee nodos desde `is` y los inserta en el contenedor limpiando su contenido previo. */
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

/**
 * @class Drawable
 * @brief Mixin de visualización que proyecta el contenedor a un grafo usando un `Layout` y lo dibuja con un `Renderer`.
 */
template <typename Traits>
class Drawable : virtual public ContainerCore<Traits> {
public:
    /**
     * @brief Construye la representación visual del contenedor y la renderiza.
     * @param layout Objeto/functor que transforma el contenedor en `VisualGraph`.
     * @param renderer Objeto encargado de renderizar el `VisualGraph` resultante.
     */
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