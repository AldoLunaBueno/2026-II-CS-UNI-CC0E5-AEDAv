#ifndef __DOUBLE_LINKEDLIST_H__
#define __DOUBLE_LINKEDLIST_H__
#include "linkedlist.h"
#include "mixins.h"

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

    LinkedListBackwardIterator& operator++() {
        m_ptr = m_ptr->m_pPrev == m_pStart ? nullptr : m_ptr->m_pPrev;
        return *this;
    }
};

template <typename T>
struct DoubleLinkedListTraits : public LinkedListTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

template <typename T>
struct DoubleLinkedListAscTraits : public AscendingTraits<T>, public DoubleLinkedListTraits<T> {};

template <typename T>
struct DoubleLinkedListDescTraits : public DescendingTraits<T>, public DoubleLinkedListTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseDoubleLinkedList;

template <typename Traits>
using DoubleLinkedList = BaseDoubleLinkedList<Traits, Callable, Searchable, Applicable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseDoubleLinkedList :
    virtual public BaseLinkedList<Traits, Mixins...> {
public:
    using Parent = BaseLinkedList<Traits, Mixins...>;
    using BackwardIterator = typename Traits::BackwardIterator;
    using typename Parent::NodePtr;

protected:
    using Parent::m_pTail;

    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override;

public:
    BackwardIterator rbegin() const { return BackwardIterator(m_pTail); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }
};

template <typename Traits, template <typename> typename... Mixins>
void BaseDoubleLinkedList<Traits, Mixins...>::linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext) {
    pNew->m_pPrev = pPrev;
    if (pNext) pNext->m_pPrev = pNew;
}

#endif