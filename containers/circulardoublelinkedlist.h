#ifndef __CIRCULAR_DOUBLE_LINKEDLIST_H__
#define __CIRCULAR_DOUBLE_LINKEDLIST_H__
#include "circularlinkedlist.h"
#include "doublelinkedlist.h"

template <typename Traits, template <typename> typename... Mixins>
class BaseCircularDoubleLinkedList;

template <typename Traits>
using CircularDoubleLinkedList = BaseCircularDoubleLinkedList<Traits, Callable, Searchable, Applicable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseCircularDoubleLinkedList : virtual public BaseCircularLinkedList<Traits, Mixins...>,
                                     virtual public BaseDoubleLinkedList<Traits, Mixins...> {
public:
    using Parent            = BaseLinkedList<Traits, Mixins...>;
    using CircularParent    = BaseCircularLinkedList<Traits, Mixins...>;
    using DoubleParent      = BaseDoubleLinkedList<Traits, Mixins...>;
    using NodePtr           = typename Parent::NodePtr;
protected:
    using Parent::m_pRoot;
    using Parent::m_pTail;

    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override;
public:
    BaseCircularDoubleLinkedList() = default;
    BaseCircularDoubleLinkedList(const BaseCircularDoubleLinkedList& other) { *this = other; }
    BaseCircularDoubleLinkedList(BaseCircularDoubleLinkedList&& other) noexcept { *this = std::move(other); }
    BaseCircularDoubleLinkedList& operator=(const BaseCircularDoubleLinkedList& other) { Parent::operator=(other); return *this; }
    BaseCircularDoubleLinkedList& operator=(BaseCircularDoubleLinkedList&& other) noexcept { Parent::operator=(std::move(other)); return *this; }
};

template <typename Traits, template <typename> typename... Mixins>
void BaseCircularDoubleLinkedList<Traits, Mixins...>::linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext) {
    CircularParent::linkNode(pNew, pPrev, pNext);
    DoubleParent::linkNode(pNew, pPrev, pNext);
    if (m_pRoot) m_pRoot->m_pPrev = m_pTail;
}

#endif