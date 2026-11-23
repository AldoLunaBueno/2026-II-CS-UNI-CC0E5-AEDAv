/**
 * @file container_core.h
 * @ingroup core
 * @brief Interfaces base `IterableCore` y `ContainerCore` con soporte de sincronización recursiva.
 */

#ifndef __CONTAINER_CORE__
#define __CONTAINER_CORE__
#include <mutex>
#include "../../types.h"

/**
 * @class IterableCore
 * @brief Interfaz mínima para cualquier entidad iterable y sincronizada mediante mutex.
 *
 * @tparam Traits Estructura de rasgos del contenedor o vista.
 */
template <typename Traits>
class IterableCore {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node*;
    using Delim             = typename Node::Delim;
    using ForwardIterator   = typename Traits::ForwardIterator;

    virtual ~IterableCore() = default;

    /** @brief Retorna el mutex recursivo que protege el acceso al iterable. */
    virtual std::recursive_mutex& mutex() const = 0;
    /** @brief Retorna un iterador al inicio del rango. */
    virtual ForwardIterator begin() const = 0;
    /** @brief Retorna un iterador al final del rango. */
    virtual ForwardIterator end()   const = 0;

    /**
     * @brief Indica si el iterable no contiene elementos.
     * @return `true` si `begin() == end()`, `false` en caso contrario.
     */
    virtual bool empty() const {
        std::lock_guard lock(this->mutex());
        return this->begin() == this->end();
    }
};

/**
 * @class ContainerCore
 * @brief Base abstracta para contenedores mutables con operaciones de copia, movimiento y reconstrucción.
 *
 * @tparam Traits Estructura de rasgos del contenedor.
 */
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
    mutable std::recursive_mutex m_mutex; /**< Mutex recursivo para sincronización y composición segura. */

    /**
     * @brief Copia los elementos desde otro contenedor bloqueando ambos mutex sin deadlock.
     * @param other Contenedor origen a copiar.
     */
    void copy_from(const ContainerCore& other) {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        for (const auto& node : other)
            this->push_back(node.getValue(), node.getRef());
    }

    /**
     * @brief Transfiere recursos desde otro contenedor ejecutando `steal_fn` bajo bloqueo mutuo.
     * @tparam StealFunc Tipo del callable encargado de robar los punteros/recursos.
     * @param other Contenedor origen.
     * @param steal_fn Función sin argumentos que realiza la transferencia de propiedad.
     */
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

    /** @brief Constructor de copia explícito que no copia el mutex. */
    ContainerCore(const ContainerCore&) {}
    /** @brief Operador de asignación por copia que preserva el mutex local. */
    ContainerCore& operator=(const ContainerCore&) { return *this; }

    std::recursive_mutex& mutex() const override { return m_mutex; }

    /** @brief Elimina todos los elementos del contenedor. */
    virtual void clear() = 0;
    /**
     * @brief Inserta un nuevo elemento al contenedor.
     * @param value Valor a insertar.
     * @param ref Referencia opcional asociada al nodo.
     */
    virtual void push_back(const value_type& value, Ref ref = 0) = 0;
    /** @brief Restaura invariantes estructurales tras modificaciones externas (por defecto no-op). */
    virtual void rebuild() {}
};

#endif