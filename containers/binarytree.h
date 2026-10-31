#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"
#include "../types.h"
#include "../utilities/render.h"
#include <iostream>

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

    MySelf& operator++();
};

template <typename T, TX Dir>
auto BinaryTreeIterator<T, Dir>::operator++() -> MySelf& {
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

template <typename T>
using BinaryTreeForwardIterator = BinaryTreeIterator<T, 0>;

template <typename T>
using BinaryTreeBackwardIterator = BinaryTreeIterator<T, 1>;

template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T>;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree;

template <typename Traits>
using BinaryTree = BaseBinaryTree<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable, Drawable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseBinaryTree;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;

protected:
    using Parent::m_mutex;

private:
    NodePtr m_pRoot = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr);
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(m_pRoot, value, ref);
    }
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    void clear_recursive(NodePtr);

    ForwardIterator begin() const override;
    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const;
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    BaseBinaryTree& operator=(const BaseBinaryTree& another);
    BaseBinaryTree& operator=(BaseBinaryTree&& another) noexcept;

    VisualGraph sketch() const;
    TX height(const NodePtr&) const;
    TX height() const { return height(m_pRoot); };
};

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        return;
    }
    auto branch = Compare()(node->getValue(), value);
    internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
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
auto BaseBinaryTree<Traits, Mixins...>::begin() const -> ForwardIterator {
    NodePtr curr = m_pRoot;
    while (curr && curr->m_pChild[0]) curr = curr->m_pChild[0];
    return ForwardIterator(curr);
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::rbegin() const -> BackwardIterator {
    NodePtr curr = m_pRoot;
    while (curr && curr->m_pChild[1]) curr = curr->m_pChild[1];
    return BackwardIterator(curr);
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
VisualGraph BaseBinaryTree<Traits, Mixins...>::sketch() const {
    VisualGraph graph;
    if (!m_pRoot) return graph;

    TX cell_width = 3;
    TX h = height();
    TX line_width = (1 << (h - 1)) * cell_width;
    TX total_height = h > 0 ? (h - 1) * cell_width + 1 : 0;

    graph.width = line_width;
    graph.height = total_height;
    graph.cell_width = cell_width;

    SimpleVector<NodePtr> curr_level;
    curr_level.push_back(m_pRoot);

    SimpleVector<TX> curr_ids;
    curr_ids.push_back(0);
    TX next_id = 1;
    TX num_level_nodes = 1;

    for (TX k = 0; k < h; k++) {
        TX segment_width = cell_width * (1 << (h - k - 1));
        SimpleVector<NodePtr> next_level;
        SimpleVector<TX> next_ids;
        TX next_size = 1 << (k + 1);
        for (TX s = 0; s < next_size; s++) {
            next_level.push_back(nullptr);
            next_ids.push_back(-1);
        }

        for (TX i = 0; i < num_level_nodes; i++) {
            NodePtr node = curr_level[i].getValue();
            if (node) {
                TX node_id = curr_ids[i].getValue();
                TX x = i * segment_width;
                TX y = k * cell_width;

                graph.nodes.push_back(VisualNode{node_id, std::to_string(node->getValue()), x, y});

                // crear aristas
                for (size_t j=0; j<2; j++) {
                    if (node->m_pChild[j]) {
                        next_level[2 * i + j].value() = node->m_pChild[j];
                        TX to_id = next_id++;
                        next_ids[2 * i + j].value() = to_id;
                        graph.edges.push_back(VisualEdge{
                            node_id,
                            to_id,
                            (j == 0 ? VisualEdgeType::LeftChild : VisualEdgeType::RightChild)
                        });
                    }
                }
            }
        }

        if (k == h - 1) break;

        curr_level = std::move(next_level);
        curr_ids = std::move(next_ids);
        num_level_nodes *= 2;
    }

    return graph;
}

template <typename Traits, template <typename> typename... Mixins>
TX BaseBinaryTree<Traits, Mixins...>::height(const NodePtr& root) const {
        if (!root) return 0;
        return 1 + std::max(height(root->m_pChild[0]), height(root->m_pChild[1]));
}

#endif // __BINARYTREE_H__