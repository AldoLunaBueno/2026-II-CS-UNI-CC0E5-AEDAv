/**
 * @file merkle_tree.h
 * @ingroup trees
 * @brief Árbol de Merkle (`BaseMerkleTree` / `MerkleTree`) con separación de dominios y pruebas de auditoría.
 */

#ifndef __MERKLETREE_H__
#define __MERKLETREE_H__

#include "binary_tree.h"
#include "../vector.h"
#include "../core/mixins.h"
#include "../core/general_container.h"
#include "../../types.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <functional>
#include <stdexcept>
#include <iostream>

/**
 * @struct MerkleProofStep
 * @brief Representa un paso en la ruta de autenticación (Merkle Proof).
 * @tparam HashType Tipo del resumen hash (por defecto `std::string`).
 */
template <typename HashType = std::string>
struct MerkleProofStep {
    HashType sibling_hash;       /**< Hash del nodo hermano necesario para reconstruir el padre. */
    bool is_left_sibling = false; /**< `true` si el hermano debe concatenarse a la izquierda. */

    /** @brief Imprime el paso de prueba indicando lado `[L: ...]` o `[R: ...]`. */
    friend std::ostream& operator<<(std::ostream& os, const MerkleProofStep& step) {
        os << (step.is_left_sibling ? "[L: " : "[R: ") << step.sibling_hash << "]";
        return os;
    }
};

/**
 * @struct DefaultMerkleHasher
 * @brief Hasher por defecto con separación de dominios (RFC 6962).
 *
 * - `"leaf:"` previene ataques de segunda preimagen entre hojas y nodos internos.
 * - `"node:"` combina los hashes de dos nodos hijos.
 */
struct DefaultMerkleHasher {
    /** @brief Calcula el hash hexadecimal de una hoja con prefijo `"leaf:"`. */
    std::string hash_leaf(const std::string& data) const {
        std::hash<std::string> hasher;
        size_t h = hasher("leaf:" + data);
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << h;
        return ss.str();
    }

    /** @brief Combina dos hashes hijos con prefijo `"node:"`. */
    std::string hash_nodes(const std::string& left, const std::string& right) const {
        std::hash<std::string> hasher;
        size_t h = hasher("node:" + left + ":" + right);
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << h;
        return ss.str();
    }
};

/** @brief Rasgos por defecto para `BaseMerkleTree`. */
template <typename HashType = std::string, typename Hasher = DefaultMerkleHasher>
struct MerkleTreeTraits {
    using value_type       = HashType;
    using HasherType       = Hasher;
    using Node             = BinaryTreeNode<HashType>;
    using ForwardIterator  = BinaryTreeIterator<HashType, Order::InOrder, Direction::Asc>;
    using BackwardIterator = BinaryTreeIterator<HashType, Order::InOrder, Direction::Desc>;
    using Compare          = std::less<HashType>;
};

template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleTree;

