/**
 * @file avl_tree.h
 * @ingroup trees
 * @brief Árbol binario autobalanceado AVL (`BaseAVLTree` / `AVLTree`).
 */

#ifndef __AVLTREE_H__
#define __AVLTREE_H__
#include "binary_tree.h"
#include "../../types.h"
#include <cmath>

template <typename Traits, template <typename> typename... Mixins>
class BaseAVLTree;

/** @brief Alias estándar de árbol AVL con soporte completo de mixins y visualización. */
template <typename Traits>
using AVLTree = BaseAVLTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseAVLTree
 * @brief Extiende `BaseBinaryTree` manteniendo el factor de equilibrio en `[-1, 1]` tras inserciones y borrados.
 */
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

    /** @brief Retorna la altura almacenada del nodo o `-1` si es `nullptr`. */
    TX getHeight(NodePtr node) const { return (node) ? node->m_height : -1; }

    /** @brief Calcula el factor de balance (`altura_izq - altura_der`) de `node`. */
    TX getbf(const NodePtr& node) const {
        return getHeight(node->m_pChild[0]) - getHeight(node->m_pChild[1]);
    }
    /** @brief Recalcula la altura de `node` a partir de las alturas de sus hijos. */
    TX calcHeight(NodePtr node) const {
        return 1 + std::max(getHeight(node->m_pChild[0]), getHeight(node->m_pChild[1]));
    }

    /**
     * @brief Recorre hacia la raíz actualizando alturas y aplicando rotaciones simples o dobles.
     * @param node Nodo desde el cual inicia el reequilibrio ascendente.
     */
    void rebalance(NodePtr node) override {
        for(; node; node = node->m_parent) {
            node->m_height = calcHeight(node); // z
            TX bf_z = getbf(node);
            if (std::abs(bf_z) <= 1 ) continue;
            node = node->m_pChild[bf_z < 0]; // y: hijo más alto de z
            TX bf_y = getbf(node);
            node = node->m_pChild[bf_y < 0]; // x: hijo más alto de y

            if (bf_z * bf_y > 0 || bf_y == 0) {
                rotate_up(node->m_parent); // rota 1 vez y
            } else {
                rotate_up(node); // rota 2 veces x 
                rotate_up(node);
            }
        }
    }

    /**
     * @brief Rotación unificada que asciende a `node` por encima de su padre.
     *
     * Maneja simétricamente rotaciones a izquierda y derecha determinando el lado `dp`.
     * @param node Nodo a promover hacia la posición de su padre.
     *
     * @image html avl_step_up.svg "Rotaciones para rebalancear AVL mediante rotate_up"
     */
    void rotate_up(NodePtr node) {
        NodePtr node_p = node->m_parent;
        NodePtr node_g = node_p->m_parent;
        TX dp = node_p && node == node_p->m_pChild[1]; // true -> 1 (hijo derecho)
        TX dg = node_g && node_p == node_g->m_pChild[1];
        connect(node_p, node->m_pChild[1-dp], dp);
        connect(node, node_p, 1-dp);
        connect(node_g, node, dg);
    }

    /** @brief Conecta `child` como hijo `d` de `parent` y actualiza la altura de `parent`. */
    void connect(NodePtr parent, NodePtr child, TX d) {
        Parent::connect(parent, child, d);
        if (parent) parent->m_height = calcHeight(parent);
    }
};

#endif // "__AVLTREE_H__"