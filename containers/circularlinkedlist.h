#ifndef __CIRCULAR_LINKEDLIST_H__
#define __CIRCULAR_LINKEDLIST_H__

#include "linkedlist.h"

template < typename Traits >
class CircularLinkedList : virtual public LinkedList< Traits > {
public:
    using Base = LinkedList<Traits>;
    using Base::Base; // constructores default, initializer_list

protected:
    void linkNode(typename Traits::Node* pNew,
                  typename Traits::Node* pPrev,
                  typename Traits::Node* pNext = nullptr) override {
        if (this->m_pTail)
            this->m_pTail->m_pNext = this->m_pRoot;
    }
};

#endif