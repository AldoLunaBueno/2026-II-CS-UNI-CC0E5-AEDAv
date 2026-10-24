#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
    NodePtr m_parent = nullptr;

};

template <typename T, TX Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T, Dir>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeIterator<T, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using Parent::Parent;
    NodePtr m_parent = nullptr;

    MySelf& operator++() {
        auto node = Parent::m_ptr;
        if (node == nullptr)
            return *this;
        if (node->m_pChild[1-Dir]) {
            node = node->m_pChild[1-Dir];
            while (node->m_pChild[Dir])
                node = node->m_pChild[Dir];
            Parent::m_ptr = node;
        } else {
            auto parent = node->m_parent;
            while (parent && node == parent->m_pChild[1-Dir]) {
                node = parent;
                parent = parent->m_parent;
            }
            Parent::m_ptr = parent;
        }

        return *this;
    }
};

template <typename T>
using BinaryTreeForwardIterator = BinaryTreeIterator<T, 0>;

template <typename T>
using BinaryTreeBackwardIterator = BinaryTreeIterator<T, 1>;

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T> {
    using value_type        = T;
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T>;
};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T> {
    using value_type        = T;
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T>;
};

template <typename Traits>
class BinaryTree : public GeneralContainer<Traits, Callable, Applicable, Searchable, ReverseCallable> {
public:
    using Parent            = GeneralContainer<Traits, Callable, Applicable, Searchable, ReverseCallable>;
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using NodePtr           = Node*;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;
    using Compare           = Traits::Compare;



private:
    NodePtr m_root = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr) {
        if (!node) {
            node = new Node(value, ref);
            node->m_parent = parent;
            return;
        }
        auto branch = Compare()(node->getValue(), value);
        internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
    }
public:
    virtual ~BinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        // Implement insertion logic for binary tree
        internal_insert(m_root, value, ref);
    }
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override {
        clear_recursive(m_root);
        m_root = nullptr;
    }

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    void clear_recursive(NodePtr node) {
        if (!node) return;
        clear_recursive(node->m_pChild[0]);
        clear_recursive(node->m_pChild[1]);
        delete node;
    }

    ForwardIterator begin() const override {
        NodePtr curr = m_root;
        while (curr && curr->m_pChild[0]) curr = curr->m_pChild[0];
        return ForwardIterator(curr);
    }

    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const override {
        NodePtr curr = m_root;
        while (curr && curr->m_pChild[1]) curr = curr->m_pChild[1];
        return BackwardIterator(curr);
    }

    BackwardIterator rend() const override { return BackwardIterator(nullptr); }

    BinaryTree& operator=(BinaryTree& another) noexcept {
        std::scoped_lock lock(this->m_mutex, another.m_mutex);
        clear_recursive(m_root);
        m_root = std::exchange(another.m_root, nullptr);
        return *this;
    }
};

#endif // __BINARYTREE_H__