#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avltree.h"
#include "binarytree.h"
#include "mixins.h"
#include "GeneralContainer.h"
#include <iomanip>
#include <string>
#include <type_traits>

template <typename _Key, typename _Value>
class HashEntry {
public:
    using MySelf    = HashEntry;
    using Key       = _Key;
    using Value     = _Value;
    using Delim     = std::istream::char_type;
protected:
    TX m_hash = 0;
    Key m_key;
    Value m_value;
public:
    Key&          key()            { return m_key; }
    Value&        value()          { return m_value; }
    const Key&    getKey()   const { return m_key; }
    const Value&  getValue() const { return m_value; }

    HashEntry() = default;
    HashEntry(const Key& k, const Value& v = Value{}) : m_key(k), m_value(v) {}

    auto operator<=>(const MySelf& another) const {
        return m_key <=> another.m_key;
    }

private:
    template <typename T>
    static void write_elem(std::ostream& os, const T& val) {
        if constexpr (std::is_same_v<T, std::string>) {
            os << std::quoted(val);
        } else {
            os << val;
        }
    }

    template <typename T>
    static void read_elem(std::istream& is, T& val) {
        if constexpr (std::is_same_v<T, std::string>) {
            is >> std::quoted(val);
        } else {
            is >> val;
        }
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const HashEntry& entry) {
        os << "(";
        write_elem(os, entry.m_key);
        os << ":";
        write_elem(os, entry.m_value);
        os << ")";
        return os;
    }

    friend std::istream& operator>>(std::istream& is, HashEntry& entry) {

        Delim open_d, sep, close_d;
        is >> open_d;
        read_elem(is, entry.m_key);
        is >> sep;
        read_elem(is, entry.m_value);
        is >> close_d;
        return is;
    }
};

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

template <typename Traits>
using HashTable = BaseHashTable<Traits, Callable, Printable, Readable>;

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
    BaseAVLTree<Traits, Mixins...> m_data;
public:
    Value& operator[](const Key& key);
    bool contains(const Key& key);
    void clear() override;
    
    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }

    void push_back(const Pair& pair, Ref ref = 0) override;
};

template <typename Traits, template <typename> typename... Mixins>
auto BaseHashTable<Traits, Mixins...>::operator[](const Key& key) -> Value& {
    std::lock_guard lock(m_mutex);
    Pair target(key, Value{});
    Pair* pair_found = m_data.find(target);
    if (pair_found) {
        return pair_found->value();
    } else {
        Pair* pair_inserted = m_data.insert(target, 0);
        return pair_inserted->value();
    }
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