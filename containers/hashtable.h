#include "avltree.h"
#include "binarytree.h"
#include "mixins.h"
#include "GeneralContainer.h"

template <typename _Key, typename _Value>
class HashEntry {
public:
    using MySelf    = HashEntry;
    using Key       = _Key;
    using Value     = _Value;
protected:
    TX m_hash = 0;
    Key m_key;
    Value m_value;
public:
    Key&    key()   { return m_key; }
    Value&  value() { return m_value; }


    HashEntry(const Key& k, const Value& v) : m_key(k), m_value(v) {}

    bool operator<(const MySelf& another) const {
        return this->m_key < another.m_key;
    }

    TX hash(Key k) {
        return k.hash();
    }
};


template <typename Derived>
class Hashable {
};

template <typename _Key, typename _Value>
struct HashTableTraits : public AscendingTraits<HashEntry<_Key, _Value>>{
    using Key               = _Key;
    using Value             = _Value;
    using value_type        = HashEntry<Key, Value>;
    using Node              = BinaryTreeNode<value_type>;
    using ForwardIterator   = BinaryTreeIterator<value_type, Order::InOrder, Direction::Asc>;
    using BackwardIterator   = BinaryTreeIterator<value_type, Order::InOrder, Direction::Asc>;
};

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable;

template <typename Traits>
using HashTable = BaseHashTable<Traits, Callable, Printable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseHashTable : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf    = BaseHashTable;
    using Parent    = GeneralContainer<Traits, Mixins...>;
    using Pair      = Traits::value_type; // HashEntry<Key, Value>
    using Key       = Pair::Key;
    using Value     = Pair::Value;
    using typename Parent::ForwardIterator;
    using typename Parent::Node;
protected:
    using Parent::Parent;
    using Parent::m_mutex;
    BaseAVLTree<Traits, Mixins...> m_data;
public:
    Value& operator[](const Key& k) {
        Pair target(k, Value{});
        Pair* pair_found = m_data.find(target);
        if (pair_found) {
            return pair_found->value();
        } else {
            m_data.insert(target, 0);
            Pair* pair_inserted = m_data.find(target); 
            return pair_inserted->value();
        }
    }

    
    void clear() override {
        std::lock_guard lock(m_mutex);
        m_data.clear();
    }
    
    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }

private:
    void push_back(const Pair &value, Ref ref) override {};
};