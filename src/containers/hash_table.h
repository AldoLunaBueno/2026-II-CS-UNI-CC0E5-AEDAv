/**
 * @file hash_table.h
 * @ingroup containers
 * @brief Diccionario asociativo (`BaseHashTable` / `HashTable`) respaldado por un `BaseAVLTree`.
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "trees/avl_tree.h"
#include "trees/binary_tree.h"
#include "core/mixins.h"
#include "core/general_container.h"
#include <iomanip>
#include <string>
#include <type_traits>

/**
 * @class HashEntry
 * @brief Par clave-valor ordenado por clave para almacenamiento en `BaseHashTable`.
 *
 * @tparam _Key Tipo de la clave.
 * @tparam _Value Tipo del valor asociado.
 */
template <typename _Key, typename _Value>
class HashEntry {
public:
    using MySelf    = HashEntry;
    using Key       = _Key;
    using Value     = _Value;
    using Delim     = std::istream::char_type;
protected:
    TX m_hash = 0; /**< Valor hash reservado. */
    Key m_key;     /**< Clave de búsqueda y ordenamiento. */
    Value m_value; /**< Valor asociado a la clave. */
public:
    /** @brief Acceso mutable a la clave. */
    Key&          key()            { return m_key; }
    /** @brief Acceso mutable al valor. */
    Value&        value()          { return m_value; }
    /** @brief Acceso de solo lectura a la clave. */
    const Key&    getKey()   const { return m_key; }
    /** @brief Acceso de solo lectura al valor. */
    const Value&  getValue() const { return m_value; }

    HashEntry() = default;
    /** @brief Construye una entrada con clave `k` y valor opcional `v`. */
    HashEntry(const Key& k, const Value& v = Value{}) : m_key(k), m_value(v) {}

    /** @brief Comparación de tres vías basada únicamente en `m_key`. */
    auto operator<=>(const MySelf& another) const {
        return m_key <=> another.m_key;
    }

private:
    template <typename T>
    static decltype(auto) elem(T& val) {
        if constexpr (std::is_same_v<std::remove_const_t<T>, std::string>) return std::quoted(val);
        else return val;
    }

public:
    /** @brief Serializa la entrada con formato `(clave:valor)`. */
    friend std::ostream& operator<<(std::ostream& os, const HashEntry& entry) {
        return os << "(" << elem(entry.m_key) << ":" << elem(entry.m_value) << ")";
    }

    /** @brief Deserializa una entrada con formato `(clave:valor)`. */
    friend std::istream& operator>>(std::istream& is, HashEntry& entry) {
        Delim open_d, sep, close_d;
        return is >> open_d >> elem(entry.m_key) >> sep >> elem(entry.m_value) >> close_d;
    }
};

/**
 * @struct HashTableTraits
 * @brief Rasgos de `BaseHashTable` que definen el nodo binario e iteradores InOrder.
 */
template <typename _Key, typename _Value>
struct HashTableTraits : public AscendingTraits<HashEntry<_Key, _Value>>{
    using Key               = _Key;
    using Value             = _Value;
    using value_type        = HashEntry<Key, Value>;
    using Node              = BinaryTreeNode<value_type>;
    using ForwardIterator   = BinaryTreeIterator<value_type, Order::InOrder, Direction::Asc>;
    using BackwardIterator  = BinaryTreeIterator<value_type, Order::InOrder, Direction::Desc>;
};

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable;

/** @brief Alias estándar de tabla asociativa con mixins de consulta y serialización. */
template <typename Traits>
using HashTable = BaseHashTable<Traits, Callable, Printable, Readable>;

/**
 * @class BaseHashTable
 * @brief Contenedor asociativo clave-valor respaldado internamente por un árbol AVL (`BaseAVLTree`).
 *
 * @tparam Traits Rasgos de la tabla (`HashTableTraits`).
 * @tparam Mixins Lista de mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf    = BaseHashTable;
    using Parent    = GeneralContainer<Traits, Mixins...>;
    using Pair      = typename Parent::value_type; // HashEntry<Key, Value>
    using Key       = typename Pair::Key;
    using Value     = typename Pair::Value;
    using typename Parent::ForwardIterator;
    using typename Parent::Node;
protected:
    using Parent::Parent;
    using Parent::m_mutex;
    BaseAVLTree<Traits, Mixins...> m_data; /**< Árbol AVL interno que indexa las entradas por clave. */
public:
    /**
     * @brief Accede o inserta por defecto el valor asociado a `key`.
     * @param key Clave a buscar o insertar.
     * @return Referencia mutable al valor asociado.
     */
    Value& operator[](const Key& key);

    /**
     * @brief Verifica si `key` existe en la tabla.
     * @param key Clave a consultar.
     * @return `true` si la clave está presente, `false` en caso contrario.
     */
    bool contains(const Key& key);

    /** @brief Elimina todas las entradas de la tabla. */
    void clear() override;
    
    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }

    /** @brief Inserta o actualiza una entrada `(clave, valor)` en la tabla. */
    void push_back(const Pair& pair, Ref ref = 0) override;
};

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::operator[](const Key& key) -> Value& {
    std::lock_guard lock(m_mutex);
    return m_data.find_or_insert(Pair(key, Value{}), 0)->value();
}

template <typename Traits, template <typename> typename... Mixins>
bool BaseHashTable<Traits, Mixins...>::contains(const Key& key) {
    std::lock_guard lock(m_mutex);
    Pair target(key, Value{});
    Pair* pair_found = m_data.find(target);
    return pair_found != nullptr;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHashTable<Traits, Mixins...>::clear() {
    std::lock_guard lock(m_mutex);
    m_data.clear();
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHashTable<Traits, Mixins...>::push_back(const Pair& pair, Ref ref) {
    std::lock_guard lock(m_mutex);
    Pair* pair_found = m_data.find(pair);
    if (pair_found) {
        pair_found->value() = pair.getValue();
    } else {
        m_data.insert(pair, ref);
    }
}

#endif // __HASHTABLE_H__