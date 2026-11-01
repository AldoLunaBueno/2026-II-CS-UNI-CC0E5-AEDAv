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

namespace TraversalOrder { enum Type { PreOrder = 0, InOrder = 1, PostOrder = 2 }; };
namespace Direction { enum Type {Asc = 0, Desc = 1 }; };

template <typename T, TX Order, TX Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T, Order, Dir>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeIterator<T, Order, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using Parent::Parent;
    NodePtr m_parent = nullptr;
    static const TX order = Order;

    MySelf &operator++();

    void step_until_order(TX phase);
};

template <typename T, TX Order, TX Dir>
auto BinaryTreeIterator<T, Order, Dir>::operator++() -> MySelf& {
    step_until_order(Order);
    return *this;
}

// Máquina de estados del recorrido de Euler por fases
template <typename T, TX Order, TX Dir>
void BinaryTreeIterator<T, Order, Dir>::step_until_order(TX phase) {
    auto& node = Parent::m_ptr;
    do {
        switch (phase) {
        case 0:
            if (node->m_pChild[Dir]) node = node->m_pChild[Dir];
            else phase = 1;
            break;
        case 1:
            if (node->m_pChild[1 - Dir]) {
                node = node->m_pChild[1 - Dir];
                phase = 0;
            }
            else phase = 2;
            break;
        default:
            auto parent = node->m_parent;
            if (parent && parent->m_pChild[Dir] && node == parent->m_pChild[Dir])
                phase = 1;
            node = parent;
        }
    } while (phase != Order && node);
}

template <typename T>
using BinaryTreeInOrderForwardIterator = BinaryTreeIterator<T, TraversalOrder::InOrder, Direction::Asc>;

template <typename T>
using BinaryTreeInOrderBackwardIterator = BinaryTreeIterator<T, TraversalOrder::InOrder, Direction::Desc>;

template <typename T>
using BinaryTreePreOrderForwardIterator = BinaryTreeIterator<T, TraversalOrder::PreOrder, Direction::Asc>;

template <typename T>
using BinaryTreePreOrderBackwardIterator = BinaryTreeIterator<T, TraversalOrder::PreOrder, Direction::Desc>;

template <typename T>
using BinaryTreePostOrderForwardIterator = BinaryTreeIterator<T, TraversalOrder::PostOrder, Direction::Asc>;

template <typename T>
using BinaryTreePostOrderBackwardIterator = BinaryTreeIterator<T, TraversalOrder::PostOrder, Direction::Desc>;

template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeInOrderForwardIterator<T>;
    using BackwardIterator  = BinaryTreeInOrderBackwardIterator<T>;
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
    NodePtr m_pRoot = nullptr; // raíz del árbol binario
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
    if (!m_pRoot) return ForwardIterator(nullptr);
    auto it = ForwardIterator(m_pRoot);
    if constexpr (ForwardIterator::order != TraversalOrder::PreOrder)
        it.step_until_order(0);
    return it;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::rbegin() const -> BackwardIterator {
    return BackwardIterator(m_pRoot);
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