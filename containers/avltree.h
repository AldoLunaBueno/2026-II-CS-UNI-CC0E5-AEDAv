#ifndef __AVLTREE_H__
#define __AVLTREE_H__
#include "binarytree.h"
#include "../types.h"
#include <cmath>

template <typename Traits, template <typename> typename... Mixins>
class BaseAVLTree;

template <typename Traits>
using AVLTree = BaseAVLTree<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable, Drawable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseAVLTree : public BaseBinaryTree<Traits, Mixins...> {
public:
    using MySelf = BaseAVLTree;
    using Parent = BaseBinaryTree<Traits, Mixins...>;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Compare           = typename Traits::Compare;
protected:
    using Parent::m_pRoot;

    TX getHeight(NodePtr node) const { return (node) ? node->m_height : -1; }

    TX getbf(const NodePtr& node) const {
        return getHeight(node->m_pChild[0]) - getHeight(node->m_pChild[1]);
    }
    TX parentHeight(NodePtr parent) const { 
        return 1 + std::max(getHeight(parent->m_pChild[0]), getHeight(parent->m_pChild[1]));
    }

    void rebalance(NodePtr node) override {
        for(; node; node = node->m_parent) {
            node->m_height = parentHeight(node); // z
            TX bf_z = getbf(node);
            if (std::abs(bf_z) <= 1 ) continue;
            node = node->m_pChild[bf_z < 0]; // y: hijo más alto de z
            TX bf_y = getbf(node);
            node = node->m_pChild[bf_y < 0]; // x: hijo más alto de y

            if (bf_z * bf_y > 0) {
                rotate_up(node->m_parent); // rota 1 vez y
            } else {
                rotate_up(node); // rota 2 veces x 
                rotate_up(node);
            }
        }
    }

    void rotate_up(NodePtr node) {
        NodePtr node_p = node->m_parent;
        NodePtr node_g = node_p->m_parent;
        TX dp = node_p && node == node_p->m_pChild[1]; // true -> 1 (hijo derecho)
        TX dg = node_g && node_p == node_g->m_pChild[1];
        connect(node_p, node->m_pChild[1-dp], dp);
        connect(node, node_p, 1-dp);
        connect(node_g, node, dg);
    }

    void connect(NodePtr parent, NodePtr child, TX d) {
        if (!parent) {
            m_pRoot = child;
            child->m_parent = nullptr;
            return;
        }
        parent->m_pChild[d] = child;
        if (child) child->m_parent = parent;
        parent->m_height = parentHeight(parent);
    }
};

#endif // "__AVLTREE_H__"