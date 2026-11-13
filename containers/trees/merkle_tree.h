#ifndef __MERKLETREE_H__
#define __MERKLETREE_H__

#include "binary_tree.h"
#include "../core/mixins.h"
#include "../core/general_container.h"
#include "../../types.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <functional>
#include <stdexcept>
#include <iostream>

// Estructura que representa un paso en la ruta de autenticación (Merkle Proof)
template <typename HashType = std::string>
struct MerkleProofStep {
    HashType sibling_hash;
    bool is_left_sibling = false; // true si el hermano debe concatenarse a la izquierda

    friend std::ostream& operator<<(std::ostream& os, const MerkleProofStep& step) {
        os << (step.is_left_sibling ? "[L: " : "[R: ") << step.sibling_hash << "]";
        return os;
    }
};

// Hasher por defecto con separación de dominios (RFC 6962):
// - "leaf:" previene ataques de segunda preimagen entre hojas y nodos internos.
// - "node:" se usa para combinar los hashes de dos hijos.
struct DefaultMerkleHasher {
    std::string hash_leaf(const std::string& data) const {
        std::hash<std::string> hasher;
        size_t h = hasher("leaf:" + data);
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << h;
        return ss.str();
    }

    std::string hash_nodes(const std::string& left, const std::string& right) const {
        std::hash<std::string> hasher;
        size_t h = hasher("node:" + left + ":" + right);
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << h;
        return ss.str();
    }
};

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

template <typename Traits = MerkleTreeTraits<std::string>>
using MerkleTree = BaseMerkleTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

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
    using Proof             = std::vector<ProofStep>;
    using typename Parent::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;

protected:
    using Parent::m_pRoot;
    using Parent::m_mutex;

    std::vector<std::string> m_raw_data;
    std::vector<Ref>         m_raw_refs;
    std::vector<NodePtr>     m_leaf_nodes;
    Hasher                   m_hasher;

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
        m_raw_refs = other.m_raw_refs;
        rebuild();
        return *this;
    }

    BaseMerkleTree& operator=(BaseMerkleTree&& other) noexcept {
        if (this == &other) return *this;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        m_raw_data   = std::move(other.m_raw_data);
        m_raw_refs   = std::move(other.m_raw_refs);
        m_leaf_nodes = std::move(other.m_leaf_nodes);
        m_pRoot      = std::exchange(other.m_pRoot, nullptr);
        return *this;
    }

    void clear() override {
        std::lock_guard lock(m_mutex);
        Parent::clear();
        m_leaf_nodes.clear();
        m_raw_data.clear();
        m_raw_refs.clear();
    }

    void push_back(const value_type& value, Ref ref = 0) override {
        std::lock_guard lock(m_mutex);
        m_raw_data.push_back(value);
        m_raw_refs.push_back(ref);
        rebuild();
    }

    void add_data(const std::string& data, Ref ref = 0) {
        push_back(data, ref);
    }

    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_raw_data.size();
    }

    bool empty() const override {
        std::lock_guard lock(m_mutex);
        return m_raw_data.empty();
    }

    void rebuild() override {
        std::lock_guard lock(m_mutex);

        // 1. Limpiar los nodos del árbol actual conservando los datos crudos
        Parent::clear();
        m_leaf_nodes.clear();

        if (m_raw_data.empty()) return;

        // 2. Construir el nivel de hojas (Nivel 0)
        std::vector<NodePtr> current_level;
        current_level.reserve(m_raw_data.size());
        m_leaf_nodes.reserve(m_raw_data.size());

        for (size_t i = 0; i < m_raw_data.size(); ++i) {
            value_type leaf_hash = m_hasher.hash_leaf(m_raw_data[i]);
            NodePtr leaf = new Node(leaf_hash, m_raw_refs[i]);
            current_level.push_back(leaf);
            m_leaf_nodes.push_back(leaf);
        }

        // Caso base: un solo elemento
        if (current_level.size() == 1) {
            m_pRoot = current_level[0];
            m_pRoot->m_parent = nullptr;
            return;
        }

        // 3. Reducción bottom-up por niveles hasta alcanzar la raíz
        while (current_level.size() > 1) {
            std::vector<NodePtr> next_level;
            size_t n = current_level.size();
            next_level.reserve((n + 1) / 2);

            for (size_t i = 0; i < n; i += 2) {
                NodePtr left = current_level[i];
                NodePtr right = nullptr;

                if (i + 1 < n) {
                    right = current_level[i + 1];
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

        m_pRoot = current_level[0];
        m_pRoot->m_parent = nullptr;
    }

    value_type getRootHash() const {
        std::lock_guard lock(m_mutex);
        return m_pRoot ? m_pRoot->getValue() : value_type{};
    }

    const std::string& getData(size_t index) const {
        std::lock_guard lock(m_mutex);
        if (index >= m_raw_data.size()) {
            throw std::out_of_range("MerkleTree::getData: indice fuera de rango");
        }
        return m_raw_data[index];
    }

    const std::vector<std::string>& getRawData() const {
        std::lock_guard lock(m_mutex);
        return m_raw_data;
    }

    const std::vector<NodePtr>& getLeafNodes() const {
        std::lock_guard lock(m_mutex);
        return m_leaf_nodes;
    }

    value_type getLeafHash(size_t index) const {
        std::lock_guard lock(m_mutex);
        if (index >= m_leaf_nodes.size()) {
            throw std::out_of_range("MerkleTree::getLeafHash: indice fuera de rango");
        }
        return m_leaf_nodes[index]->getValue();
    }

    void update(size_t index, const std::string& new_data) {
        std::lock_guard lock(m_mutex);
        if (index >= m_raw_data.size()) {
            throw std::out_of_range("MerkleTree::update: indice fuera de rango");
        }
        m_raw_data[index] = new_data;
        rebuild();
    }

    // Generación de prueba de inclusión (Merkle Proof) para la hoja en 'index'
    Proof getProof(size_t index) const {
        std::lock_guard lock(m_mutex);
        Proof proof;
        if (index >= m_leaf_nodes.size()) {
            return proof;
        }

        NodePtr curr = m_leaf_nodes[index];
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

    // Verificación estática e independiente a partir del dato original
    static bool verify(const std::string& raw_data, const Proof& proof, const value_type& expected_root) {
        Hasher hasher;
        value_type curr_hash = hasher.hash_leaf(raw_data);

        for (const auto& step : proof) {
            if (step.is_left_sibling) {
                curr_hash = hasher.hash_nodes(step.sibling_hash, curr_hash);
            } else {
                curr_hash = hasher.hash_nodes(curr_hash, step.sibling_hash);
            }
        }
        return curr_hash == expected_root;
    }

    // Verificación estática a partir del hash de la hoja
    static bool verify_hash(const value_type& leaf_hash, const Proof& proof, const value_type& expected_root) {
        Hasher hasher;
        value_type curr_hash = leaf_hash;

        for (const auto& step : proof) {
            if (step.is_left_sibling) {
                curr_hash = hasher.hash_nodes(step.sibling_hash, curr_hash);
            } else {
                curr_hash = hasher.hash_nodes(curr_hash, step.sibling_hash);
            }
        }
        return curr_hash == expected_root;
    }
};

#endif // __MERKLETREE_H__
