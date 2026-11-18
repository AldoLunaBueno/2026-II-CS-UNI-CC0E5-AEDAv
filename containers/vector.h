/**
 * @file vector.h
 * @ingroup containers
 * @brief Implementación de arreglo dinámico genérico (`BaseVector` / `Vector`) con iteradores y mixins.
 */

#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <initializer_list>
#include "core/general_container.h"
#include "core/general_iterator.h"
#include "core/general_node.h"
#include "core/mixins.h"

using namespace std;

/**
 * @class VectorForwardIterator
 * @brief Iterador hacia adelante para `BaseVector`.
 * @tparam T Tipo de nodo almacenado en el vector (`GeneralNode<...>`).
 */
template <typename T>
class VectorForwardIterator : public GeneralIterator<VectorForwardIterator<T>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    /** @brief Avanza el iterador al siguiente elemento del arreglo. */
    VectorForwardIterator& operator++() { ++Parent::m_ptr; return *this; }
};

/**
 * @class VectorBackwardIterator
 * @brief Iterador inverso para `BaseVector`.
 * @tparam T Tipo de nodo almacenado en el vector.
 */
template <typename T>
class VectorBackwardIterator : public GeneralIterator<VectorBackwardIterator<T>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorBackwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    /** @brief Retrocede el iterador hacia el elemento anterior del arreglo. */
    VectorBackwardIterator& operator++() { --Parent::m_ptr; return *this; }
};

/**
 * @struct VectorTraits
 * @brief Rasgos base de nodo e iteradores para `BaseVector`.
 */
template <typename T>
struct VectorTraits {
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;  /**< Itera sobre `Node`, no sobre `T`. */
    using BackwardIterator  = VectorBackwardIterator<Node>; /**< Itera en sentido inverso sobre `Node`. */
};

/** @brief Rasgos de vector con orden ascendente. */
template <typename T>
struct VectorAscTraits : public AscendingTraits<T>, public VectorTraits<T> {};

/** @brief Rasgos de vector con orden descendente. */
template <typename T>
struct VectorDescTraits : public DescendingTraits<T>, public VectorTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseVector;

/** @brief Alias estándar de vector equipado con mixins de consulta, mutación, reversa y serialización. */
template <typename Traits>
using Vector = BaseVector<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable>;

/**
 * @class BaseVector
 * @brief Arreglo dinámico thread-safe basado en `GeneralContainer`.
 *
 * @tparam Traits Estructura de rasgos del vector.
 * @tparam Mixins Lista de mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseVector : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseVector;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using typename Parent::NodePtr;
protected:
    using Parent::m_mutex;
private:
    NodePtr      m_data     = nullptr;   /**< Puntero al arreglo dinámico de nodos. */
    size_t       m_size     = 0,         /**< Cantidad actual de elementos. */
                 m_capacity = 0;         /**< Capacidad reservada en memoria. */

    /** @brief Redimensiona la capacidad interna del arreglo a `new_cap`. */
    void resize(size_t new_cap);

public:
    BaseVector() {}

    /**
     * @brief Construye el vector a partir de una lista de pares `(valor, ref)`.
     * @param values Lista inicializadora de pares `(valor, ref)` para cada `Node`.
     */
    BaseVector(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    virtual ~BaseVector() { clear(); }

    BaseVector(const BaseVector& other) { *this = other; }
    BaseVector(BaseVector&& other) noexcept { *this = std::move(other); }

    BaseVector& operator=(const BaseVector& other);
    BaseVector& operator=(BaseVector&& other) noexcept;

    /** @brief Inserta un nodo al final del vector duplicando la capacidad si es necesario. */
    void push_back(const value_type& value, Ref ref = 0) override;
    /** @brief Elimina el último elemento del vector si no está vacío. */
    void pop_back();

    /** @brief Acceso indexado con verificación de límites (`std::out_of_range`). */
    Node& operator[](size_t index);
    /** @brief Acceso indexado de solo lectura con verificación de límites (`std::out_of_range`). */
    const Node& operator[](size_t index) const;
    /** @brief Acceso indexado con verificación de límites (`std::out_of_range`). */
    Node& at(size_t index);
    /** @brief Acceso indexado de solo lectura con verificación de límites (`std::out_of_range`). */
    const Node& at(size_t index) const;
    /** @brief Retorna el último nodo del vector. */
    Node& back();
    /** @brief Retorna el último nodo del vector (solo lectura). */
    const Node& back() const;

    /** @brief Reserva capacidad para al menos `new_cap` elementos. */
    void reserve(size_t new_cap) {
        std::lock_guard lock(m_mutex);
        resize(new_cap);
    }

    /** @brief Retorna la cantidad de elementos almacenados. */
    size_t size()     const { return m_size; }
    /** @brief Retorna la capacidad actualmente reservada. */
    size_t capacity() const { return m_capacity; }

    /** @brief Libera la memoria reservada y reinicia tamaño y capacidad a 0. */
    void clear() override;

    ForwardIterator   begin() const override { return ForwardIterator(m_data); }
    ForwardIterator   end()   const override { return ForwardIterator(m_data + m_size); }
    BackwardIterator rbegin() const { return BackwardIterator(m_data + m_size - 1); }
    BackwardIterator rend()   const { return BackwardIterator(m_data - 1); }
};

template <typename Traits, template <typename> typename... Mixins>
void BaseVector<Traits, Mixins...>::resize(size_t new_cap) {
    if (new_cap <= m_capacity) return;
    NodePtr new_data = new Node[new_cap];
    for (size_t i = 0; i < m_size; ++i)
        new_data[i] = m_data[i];
    delete[] m_data;
    m_data = new_data;
    m_capacity = new_cap;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::operator=(const BaseVector& other) -> BaseVector& {
    this->copy_from(other);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::operator=(BaseVector&& other) noexcept -> BaseVector& {
    this->move_from(other, [&]{
        m_data     = std::exchange(other.m_data, nullptr);
        m_size     = std::exchange(other.m_size, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    });
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseVector<Traits, Mixins...>::push_back(const value_type& value, Ref ref) {
    std::lock_guard lock(m_mutex);
    if (m_size == m_capacity) {
        size_t new_cap = (m_capacity == 0) ? 10 : m_capacity * 2;
        resize(new_cap);
    }
    m_data[m_size] = Node(value, ref);
    ++m_size;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseVector<Traits, Mixins...>::pop_back() {
    std::lock_guard lock(m_mutex);
    if (m_size > 0) --m_size;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::operator[](size_t index) -> Node& {
    if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
    return m_data[index];
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::operator[](size_t index) const -> const Node& {
    if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
    return m_data[index];
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::at(size_t index) -> Node& {
    if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
    return m_data[index];
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::at(size_t index) const -> const Node& {
    if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
    return m_data[index];
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::back() -> Node& {
    if (m_size == 0) throw std::out_of_range("Vector vacio");
    return m_data[m_size - 1];
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseVector<Traits, Mixins...>::back() const -> const Node& {
    if (m_size == 0) throw std::out_of_range("Vector vacio");
    return m_data[m_size - 1];
}

template <typename Traits, template <typename> typename... Mixins>
void BaseVector<Traits, Mixins...>::clear() {
    std::lock_guard lock(m_mutex);
    delete [] m_data;
    m_data     = nullptr;
    m_size     = 0;
    m_capacity = 0;
}

#endif // __VECTOR_H__