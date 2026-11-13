#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "../core/general_node.h"
#include "../core/general_iterator.h"
#include "../core/general_container.h"
#include "../core/mixins.h"
#include "../../types.h"
#include "../../utilities/render.h"
#include <iostream>

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // constructor heredado
    NodePtr m_pChild[2] = {nullptr, nullptr}; // hijo izquierdo y derecho
    NodePtr m_parent = nullptr; // padre
    TX m_height = 0;
};

namespace detail {
    enum Phase : TX {
        Pre  = 0b100,
        In   = 0b010,
        Post = 0b001
    };
    enum Order : TX {
    // Puros
        PreOrder        = Phase::Pre,
        InOrder         =              Phase::In,
        PostOrder       =                          Phase::Post,
    // Híbridos
        PreInOrder      = Phase::Pre + Phase::In,
        PrePostOrder    = Phase::Pre             + Phase::Post,
        InPostOrder     =              Phase::In + Phase::Post,
        PreInPostOrder  = Phase::Pre + Phase::In + Phase::Post,
    };
    enum Direction : TX {Asc = 0, Desc = 1 };
}

using detail::Phase;
using detail::Order;
using detail::Direction;

template <typename T, Order Ord, Direction Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T, Ord, Dir>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeIterator<T, Ord, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using Parent::m_ptr;
protected:
    static const TX order      = Ord;
    static const TX FirstPhase = (Dir == Direction::Asc) ? Phase::Pre  : Phase::Post;
    static const TX LastPhase  = (Dir == Direction::Asc) ? Phase::Post : Phase::Pre;
    TX m_phase;
public:
    BinaryTreeIterator(NodePtr root) : Parent(root) {
        m_phase = FirstPhase;
        if (m_ptr && !(MySelf::order & FirstPhase)) {
            step_until_phase();
        }
    }
    
    MySelf &operator++();

    friend bool operator== (const MySelf& a, const MySelf& b) {
        return a.m_ptr == b.m_ptr && a.m_phase == b.m_phase;
    }
    friend bool operator!= (const MySelf& a, const MySelf& b) {
        return a.m_ptr != b.m_ptr || a.m_phase != b.m_phase;
    }

    void step_until_phase();
};

template <typename T, Order Ord, Direction Dir>
auto BinaryTreeIterator<T, Ord, Dir>::operator++() -> MySelf& {
    step_until_phase();
    return *this;
}

// Máquina de estados del recorrido de Euler por fases
template <typename T, Order Ord, Direction Dir>
void BinaryTreeIterator<T, Ord, Dir>::step_until_phase() {
    if (!m_ptr) return;
    do {
        switch (m_phase) {
        case FirstPhase:
            if (m_ptr->m_pChild[Dir]) m_ptr = m_ptr->m_pChild[Dir];
            else m_phase = Phase::In;
            break;
        case Phase::In:
            if (m_ptr->m_pChild[1 - Dir]) {
                m_ptr = m_ptr->m_pChild[1 - Dir];
                m_phase = FirstPhase;
            }
            else m_phase = LastPhase;
            break;
        case LastPhase:
            auto parent = m_ptr->m_parent;
            if (parent && parent->m_pChild[Dir] && m_ptr == parent->m_pChild[Dir])
                m_phase = Phase::In;
            m_ptr = parent;
        }
    } while (m_ptr && !(m_phase & Ord));
    if (!m_ptr) m_phase = FirstPhase;
}

template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeIterator<T, Order::InOrder, Direction::Asc>;
    using BackwardIterator  = BinaryTreeIterator<T, Order::InOrder, Direction::Desc>;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {
    using value_type = T;
};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree;

