#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<MySelf>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children

};

template <typename Traits>
class BinaryTree {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;

private:
    NodePtr m_root = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node, const value_type& value, Ref ref) {
        if (!node) {
            node = new Node(value, ref);
            return;
        }
        auto branch = value > node->m_value;
        internal_insert(node->m_pChild[branch], value, ref); // Tail recursion to insert in the correct branch
    }
public:
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
};
#endif // __BINARYTREE_H__