#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <initializer_list>
#include "GeneralContainer.h"
#include "GeneralIterator.h"
#include "GeneralNode.h"
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
struct VectorAscTraits {
    using value_type        = T;
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;  // itera sobre Node, no sobre T
    using BackwardIterator  = VectorBackwardIterator<Node>; // itera sobre Node, no sobre T
};

template <typename Traits>
class Vector : public GeneralContainer<Traits> {
public:
    using Base              = GeneralContainer<Traits>;
    using value_type        = typename Base::value_type;
    using Node              = typename Base::Node;
    using ForwardIterator   = typename Base::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;

private:
    Node        *m_data     = nullptr;   // puntero al arreglo dinámico
    size_t       m_size     = 0,         // cantidad actual
                 m_capacity = 0;         // capacidad
    using Base::m_mutex;

    void resize(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        Node* new_data = new Node[new_cap];
        for (size_t i = 0; i < m_size; ++i)
            new_data[i] = m_data[i];
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_cap;
    }

public:
    Vector() {}

    // Cada elemento de la lista es una pareja (valor, ref) para un Node
    Vector(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    virtual ~Vector() {
        clear();
    }

    Vector(const Vector& other) {
        resize(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i)
            m_data[i] = other.m_data[i];
        m_size = other.m_size;
    }

    // Move constructor and move assignment operator
    Vector(Vector&& other) noexcept {
        m_data     = std::exchange(other.m_data, nullptr);
        m_size     = std::exchange(other.m_size, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    }

    // Move assignment operator
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            clear();
            m_data     = std::exchange(other.m_data, nullptr);
            m_size     = std::exchange(other.m_size, 0);
            m_capacity = std::exchange(other.m_capacity, 0);
        }
        return *this;
    }

    void push_back(const value_type& value, Ref ref) {
        lock_guard<mutex> lock(m_mutex);
        if (m_size == m_capacity) {
            size_t new_cap = (m_capacity == 0) ? 10 : m_capacity * 2;
            resize(new_cap);
        }
        m_data[m_size] = Node(value, ref);
        ++m_size;
    }

    void pop_back() {
        lock_guard<mutex> lock(m_mutex);
        if (m_size > 0)
            --m_size;
    }

    Node& operator[](size_t index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return m_data[index];
    }

    Node& at(size_t index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return m_data[index];
    }

    size_t size()     const { return m_size; }
    size_t capacity() const { return m_capacity; }

    void clear() {
        lock_guard<mutex> lock(m_mutex);
        delete [] m_data;
        m_data     = nullptr;
        m_size     = 0;
        m_capacity = 0;
    }

    ForwardIterator   begin() const override { return ForwardIterator(m_data); }
    ForwardIterator   end()   const override { return ForwardIterator(m_data + m_size); }
    BackwardIterator rbegin() const { return BackwardIterator(m_data + m_size - 1); }
    BackwardIterator rend()   const { return BackwardIterator(m_data - 1); }


    // TODO: implementar la lectura de un vector desde un stream
    istream &read(istream &is){
        // Implementation for reading vector from stream
    } 

    // Igual que call(), pero recorriendo el container en sentido inverso
    // (rbegin()/rend()) en vez de hacia adelante.
    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args)
    {    lock_guard<mutex> lock(m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, Node&, Args...>>)
            ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename T>
istream& operator>>(istream &is, Vector<T> &vec) {
    return vec.read(is);
}

#endif // __VECTOR_H__