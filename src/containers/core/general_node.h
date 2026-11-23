/**
 * @file general_node.h
 * @ingroup core
 * @brief Definición del nodo base genérico `GeneralNode` para los contenedores.
 */

#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <iostream>
#include "../../types.h" // Ref

/**
 * @struct GeneralNode
 * @brief Nodo base que encapsula un valor de tipo `T` y una referencia entera `Ref`.
 *
 * @tparam T Tipo del dato almacenado en el nodo.
 */
template <typename T>
struct GeneralNode{
protected:
    T   m_value;    /**< Valor almacenado en el nodo. */
    Ref m_ref;      /**< Referencia o identificador asociado al valor. */

public:
    using value_type = T;

    /** @brief Constructor por defecto (requerido por `resize()`). */
    GeneralNode() = default;

    /**
     * @brief Construye un nodo a partir de un valor y su referencia.
     * @param value Valor a almacenar.
     * @param ref Identificador o referencia asociada.
     */
    GeneralNode(const T& value, Ref ref) : m_value(value), m_ref(ref) {}

    /** @brief Retorna una copia del valor almacenado. */
    T    getValue() const { return m_value; }
    /** @brief Retorna la referencia asociada al nodo. */
    Ref  getRef()   const { return m_ref;   }
    /** @brief Retorna una referencia mutable al valor almacenado. */
    T&   value()          { return m_value; }
    /** @brief Retorna una referencia mutable a `m_ref`. */
    Ref& ref()            { return m_ref;   }

    using Delim = std::istream::char_type;

    /** @brief Serializa el nodo con formato `(valor,ref)` en un flujo de salida. */
    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T> &node) {
        os << "(" << node.getValue() << "," << node.getRef() << ")";
        return os;
    }

    /** @brief Deserializa un nodo con formato `(valor,ref)` desde un flujo de entrada. */
    friend std::istream &operator >>(std::istream &is, GeneralNode<T> &node){
        Delim d;
        is >> d >> node.m_value >> d >> node.m_ref >> d;
        return is;
    }
};

#endif // __GENERAL_NODE_H__