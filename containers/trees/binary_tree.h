/**
 * @file binary_tree.h
 * @ingroup trees
 * @brief Árbol binario de búsqueda (`BaseBinaryTree` / `BinaryTree`) e iterador unificado por recorrido de Euler.
 */

#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "../core/general_node.h"
#include "../core/general_iterator.h"
#include "../core/general_container.h"
#include "../core/mixins.h"
#include "../../types.h"
#include "../../utilities/render.h"
#include <iostream>

/**
 * @class BinaryTreeNode
 * @brief Nodo de árbol binario con punteros a hijos izquierdo/derecho, padre y altura.
 * @tparam T Tipo del valor almacenado.
 */
template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // constructor heredado
    NodePtr m_pChild[2] = {nullptr, nullptr}; /**< Hijos: índice 0 izquierdo, índice 1 derecho. */
    NodePtr m_parent = nullptr;               /**< Puntero al nodo padre (`nullptr` en la raíz). */
    TX m_height = 0;                          /**< Altura del subárbol (usada en `BaseAVLTree`). */
};

namespace detail {
    /**
     * @enum Phase
     * @brief Máscara de bits para las fases del recorrido de Euler.
     *
     * @image html euler_bitmask.svg "Puente entre fases y máscara binaria"
     */
    enum Phase : TX {
        Pre  = 0b100, /**< Visita al descender por primera vez al nodo. */
        In   = 0b010, /**< Visita entre el subárbol izquierdo y el derecho. */
        Post = 0b001  /**< Visita al ascender tras procesar ambos subárboles. */
    };

    /**
     * @enum Order
     * @brief Combinaciones puras e híbridas de órdenes de recorrido (7 órdenes × 2 direcciones = 14 recorridos).
     *
     * @image html euler_venn.svg "Diagrama de Venn de los 14 recorridos posibles"
     */
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

    /** @brief Dirección del recorrido: `Asc` (izq→der) o `Desc` (der→izq). */
    enum Direction : TX {Asc = 0, Desc = 1 };
}

using detail::Phase;
using detail::Order;
using detail::Direction;

/**
 * @class BinaryTreeIterator
 * @brief Iterador unificado mediante máquina de estados sobre el contorno de Euler.
 *
 * @tparam T Tipo de dato del nodo.
 * @tparam Ord Máscara `Order` que selecciona las fases emitidas.
 * @tparam Dir Dirección del recorrido (`Direction::Asc` o `Direction::Desc`).
 *
 * @image html euler_contour.svg "Contorno del recorrido de Euler sobre el árbol binario"
 */
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
    TX m_phase; /**< Fase actual de la máquina de estados en el nodo `m_ptr`. */
public:
    /** @brief Inicializa el iterador en la raíz avanzando hasta la primera fase incluida en `Ord`. */
    BinaryTreeIterator(NodePtr root) : Parent(root) {
        m_phase = FirstPhase;
        if (m_ptr && !(MySelf::order & FirstPhase)) {
            step_until_phase();
        }
    }
    
    /** @brief Avanza al siguiente nodo/fase válido según `Ord`. */
    MySelf &operator++();

    friend bool operator== (const MySelf& a, const MySelf& b) {
        return a.m_ptr == b.m_ptr && a.m_phase == b.m_phase;
    }
    friend bool operator!= (const MySelf& a, const MySelf& b) {
        return a.m_ptr != b.m_ptr || a.m_phase != b.m_phase;
    }

    /**
     * @brief Avanza la máquina de estados del recorrido de Euler hasta la próxima fase activa en `Ord`.
     *
     * @image html euler_state_machine.svg "Máquina de estados del recorrido de Euler"
     */
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

