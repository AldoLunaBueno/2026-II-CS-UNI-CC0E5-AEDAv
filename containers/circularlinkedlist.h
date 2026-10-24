#ifndef __CIRCULAR_LINKEDLIST_H__
#define __CIRCULAR_LINKEDLIST_H__
#include "linkedlist.h"

template <typename Traits>
class CircularLinkedList : virtual public LinkedList<Traits> {
public:
    using Parent    = LinkedList<Traits>;
    using NodePtr = typename Parent::NodePtr;
protected:
    using Parent::m_pTail;
    using Parent::m_pRoot;

    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        if (m_pTail) m_pTail->m_pNext = m_pRoot;
    }
};

#endif