/** @brief Alias estándar de árbol de Merkle con soporte completo de mixins y visualización. */
template <typename Traits = MerkleTreeTraits<std::string>>
using MerkleTree = BaseMerkleTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseMerkleTree
 * @brief Árbol binario de hashes criptográficos que permite verificar integridad y pertenencia en O(log n).
 *
 * @image html merkle_tree_proof.svg "Árbol Merkle y prueba de auditoría"
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleTree : public BaseBinaryTree<Traits, Mixins...> {
public:
    using MySelf            = BaseMerkleTree;
    using Parent            = BaseBinaryTree<Traits, Mixins...>;
    using typename Parent::value_type; // HashType (ej: std::string)
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using Hasher            = typename Traits::HasherType;
    using ProofStep         = MerkleProofStep<value_type>;
    using Proof             = BaseVector<VectorAscTraits<ProofStep>>;
    using RawDataContainer  = BaseVector<VectorAscTraits<std::string>>;
    using LeafNodeContainer = BaseVector<VectorAscTraits<NodePtr>>;
    using typename Parent::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;

protected:
    using Parent::m_pRoot;
    using Parent::m_mutex;

    RawDataContainer  m_raw_data;
    LeafNodeContainer m_leaf_nodes;
    Hasher            m_hasher;

public:
    BaseMerkleTree() = default;

    BaseMerkleTree(std::initializer_list<std::pair<std::string, Ref>> items) {
        for (const auto& item : items) {
            push_back(item.first, item.second);
        }
    }

    BaseMerkleTree(const BaseMerkleTree& other) { *this = other; }
    BaseMerkleTree(BaseMerkleTree&& other) noexcept { *this = std::move(other); }

    virtual ~BaseMerkleTree() { clear(); }

    BaseMerkleTree& operator=(const BaseMerkleTree& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        m_raw_data = other.m_raw_data;
        rebuild();
        return *this;
    }

    BaseMerkleTree& operator=(BaseMerkleTree&& other) noexcept {
        if (this == &other) return *this;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        m_raw_data   = std::move(other.m_raw_data);
        m_leaf_nodes = std::move(other.m_leaf_nodes);
        m_pRoot      = std::exchange(other.m_pRoot, nullptr);
        return *this;
    }

    void clear() override {
        std::lock_guard lock(m_mutex);
        Parent::clear();
        m_leaf_nodes.clear();
        m_raw_data.clear();
    }

    /** @brief Agrega un nuevo dato crudo al árbol y reconstruye la estructura. */
    void push_back(const value_type& value, Ref ref = 0) override {
        std::lock_guard lock(m_mutex);
        m_raw_data.push_back(value, ref);
        rebuild();
    }

    /** @brief Alias semántico de `push_back()` para agregar bloques de datos. */
    void add_data(const std::string& data, Ref ref = 0) {
        push_back(data, ref);
    }

    /** @brief Retorna la cantidad de hojas (datos crudos) en el árbol. */
    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_raw_data.size();
    }

    /** @brief Indica si el árbol no contiene datos crudos. */
    bool empty() const override {
        std::lock_guard lock(m_mutex);
        return m_raw_data.empty();
    }

    /** @brief Reconstruye bottom-up todos los niveles del árbol de Merkle a partir de `m_raw_data`. */
    void rebuild() override {
        std::lock_guard lock(m_mutex);

        // 1. Limpiar los nodos del árbol actual conservando los datos crudos
        Parent::clear();
        m_leaf_nodes.clear();

        if (m_raw_data.empty()) return;

        // 2. Construir el nivel de hojas (Nivel 0)
        LeafNodeContainer current_level;
        current_level.reserve(m_raw_data.size());
        m_leaf_nodes.reserve(m_raw_data.size());

        for (size_t i = 0; i < m_raw_data.size(); ++i) {
            value_type leaf_hash = m_hasher.hash_leaf(m_raw_data[i].getValue());
            NodePtr leaf = new Node(leaf_hash, m_raw_data[i].getRef());
            current_level.push_back(leaf);
            m_leaf_nodes.push_back(leaf);
        }

        // Caso base: un solo elemento
        if (current_level.size() == 1) {
            m_pRoot = current_level[0].getValue();
            m_pRoot->m_parent = nullptr;
            return;
        }

        // 3. Reducción bottom-up por niveles hasta alcanzar la raíz
        while (current_level.size() > 1) {
            LeafNodeContainer next_level;
            size_t n = current_level.size();
            next_level.reserve((n + 1) / 2);

            for (size_t i = 0; i < n; i += 2) {
                NodePtr left = current_level[i].getValue();
                NodePtr right = nullptr;

                if (i + 1 < n) {
                    right = current_level[i + 1].getValue();
                } else {
                    // Si el nivel tiene cantidad impar de nodos, se clona el último
                    // (regla clásica de Bitcoin). La asignación con 'new' previene
                    // doble liberación (double free) al llamar a clear().
                    right = new Node(left->getValue(), left->getRef());
                }

                value_type parent_hash = m_hasher.hash_nodes(left->getValue(), right->getValue());
                NodePtr parent = new Node(parent_hash, -1);

                this->connect(parent, left, 0);
                this->connect(parent, right, 1);

                next_level.push_back(parent);
            }
            current_level = std::move(next_level);
        }

        m_pRoot = current_level[0].getValue();
        m_pRoot->m_parent = nullptr;
    }

    /** @brief Retorna el hash de la raíz del árbol (Merkle Root), o cadena vacía si está vacío. */
    value_type getRootHash() const {
        std::lock_guard lock(m_mutex);
        return m_pRoot ? m_pRoot->getValue() : value_type{};
    }

    /** @brief Retorna el dato crudo en la posición `index`. */
    std::string getData(size_t index) const {
        std::lock_guard lock(m_mutex);
        if (index >= m_raw_data.size()) {
            throw std::out_of_range("MerkleTree::getData: indice fuera de rango");
        }
        return m_raw_data[index].getValue();
    }

    /** @brief Retorna el vector completo de datos crudos almacenados. */
    const RawDataContainer& getRawData() const {
        std::lock_guard lock(m_mutex);
        return m_raw_data;
    }

    /** @brief Retorna los punteros a los nodos hoja en orden de inserción. */
    const LeafNodeContainer& getLeafNodes() const {
        std::lock_guard lock(m_mutex);
        return m_leaf_nodes;
    }

    /** @brief Retorna el hash almacenado en la hoja `index`. */
    value_type getLeafHash(size_t index) const {
        std::lock_guard lock(m_mutex);
        if (index >= m_leaf_nodes.size()) {
            throw std::out_of_range("MerkleTree::getLeafHash: indice fuera de rango");
        }
        return m_leaf_nodes[index].getValue()->getValue();
    }

    /** @brief Actualiza el dato crudo en `index` y reconstruye el árbol. */
    void update(size_t index, const std::string& new_data) {
        std::lock_guard lock(m_mutex);
        if (index >= m_raw_data.size()) {
            throw std::out_of_range("MerkleTree::update: indice fuera de rango");
        }
        m_raw_data[index].value() = new_data;
        rebuild();
    }

    /**
     * @brief Genera la ruta de auditoría (Merkle Proof) para verificar la hoja `index`.
     * @param index Índice de la hoja en `m_leaf_nodes`.
     * @return Vector de `ProofStep` desde la hoja hasta la raíz.
     */
    Proof getProof(size_t index) const {
        std::lock_guard lock(m_mutex);
        Proof proof;
        if (index >= m_leaf_nodes.size()) {
            return proof;
        }

        NodePtr curr = m_leaf_nodes[index].getValue();
        while (curr && curr->m_parent) {
            NodePtr parent = curr->m_parent;
            bool is_left_child = (parent->m_pChild[0] == curr);
            NodePtr sibling = parent->m_pChild[is_left_child ? 1 : 0];

            if (sibling) {
                proof.push_back(ProofStep{
                    sibling->getValue(),
                    !is_left_child // true si el hermano es el hijo izquierdo
                });
            }
            curr = parent;
        }
        return proof;
    }

    /**
     * @brief Verifica estáticamente si `raw_data` pertenece al árbol con raíz `expected_root`.
     */
    static bool verify(const std::string& raw_data, const Proof& proof, const value_type& expected_root) {
        Hasher hasher;
        value_type curr_hash = hasher.hash_leaf(raw_data);

        for (const auto& node : proof) {
            const auto& step = node.getValue();
            if (step.is_left_sibling) {
                curr_hash = hasher.hash_nodes(step.sibling_hash, curr_hash);
            } else {
                curr_hash = hasher.hash_nodes(curr_hash, step.sibling_hash);
            }
        }
        return curr_hash == expected_root;
    }

    /**
     * @brief Verifica estáticamente una prueba de inclusión a partir del hash de la hoja.
     */
    static bool verify_hash(const value_type& leaf_hash, const Proof& proof, const value_type& expected_root) {
        Hasher hasher;
        value_type curr_hash = leaf_hash;

        for (const auto& node : proof) {
            const auto& step = node.getValue();
            if (step.is_left_sibling) {
                curr_hash = hasher.hash_nodes(step.sibling_hash, curr_hash);
            } else {
                curr_hash = hasher.hash_nodes(curr_hash, step.sibling_hash);
            }
        }
        return curr_hash == expected_root;
    }

    /** @brief Serializa los datos crudos del árbol `[("dato",ref),...]` para persistencia exacta. */
    friend std::ostream& operator<<(std::ostream& os, const BaseMerkleTree& tree) {
        std::lock_guard lock(tree.m_mutex);
        os << "[";
        for (size_t i = 0; i < tree.m_raw_data.size(); ++i) {
            if (i > 0) os << ",";
            os << "(" << std::quoted(tree.m_raw_data[i].getValue()) << "," << tree.m_raw_data[i].getRef() << ")";
        }
        return os << "]";
    }

    /** @brief Deserializa `[("dato",ref),...]` y reconstruye el árbol de Merkle en una sola pasada. */
    friend std::istream& operator>>(std::istream& is, BaseMerkleTree& tree) {
        std::lock_guard lock(tree.m_mutex);
        tree.clear();
        char ch = 0;
        if (!(is >> ch) || ch != '[') {
            is.setstate(std::ios::failbit);
            return is;
        }
        if (is >> ch && ch == ']') {
            return is;
        }
        is.unget();

        while (true) {
            char open_p = 0, comma = 0, close_p = 0, sep = 0;
            std::string data;
            Ref ref = 0;
            if (!(is >> open_p) || open_p != '(' ||
                !(is >> std::quoted(data)) ||
                !(is >> comma) || comma != ',' ||
                !(is >> ref) ||
                !(is >> close_p) || close_p != ')') {
                is.setstate(std::ios::failbit);
                return is;
            }
            tree.m_raw_data.push_back(std::move(data), ref);
            if (!(is >> sep) || sep == ']') break;
            if (sep != ',') {
                is.setstate(std::ios::failbit);
                return is;
            }
        }
        tree.rebuild();
        return is;
    }
};

#endif // __MERKLETREE_H__
