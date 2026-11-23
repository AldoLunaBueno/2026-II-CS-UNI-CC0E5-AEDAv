/**
 * @file linked_list.h
 * @ingroup lists
 * @brief Lista simplemente enlazada (`BaseLinkedList` / `LinkedList`) e iterador hacia adelante.
 */

#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include "../core/general_container.h"
#include "../core/general_iterator.h"
#include "../core/general_node.h"
#include "../core/mixins.h"

/**
 * @class LinkedListNode
 * @brief Nodo para listas enlazadas simples, dobles y circulares.
 * @tparam T Tipo del valor almacenado en el nodo.
 */
template <typename T>
class LinkedListNode : public GeneralNode<T> {
    using Node = LinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pNext = nullptr; /**< Puntero al siguiente nodo. */
    Node* m_pPrev = nullptr; /**< Puntero al nodo anterior (usado en listas dobles). */
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr) {}
    /** @brief Construye un nodo enlazado con valor, referencia y puntero al siguiente. */
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref), m_pNext(pNext) {}
};

/**
 * @class LinkedListForwardIterator
 * @brief Iterador hacia adelante para listas enlazadas (maneja corte de ciclo en listas circulares).
 */
template <typename T>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<T>, LinkedListNode<T>> {
public:
    using Node   = LinkedListNode<T>;
    using MySelf = LinkedListForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, Node>;
    using Parent::Parent; // Inherit constructor
    using Parent::m_ptr;
    using Parent::m_pStart;

    /** @brief Avanza al siguiente nodo y se detiene al volver al nodo inicial en listas circulares. */
    LinkedListForwardIterator& operator++() { 
        m_ptr = m_ptr->m_pNext == m_pStart ? nullptr : m_ptr->m_pNext;
        return *this;
    }
};

/** @brief Rasgos base de nodo e iterador para listas simplemente enlazadas. */
template <typename T>
struct LinkedListTraits {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<T>;  /**< Itera sobre `Node`, no sobre `T`. */
};

/** @brief Rasgos de lista enlazada con inserción ordenada ascendente. */
template <typename T>
struct LinkedListAscTraits : public AscendingTraits<T>, public LinkedListTraits<T> {};

/** @brief Rasgos de lista enlazada con inserción ordenada descendente. */
template <typename T>
struct LinkedListDescTraits : public DescendingTraits<T>, public LinkedListTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseLinkedList;

/** @brief Alias estándar de lista simplemente enlazada. */
template <typename Traits>
using LinkedList = BaseLinkedList<Traits, Callable, Searchable, Applicable, Printable, Readable>;

/**
 * @class BaseLinkedList
 * @brief Lista simplemente enlazada genérica y thread-safe con inserción al final y ordenada.
 *
 * @tparam Traits Rasgos de la lista.
 * @tparam Mixins Mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseLinkedList : public GeneralContainer<Traits, Mixins...> {
public:  
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using typename Parent::Delim;
    using Compare = typename Traits::Compare;
protected:
    NodePtr m_pRoot = nullptr; /**< Puntero al primer nodo de la lista enlazada. */
    NodePtr m_pTail = nullptr; /**< Puntero al último nodo de la lista enlazada. */

    Compare m_comp; /**< Comparador para inserción ordenada en `insert()`. */
    using Parent::m_mutex;

    /** @brief Gancho virtual para enlazar punteros adicionales en subclases (doble/circular). */
    virtual void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) {}
private:

    NodePtr GetRoot() const { return m_pRoot; }
    /** @brief Inserta recursivamente manteniendo el orden definido por `m_comp`. */
    void internalInsert(const value_type& value, Ref ref, NodePtr& rParent, NodePtr pPrev = nullptr);

public:
    BaseLinkedList() = default;
    BaseLinkedList(const BaseLinkedList& another) { *this = another; }
    BaseLinkedList(BaseLinkedList&& another) noexcept { *this = std::move(another); }
    BaseLinkedList& operator=(const BaseLinkedList& another);
    BaseLinkedList& operator=(BaseLinkedList&& another) noexcept;
    /** @brief Construye la lista insertando los pares `(valor, ref)` al final. */
    BaseLinkedList(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    /** @brief Libera todos los nodos de la lista. */
    void clear() override;
    virtual ~BaseLinkedList() { clear(); }

    /** @brief Inserta un nodo al final de la lista en O(1). */
    void push_back(const value_type& value, Ref ref) override;

    /**
     * @brief Inserta un nodo en la posición ordenada según `Compare`.
     * @param value Valor del nuevo nodo.
     * @param ref Referencia asociada.
     */
    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        if (m_pTail) m_pTail->m_pNext = nullptr; // Si es circular rompe el ciclo temporalmente
        internalInsert(value, ref, m_pRoot);
    }
    
    // Iterators
    // ForwardIterator begin() { return ForwardIterator(m_pRoot); }
    // ForwardIterator end() { return ForwardIterator(nullptr); }
    ForwardIterator begin() const override { return ForwardIterator(m_pRoot); }
    ForwardIterator end() const override { return ForwardIterator(nullptr); }
};


template <typename Traits, template <typename> typename... Mixins>
auto BaseLinkedList<Traits, Mixins...>::operator=(const BaseLinkedList& other) -> BaseLinkedList& { 
    this->copy_from(other);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseLinkedList<Traits, Mixins...>::operator=(BaseLinkedList&& other) noexcept -> BaseLinkedList& {
    this->move_from(other, [&]{
        m_pRoot = std::exchange(other.m_pRoot, nullptr);
        m_pTail = std::exchange(other.m_pTail, nullptr);
    });
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseLinkedList<Traits, Mixins...>::clear(){
    scoped_lock lock(m_mutex);
    if (!m_pRoot) return;
    if (m_pTail) m_pTail->m_pNext = nullptr; // Si es circular rompe el ciclo
    NodePtr curr = m_pRoot;
    while (curr) {
        NodePtr next = curr->m_pNext;
        delete curr;
        curr = next;
    }
    m_pRoot = nullptr;
    m_pTail = nullptr;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseLinkedList<Traits, Mixins...>::push_back(const value_type& value, Ref ref){
    scoped_lock lock(m_mutex);
    NodePtr pPrev = m_pTail; // Guardar cola anterior antes de crear
    NodePtr new_node = new Node(value, ref, nullptr);
    if (!m_pRoot){
        m_pRoot = new_node;
        m_pTail = new_node;
    } else {
        m_pTail->m_pNext = new_node;
        m_pTail = new_node;
    }
    linkNode(new_node, pPrev, nullptr);
}

/*
internalInsert(...) recorre todos los nodos hasta llegar al final, y en la condición de parada
si el nodo padre es nullptr, crea uno nuevo con el valor y retorna
insert(...) empieza desde el nodo raiz
*/
template <typename Traits, template <typename> typename... Mixins>
void BaseLinkedList<Traits, Mixins...>::internalInsert(const value_type& value, Ref ref, NodePtr& rParent, NodePtr pPrev) {
    if (rParent == nullptr || m_comp(value, rParent->getValue())) {
        rParent = new Node(value, ref, rParent);
        if (rParent->m_pNext == nullptr) m_pTail = rParent;
        linkNode(rParent, pPrev, rParent->m_pNext);
        return;
    }
    internalInsert(value, ref, rParent->m_pNext, rParent);
}

#endif // __LINKEDLIST_H__
