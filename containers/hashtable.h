#include "avltree.h"
#include "mixins.h"
#include "GeneralContainer.h"

template <typename T>
class HashTableNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // constructor heredado
    NodePtr m_pChild[2] = {nullptr, nullptr}; // hijo izquierdo y derecho
    NodePtr m_parent = nullptr; // padre
    TX m_height = 0;
};



template <typename Key, typename Value>
class HashEntry {
    using MySelf = HashEntry;
protected:
    TX m_hash = 0;
    Key m_key;
    Value m_value;
public:
    Key& key()      { return m_key; }
    Value& value()  { return m_value; }


    HashEntry(Key k, Key v) : m_key(k), m_value(k) {}

    bool operator<(MySelf another) {
        if (this.m_key < another.m_key) {
            return true;
        }
    }

    TX hash(Key k) {
        return k.hash();
    }
};

template <typename Derived>
class Hashable {
};

template <typename Key, typename Value>
using HashTableTraits = AscendingTraits<HashEntry<Key, Value>>;

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable : public GeneralContainer<Traits, Mixins...> {
    using MySelf = BaseHashTable;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using Pair = Parent::value_type;
    using Key = Pair::Key;
    using Value = Pair::Value;

protected:
    using Parent::Parent;
    using Parent::m_mutex;
    AVLTree<Pair> m_data;
public:
    Value& operator[](Key k) {
        HashEntry<Key, Value> target(k, Value{});
        Pair pair_found = m_data.find(target);
        pair_found.

        return v;
    }
};