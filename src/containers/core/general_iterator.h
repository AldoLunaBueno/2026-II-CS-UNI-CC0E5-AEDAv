/**
 * @file general_iterator.h
 * @ingroup core
 * @brief Clase base CRTP para iteradores hacia adelante y hacia atrás de los contenedores.
 */

#ifndef __GENERAL_ITERATOR_H__
#define __GENERAL_ITERATOR_H__

#include <iostream>
#include <stdexcept>
#include <algorithm> // para std::swap

/**
 * @class GeneralIterator
 * @brief Plantilla base CRTP que implementa la interfaz común de iteradores de nodo.
 *
 * @tparam Derived Tipo derivado concreto que define `operator++`.
 * @tparam T Tipo de nodo apuntado por el iterador.
 */
template <typename Derived, typename T>
class GeneralIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type *;
    using reference         = value_type&;
protected:
    pointer m_ptr;    /**< Puntero al nodo actual. */
    pointer m_pStart; /**< Puntero al nodo inicial del recorrido. */
public:
    /**
     * @brief Construye un iterador posicionado en `ptr`.
     * @param ptr Puntero inicial al nodo.
     */
    GeneralIterator(pointer ptr) : m_ptr(ptr), m_pStart(ptr) {}

    /** @brief Desreferencia el iterador devolviendo el nodo actual. */
    value_type& operator*() const { return *m_ptr; }
    // value_type* operator->()      { return m_ptr; }

    /** @brief Compara dos iteradores por igualdad de su puntero actual. */
    friend bool operator== (const GeneralIterator& a, const GeneralIterator& b) {
        return a.m_ptr == b.m_ptr;
    }

    /** @brief Compara dos iteradores por desigualdad de su puntero actual. */
    friend bool operator!= (const GeneralIterator& a, const GeneralIterator& b) { 
        return a.m_ptr != b.m_ptr;
    }
};

#endif // __GENERAL_ITERATOR_H__