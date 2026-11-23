/**
 * @file double_linked_list.h
 * @ingroup lists
 * @brief Lista doblemente enlazada (`BaseDoubleLinkedList` / `DoubleLinkedList`) e iterador inverso.
 */

#ifndef __DOUBLE_LINKEDLIST_H__
#define __DOUBLE_LINKEDLIST_H__
#include "linked_list.h"
#include "../core/mixins.h"

/**
 * @class LinkedListBackwardIterator
 * @brief Iterador inverso para listas doblemente enlazadas (maneja corte de ciclo en listas circulares).
 */
template <typename T>
class LinkedListBackwardIterator : 
    public GeneralIterator<LinkedListBackwardIterator<T>, LinkedListNode<T>> {
public:
    using value_type = LinkedListNode<T>;
    using MySelf = LinkedListBackwardIterator<T>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // hereda constructor
    using Parent::m_ptr;
    using Parent::m_pStart;

    /** @brief Retrocede al nodo anterior (`m_pPrev`) deteniéndose si cierra un ciclo. */
    LinkedListBackwardIterator& operator++() {
        m_ptr = m_ptr->m_pPrev == m_pStart ? nullptr : m_ptr->m_pPrev;
        return *this;
    }
};

/** @brief Rasgos de lista doblemente enlazada añadiendo `BackwardIterator`. */
template <typename T>
struct DoubleLinkedListTraits : public LinkedListTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

/** @brief Rasgos ascendentes para lista doblemente enlazada. */
template <typename T>
struct DoubleLinkedListAscTraits : public AscendingTraits<T>, public DoubleLinkedListTraits<T> {};

/** @brief Rasgos descendentes para lista doblemente enlazada. */
template <typename T>
struct DoubleLinkedListDescTraits : public DescendingTraits<T>, public DoubleLinkedListTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseDoubleLinkedList;

/** @brief Alias estándar de lista doblemente enlazada con soporte `Reversible`. */
template <typename Traits>
using DoubleLinkedList = BaseDoubleLinkedList<Traits, Callable, Searchable, Applicable, Reversible, Printable, Readable>;

/**
 * @class BaseDoubleLinkedList
 * @brief Lista doblemente enlazada que mantiene punteros `m_pPrev` y soporta recorrido inverso.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseDoubleLinkedList :
    virtual public BaseLinkedList<Traits, Mixins...> {
public:
    using Parent = BaseLinkedList<Traits, Mixins...>;
    using BackwardIterator = typename Traits::BackwardIterator;
    using typename Parent::NodePtr;

protected:
    using Parent::m_pTail;

    /** @brief Enlaza los punteros `m_pPrev` del nodo insertado y su sucesor. */
    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override;

public:
    BaseDoubleLinkedList() = default;
    BaseDoubleLinkedList(const BaseDoubleLinkedList& other) { *this = other; }
    BaseDoubleLinkedList(BaseDoubleLinkedList&& other) noexcept { *this = std::move(other); }
    BaseDoubleLinkedList& operator=(const BaseDoubleLinkedList& other) { Parent::operator=(other); return *this; }
    BaseDoubleLinkedList& operator=(BaseDoubleLinkedList&& other) noexcept { Parent::operator=(std::move(other)); return *this; }

    /** @brief Retorna el iterador inverso apuntando a la cola de la lista. */
    BackwardIterator rbegin() const { return BackwardIterator(m_pTail); }
    /** @brief Retorna el iterador centinela de fin del recorrido inverso. */
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }
};

template <typename Traits, template <typename> typename... Mixins>
void BaseDoubleLinkedList<Traits, Mixins...>::linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext) {
    pNew->m_pPrev = pPrev;
    if (pNext) pNext->m_pPrev = pNew;
}

#endif