/**
 * @class BaseBinaryTree
 * @brief Árbol binario de búsqueda genérico y thread-safe con soporte para vistas de Euler.
 *
 * @tparam Traits Rasgos del árbol (`BinaryTreeAscTraits` o `BinaryTreeDescTraits`).
 * @tparam Mixins Mixins aplicados al contenedor.
 */
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
    NodePtr m_pRoot = nullptr; /**< Raíz del árbol binario. */
    /** @brief Inserción recursiva por cola respetando `Compare`. */
    auto internalInsert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr);
    /** @brief Elimina `node` manejando 0, 1 o 2 hijos y retorna su padre para reequilibrio. */
    auto internalRemove(NodePtr node);
    /** @brief Gancho virtual de reequilibrio tras inserción o eliminación (sobrescrito en `BaseAVLTree`). */
    virtual void rebalance(NodePtr node) {}
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    /** @brief Inserta un nuevo nodo y ejecuta `rebalance()`, retornando un puntero al valor insertado. */
    auto* insert(const value_type&, Ref);
    /** @brief Elimina el nodo apuntado y reequilibra desde su padre. */
    void remove(NodePtr);

    /** @brief Conecta `child` como hijo en la rama `d` (0: izq, 1: der) de `parent`. */
    void connect(NodePtr, NodePtr, TX);

    /** @brief Libera todos los nodos del árbol. */
    void clear() override;

    /** @brief Inserta un elemento delegando a `insert()`. */
    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    /**
     * @brief Busca un valor en el árbol según `Compare`.
     * @param value Valor a buscar.
     * @return Puntero mutable al valor encontrado, o `nullptr` si no existe.
     */
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

    /**
     * @brief Busca un valor equivalente en el árbol o lo inserta si no existe, en un solo recorrido.
     * @param value Valor a buscar o insertar.
     * @param ref Referencia asociada en caso de inserción (por defecto `0`).
     * @return Puntero mutable al valor existente o recién insertado.
     */
    value_type* find_or_insert(const value_type& value, Ref ref = 0) {
        std::lock_guard lock(this->m_mutex);
        NodePtr* curr = &m_pRoot;
        NodePtr parent = nullptr;
        while (*curr) {
            auto branch = Compare()((*curr)->getValue(), value);
            if (!branch && !Compare()(value, (*curr)->getValue())) {
                return &((*curr)->value());
            }
            parent = *curr;
            curr = &((*curr)->m_pChild[1 - branch]);
        }
        *curr = new Node(value, ref);
        (*curr)->m_parent = parent;
        NodePtr inserted = *curr;
        rebalance(inserted);
        return &(inserted->value());
    }

    /** @brief Libera recursivamente el subárbol en post-orden. */
    void clear_recursive(NodePtr);

    ForwardIterator begin() const override { return ForwardIterator(m_pRoot); }
    ForwardIterator end()   const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const { return BackwardIterator(m_pRoot); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }

    /**
     * @brief Genera una vista de solo lectura con cualquier combinación de orden y dirección de Euler.
     * @tparam Ord Orden de recorrido (`Order::PreOrder`, `Order::InOrder`, híbridos, etc.).
     * @tparam Dir Dirección (`Direction::Asc` o `Direction::Desc`).
     * @return `BaseView` sincronizada con el mutex del árbol.
     */
    template <Order Ord, Direction Dir = Direction::Asc>
    auto traverse() const {
        std::lock_guard lock(this->mutex());
        using Iter = BinaryTreeIterator<value_type, Ord, Dir>;
        using EulerView = typename Parent::Traits::template View<Node, Iter>;
        return EulerView(this->mutex(), Iter(m_pRoot), Iter(nullptr));
    }



    BaseBinaryTree& operator=(const BaseBinaryTree&);
    BaseBinaryTree& operator=(BaseBinaryTree&&) noexcept;

    /** @brief Retorna el puntero a la raíz del árbol. */
    NodePtr getRoot() const { return m_pRoot; }
    /** @brief Calcula recursivamente la altura del subárbol con raíz en `root`. */
    TX height(const NodePtr&) const;
    /** @brief Retorna la altura total del árbol. */
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