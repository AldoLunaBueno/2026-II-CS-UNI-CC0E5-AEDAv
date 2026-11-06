#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <iostream>
#include "../types.h" // Ref

template <typename T>
struct GeneralNode{
protected:
    T   m_value;
    Ref m_ref;      // Reference to the value

public:
    using value_type = T;
    GeneralNode() = default; // requerido por resize(): new Node[new_cap]
    GeneralNode(const T& value, Ref ref) : m_value(value), m_ref(ref) {}
    T    getValue() const { return m_value; }
    Ref  getRef()   const { return m_ref;   }
    T&   value()          { return m_value; } // acceso mutable
    Ref& ref()            { return m_ref;   } // acceso mutable

    using Delim = std::istream::char_type;

    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T> &node) {
        os << "(" << node.getValue() << "," << node.getRef() << ")";
        return os;
    }

    friend std::istream &operator >>(std::istream &is, GeneralNode<T> &node){
        Delim d;
        is >> d >> node.m_value >> d >> node.m_ref >> d;
        return is;
    }
};

#endif // __GENERAL_NODE_H__