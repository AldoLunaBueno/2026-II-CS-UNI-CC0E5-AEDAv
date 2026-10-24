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
public:
    LinkedListBackwardIterator& operator++() {
        m_ptr = m_ptr->m_pPrev == m_pStart ? nullptr : m_ptr->m_pPrev;
        return *this;
    }
};

template <typename T>
struct DoubleLinkedListAscTraits : public LinkedListAscTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

template <typename T>
struct DoubleLinkedListDescTraits : public LinkedListDescTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

template <typename Traits>
class DoubleLinkedList :
    virtual public LinkedList<Traits>,
    virtual public ReverseCallable<Traits> {
public:
    using Parent            = LinkedList<Traits>;
    using BackwardIterator  = ReverseCallable<Traits>::BackwardIterator;
    using NodePtr           = typename Parent::NodePtr;

protected:
    using Parent::m_pTail;

    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        pNew->m_pPrev = pPrev;
        if (pNext) pNext->m_pPrev = pNew;
    }

public:
    BackwardIterator rbegin() const override { return BackwardIterator(m_pTail); }
    BackwardIterator rend()   const override { return BackwardIterator(nullptr); }
};

#endif