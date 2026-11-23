/**
 * @file circular_linked_list.h
 * @ingroup lists
 * @brief Lista circular simplemente enlazada (`BaseCircularLinkedList` / `CircularLinkedList`).
 */

#ifndef __CIRCULAR_LINKEDLIST_H__
#define __CIRCULAR_LINKEDLIST_H__
#include "linked_list.h"
#include "../../utilities/render.h"

template <typename Traits, template <typename> typename... Mixins>
class BaseCircularLinkedList;

/** @brief Alias estándar de lista circular simplemente enlazada con soporte `Drawable`. */
template <typename Traits>
using CircularLinkedList = BaseCircularLinkedList<Traits, Callable, Searchable, Applicable, Printable, Readable, Drawable>;

/**
 * @class BaseCircularLinkedList
 * @brief Extiende `BaseLinkedList` cerrando el ciclo `m_pTail->m_pNext = m_pRoot`.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseCircularLinkedList : virtual public BaseLinkedList<Traits, Mixins...> {
public:
    using Parent = BaseLinkedList<Traits, Mixins...>;
    using NodePtr = typename Parent::NodePtr;
protected:
    using Parent::m_pTail;
    using Parent::m_pRoot;

    /** @brief Cierra el enlace circular conectando la cola con la raíz. */
    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        if (m_pTail) m_pTail->m_pNext = m_pRoot;
    }
public:
    BaseCircularLinkedList() = default;
    BaseCircularLinkedList(const BaseCircularLinkedList& other) { *this = other; }
    BaseCircularLinkedList(BaseCircularLinkedList&& other) noexcept { *this = std::move(other); }
    BaseCircularLinkedList& operator=(const BaseCircularLinkedList& other) { Parent::operator=(other); return *this; }
    BaseCircularLinkedList& operator=(BaseCircularLinkedList&& other) noexcept { Parent::operator=(std::move(other)); return *this; }
};

#endif