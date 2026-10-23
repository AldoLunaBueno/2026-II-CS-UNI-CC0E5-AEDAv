#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__

#include <mutex>
#include "GeneralNode.h"
#include "../foreach.h"
#include "../types.h" // Ref

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type        = T;
    using Compare           = _Compare;
};
template <typename T, typename _Compare = std::less<T>>
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template <typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};


template <typename Traits>
class GeneralContainer {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using ForwardIterator   = Traits::ForwardIterator;
    using Delim             = Node::Delim;

protected:
    mutable std::mutex m_mutex; // mutex para sincronización

public:
    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end() const = 0;

    virtual bool empty() const { return begin() == end(); }
    virtual void clear() = 0;
    virtual void push_back(const value_type& value, Ref ref) = 0;

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        call(func, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args... args) {
        return call(func, std::forward<Args>(args)...);
    }

    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) {
        lock_guard<mutex> lock(m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, Node&, Args...>>)
            ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }

    // Persistencia
    friend std::ostream& operator<<(std::ostream& os, GeneralContainer<Traits>& container) {
        auto first = true;
        os << "[";
        container.ApplyFunction([&os, &first](const Node& node){
            if (!first) os << ",";
            os << node;
            first = false;
        });
        return os << "]";
    }

    friend std::istream &operator >>(std::istream &is, GeneralContainer<Traits> &container) {
        Delim d;
        Node node;
        container.clear();

        is >> d;
        if (is >> d && d != ']'){
            is.unget();
            while(is >> node >> d){
                container.push_back(node.getValue(), node.getRef());
                if (d == ']') break;
            }    
        }

        return is; 
    }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }
};

#endif // __GENERAL_CONTAINER_H__