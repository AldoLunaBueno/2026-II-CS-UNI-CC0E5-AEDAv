#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "../foreach.h"

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
    using value_type = LinkedListNode<T>;
    using MySelf = LinkedListForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    LinkedListForwardIterator& operator++() { 
        Parent::m_ptr = Parent::m_ptr->pNext == pTail ? nullptr : Parent::m_ptr->m_pNext;
        return *this;
    }
};

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type        = T;
    using Compare           = _Compare;
};
template <typename T, typename _Compare = std::less<T>>
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template <typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};
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
class LinkedList {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Compare           = typename Traits::Compare;
    using Delim             = typename Node::Delim;
protected:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada

    Compare m_comp; // comparador para ordenar los nodos de la lista
    mutable std::mutex m_mutex; // mutex para sincronización

    virtual void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) {}
private:

    NodePtr GetRoot() const { return m_pRoot; }
    void internalInsert(const value_type& value, Ref ref, NodePtr& rParent, NodePtr pPrev = nullptr);

public:
    LinkedList() {}
    LinkedList(const LinkedList& another){ *this = another; } // copia profunda de la lista enlazada
    LinkedList& operator=(const LinkedList& another); // no se permite asignacion
    LinkedList(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    void clear();
    virtual ~LinkedList(){ clear(); };

    void push_back(const value_type& value, Ref ref);

    bool empty() const { return m_pRoot == nullptr; }

    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        internalInsert(value, ref, m_pRoot);
    }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }

    friend std::ostream& operator <<(std::ostream& os, const LinkedList<Traits>& list) {
        lock_guard lock(list.m_mutex);
        auto first = true;
        os << "[";
        for (auto it = list.begin(); it != list.end(); ++it){
            if (!first)
                os << ",";
            os << *it;
            first = false;
        }
        return os << "]";
    }
    
    friend std::istream &operator >>(std::istream &is, LinkedList<Traits> &list) {
        Delim d;
        Node node;
        list.clear();

        is >> d;
        if (is >> d && d != ']'){
            is.unget();
            while(is >> node >> d){
                list.push_back(node.getValue(), node.getRef());
                if (d == ']'){
                    break;
                }
            }    
        }

        return is; 
    }
    
    // Iterators
    // ForwardIterator begin() { return ForwardIterator(m_pRoot); }
    // ForwardIterator end() { return ForwardIterator(nullptr); }
    ForwardIterator begin() const { return ForwardIterator(m_pRoot); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        call(func, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args... args) {
        return call(func, std::forward<Args>(args)...);
    }
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args)
    {    lock_guard<mutex> lock(m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, Node&, Args...>>)
            ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};


template <typename Traits>
LinkedList<Traits>& LinkedList<Traits>::operator=(const LinkedList<Traits>& other){ 
    clear();
    if(!other.m_pRoot)
        return *this;
    std::lock_guard<std::mutex> lock(other.m_mutex);

    m_pRoot = new Node(other.GetRoot()->getValue(), other.GetRoot()->getRef(), nullptr);

    NodePtr next = other.m_pRoot->m_pNext;
    NodePtr curr = m_pRoot;

    while(next){
        curr->m_pNext = new Node(next->getValue(), next->getRef(), nullptr);
        curr = curr->m_pNext;
        next = next->m_pNext;
    }
    m_pTail = curr;
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
    NodePtr pPrev = this->m_pTail; // Guardar cola anterior antes de crear
    NodePtr new_node = new Node(value, ref, nullptr);
    if (!this->m_pRoot){
        this->m_pRoot = new_node;
        this->m_pTail = new_node;
    } else {
        this->m_pTail->m_pNext = new_node;
        this->m_pTail = new_node;
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
    if (rParent == nullptr || value < rParent->getValue()) {
        rParent = new Node(value, ref, rParent);
        if (rParent->m_pNext == nullptr) m_pTail = rParent;
        linkNode(rParent, pPrev, rParent->m_pNext);
        return;
    }
    internalInsert(value, ref, rParent->m_pNext, rParent);
}
#endif // __LINKEDLIST_H__
