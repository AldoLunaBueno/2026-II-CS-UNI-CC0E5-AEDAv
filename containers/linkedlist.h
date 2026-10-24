#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include "GeneralContainer.h"
#include "GeneralIterator.h"
#include "GeneralNode.h"

template <typename T>
class LinkedListNode : public GeneralNode<T> {
    using Node = LinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    Node* m_pPrev = nullptr;
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr) {}
    // añadir friend class añade otro typename al template
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref), m_pNext(pNext) {}
};

template <typename T>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<T>, LinkedListNode<T>> {
public:
    using Node   = LinkedListNode<T>;
    using MySelf = LinkedListForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, Node>;
    using Parent::Parent; // Inherit constructor
    using Parent::m_ptr;
    using Parent::m_pStart;

    LinkedListForwardIterator& operator++() { 
        m_ptr = m_ptr->m_pNext == m_pStart ? nullptr : m_ptr->m_pNext;
        return *this;
    }
};

template <typename T>
struct LinkedListAscTraits : public AscendingTraits<T> {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<T>;  // itera sobre Node, no sobre T
};

template <typename T>
struct LinkedListDescTraits : public DescendingTraits<T> {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<T>;  // itera sobre Node, no sobre T
};
template <typename Traits>
class LinkedList : public GeneralContainer<Traits, Callable, Searchable, Applicable> {
public:  
    using Parent            = GeneralContainer<Traits, Callable, Searchable, Applicable>;
    using value_type        = typename Parent::value_type;
    using Node              = typename Parent::Node;
    using NodePtr           = Node*;
    using ForwardIterator   = typename Parent::ForwardIterator;
    using Compare           = typename Traits::Compare;
    using Delim             = typename Parent::Delim;
protected:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada

    Compare m_comp; // comparador para ordenar los nodos de la lista
    using Parent::m_mutex;

    virtual void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) {}
private:

    NodePtr GetRoot() const { return m_pRoot; }
    void internalInsert(const value_type& value, Ref ref, NodePtr& rParent, NodePtr pPrev = nullptr);

public:
    LinkedList() {}
    LinkedList(const LinkedList& another){ *this = another; } // copia profunda de la lista enlazada
    LinkedList& operator=(const LinkedList& another);
    LinkedList(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    void clear();
    virtual ~LinkedList(){ clear(); };

    void push_back(const value_type& value, Ref ref);

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


template <typename Traits>
LinkedList<Traits>& LinkedList<Traits>::operator=(const LinkedList<Traits>& other){ 
    if (this == &other) return *this;
    clear();
    std::lock_guard<std::mutex> lock(other.m_mutex);
    for (const auto& node : other)
        push_back(node.getValue(), node.getRef());
    return *this;
}

template <typename Traits>
void LinkedList<Traits>::clear(){
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

template<typename Traits>
void LinkedList<Traits>::push_back(const value_type& value, Ref ref){
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
template <typename Traits>
void LinkedList<Traits>::internalInsert(const value_type& value, Ref ref, NodePtr& rParent, NodePtr pPrev) {
    if (rParent == nullptr || m_comp(value, rParent->getValue())) {
        rParent = new Node(value, ref, rParent);
        if (rParent->m_pNext == nullptr) m_pTail = rParent;
        linkNode(rParent, pPrev, rParent->m_pNext);
        return;
    }
    internalInsert(value, ref, rParent->m_pNext, rParent);
}
#endif // __LINKEDLIST_H__
