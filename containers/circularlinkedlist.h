#ifndef __CIRCULAR_LINKEDLIST_H__
#define __CIRCULAR_LINKEDLIST_H__

#include "linkedlist.h"

template < typename Traits >
class CircularLinkedList : virtual public LinkedList< Traits > {
public:
    using Base    = LinkedList<Traits>;
    using NodePtr = typename Base::NodePtr;

protected:
    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        if (this->m_pTail) this->m_pTail->m_pNext = this->m_pRoot;
    }
};

#endif