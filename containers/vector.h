#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <initializer_list>
#include "GeneralContainer.h"
#include "GeneralIterator.h"
#include "GeneralNode.h"
#include "mixins.h"

using namespace std;

template <typename T>
class VectorForwardIterator : public GeneralIterator<VectorForwardIterator<T>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    VectorForwardIterator& operator++() { ++Parent::m_ptr; return *this; }
};

template <typename T>
class VectorBackwardIterator : public GeneralIterator<VectorBackwardIterator<T>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorBackwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    VectorBackwardIterator& operator++() { --Parent::m_ptr; return *this; }
};

template <typename T>
struct VectorTraits {
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;  // itera sobre Node, no sobre T
    using BackwardIterator  = VectorBackwardIterator<Node>; // itera sobre Node, no sobre T
};

template <typename T>
struct VectorAscTraits : public AscendingTraits<T>, public VectorTraits<T> {};

template <typename T>
struct VectorDescTraits : public DescendingTraits<T>, public VectorTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseVector;

template <typename Traits>
using Vector = BaseVector<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

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
    NodePtr      m_data     = nullptr;   // puntero al arreglo dinámico
    size_t       m_size     = 0,         // cantidad actual
                 m_capacity = 0;         // capacidad

    void resize(size_t new_cap);

public:
    BaseVector() {}

    // Cada elemento de la lista es una pareja (valor, ref) para un Node
    BaseVector(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    virtual ~BaseVector() { clear(); }

    BaseVector(const BaseVector& other) { *this = other; }
    BaseVector(BaseVector&& other) noexcept { *this = std::move(other); }

    BaseVector& operator=(const BaseVector& other);
    BaseVector& operator=(BaseVector&& other) noexcept;

    void push_back(const value_type& value, Ref ref) override;
    void pop_back();

    Node& operator[](size_t index);
    Node& at(size_t index);

    size_t size()     const { return m_size; }
    size_t capacity() const { return m_capacity; }

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
auto BaseVector<Traits, Mixins...>::at(size_t index) -> Node& {
    if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
    return m_data[index];
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