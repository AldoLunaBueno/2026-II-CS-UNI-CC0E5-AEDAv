#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <iostream>
#include "../types.h" // Ref
template <typename T>
struct GeneralNode{
private:
    T   m_value;
    Ref m_ref;      // Reference to the value

public:
    GeneralNode() = default; // requerido por resize(): new Node[new_cap]
    GeneralNode(const T& value, Ref ref) : m_value(value), m_ref(ref) {}
    T    getValue() const { return m_value; }
    Ref  getRef()   const { return m_ref;   }
    T&   value()          { return m_value; } // acceso mutable para ApplyFunction

    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T> &node) {
        os << "(" << node.m_value << "," << node.m_ref << ")";
        return os;
    }

    // DONE
    // TODO: El operator<< deberia ir en GeneralNode, no en LinkedListNode, para que sea generico y reusable.
    // Cambié los métodos getter por simples referencias a las variables
    // porque esto es más coherente en funciones con especificador friend
    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T> &node) {
        os << "(" << node.m_value << "," << node.m_ref << ")";
        return os;
    }
};

#endif // __GENERAL_NODE_H__