template <typename Traits>
using BinaryTree = BaseBinaryTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseBinaryTree;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Compare           = typename Traits::Compare;
protected:
    using Parent::m_mutex;
    NodePtr m_pRoot = nullptr; // raíz del árbol binario
    auto internalInsert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr);
    auto internalRemove(NodePtr node);
    virtual void rebalance(NodePtr node) {}
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    auto* insert(const value_type&, Ref);
    void remove(NodePtr);

    void connect(NodePtr, NodePtr, TX);

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    value_type* find(const value_type& value) {
        NodePtr node = m_pRoot;
        while (node) {
            auto branch = Compare()(node->getValue(), value);
            if (!branch && !Compare()(value, node->getValue())) {
                return &(node->value());
            }
            node = node->m_pChild[1-branch];
        }
        return nullptr;
    }

    void clear_recursive(NodePtr);

    ForwardIterator begin() const override { return ForwardIterator(m_pRoot); }
    ForwardIterator end()   const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const { return BackwardIterator(m_pRoot); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }

    template <Order Ord, Direction Dir = Direction::Asc>
    auto traverse() const {
        std::lock_guard lock(this->mutex());
        using Iter = BinaryTreeIterator<value_type, Ord, Dir>;
        using EulerView = typename Parent::Traits::template View<Node, Iter>;
        return EulerView(this->mutex(), Iter(m_pRoot), Iter(nullptr));
    }



    BaseBinaryTree& operator=(const BaseBinaryTree&);
    BaseBinaryTree& operator=(BaseBinaryTree&&) noexcept;

    NodePtr getRoot() const { return m_pRoot; }
    TX height(const NodePtr&) const;
    TX height() const { return height(m_pRoot); };
};


template <typename Traits, template <typename> typename... Mixins>
using value_type = BaseBinaryTree<Traits, Mixins...>::value_type;

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::internalInsert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        return node;
    }
    auto branch = Compare()(node->getValue(), value);
    return internalInsert(node->m_pChild[1-branch], value, ref, node); // Tail recursion to insert in the correct branch
}

template <typename Traits, template <typename> typename... Mixins>
auto* BaseBinaryTree<Traits, Mixins...>::insert(const value_type& value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = internalInsert(m_pRoot, value, ref);
    rebalance(node);
    return &(node->value());
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::remove(NodePtr node) {
    std::lock_guard lock(this->m_mutex);
    if (!node) return;
    NodePtr parent = internalRemove(node);
    rebalance(parent);
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::internalRemove(NodePtr node) {
    if (node->m_pChild[0] && node->m_pChild[1]) {
        NodePtr successor = node->m_pChild[1];
        while (successor->m_pChild[0]) successor = successor->m_pChild[0];
        node->value() = successor->getValue();
        node->ref() = successor->getRef();
        node = successor;
    }
    NodePtr parent = node->m_parent;
    TX dc = node->m_pChild[1] != nullptr;
    TX dp = parent && node == parent->m_pChild[1];
    connect(parent, node->m_pChild[dc], dp);
    delete node;
    return parent;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::connect(NodePtr parent, NodePtr child, TX d) {
    if (!parent) {
        if(child) child->m_parent = nullptr;
        m_pRoot = child;
        return;
    }
    parent->m_pChild[d] = child;
    if (child) child->m_parent = parent;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear() {
    std::lock_guard lock(this->m_mutex);
    clear_recursive(m_pRoot);
    m_pRoot = nullptr;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear_recursive(NodePtr node) {
    if (!node) return;
    clear_recursive(node->m_pChild[0]);
    clear_recursive(node->m_pChild[1]);
    delete node;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(const BaseBinaryTree& another) -> BaseBinaryTree& {
    this->copy_from(another);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(BaseBinaryTree&& another) noexcept -> BaseBinaryTree& {
    this->move_from(another, [&]{ m_pRoot = std::exchange(another.m_pRoot, nullptr); });
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
TX BaseBinaryTree<Traits, Mixins...>::height(const NodePtr& root) const {
        if (!root) return 0;
        return 1 + std::max(height(root->m_pChild[0]), height(root->m_pChild[1]));
}

#endif // __BINARYTREE_H__