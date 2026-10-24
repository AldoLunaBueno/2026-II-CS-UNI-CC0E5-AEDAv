#ifndef __CIRCULAR_DOUBLE_LINKEDLIST_H__
#define __CIRCULAR_DOUBLE_LINKEDLIST_H__
#include "circularlinkedlist.h"
#include "doublelinkedlist.h"

template <typename Traits>
class CircularDoubleLinkedList : virtual public CircularLinkedList<Traits>,
                                 virtual public DoubleLinkedList<Traits> {
public:
    using Parent            = LinkedList<Traits>;
    using CircularParent    = CircularLinkedList<Traits>;
    using DoubleParent      = DoubleLinkedList<Traits>;
    using NodePtr           = typename Parent::NodePtr;
protected:
    using Parent::m_pRoot;
    using Parent::m_pTail;

    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        CircularParent::linkNode(pNew, pPrev, pNext);
        DoubleParent::linkNode(pNew, pPrev, pNext);
        if (m_pRoot) m_pRoot->m_pPrev = m_pTail;
    }
};

#endif