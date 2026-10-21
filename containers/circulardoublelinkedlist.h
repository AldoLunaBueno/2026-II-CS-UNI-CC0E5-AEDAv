#ifndef __CIRCULAR_DOUBLE_LINKEDLIST_H__
#define __CIRCULAR_DOUBLE_LINKEDLIST_H__

#include "circularlinkedlist.h"
#include "doublelinkedlist.h"

template <typename Traits>
class CircularDoubleLinkedList : public CircularLinkedList<Traits>,
                                 public DoubleLinkedList<Traits> {
public:
    using Base         = LinkedList<Traits>;
    using CircularBase = CircularLinkedList<Traits>;
    using DoubleBase   = DoubleLinkedList<Traits>;
    using NodePtr      = typename Base::NodePtr;

protected:
    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        CircularBase::linkNode(pNew, pPrev, pNext);
        DoubleBase::linkNode(pNew, pPrev, pNext);
        if (this->m_pRoot) this->m_pRoot->m_pPrev = this->m_pTail;
    }
};

#endif