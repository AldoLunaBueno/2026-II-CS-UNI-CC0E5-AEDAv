/**
 * @file foreach.h
 * @ingroup core
 * @brief Utilidades genéricas de iteración funcional sobre rangos y contenedores.
 */

#ifndef __FOREACH_H__
#define __FOREACH_H__
#include <utility> // std::forward
#include <type_traits> // std::is_void_v, std::invoke_result_t
#include <stdexcept> // std::runtime_error
using namespace std;

/**
 * @brief Aplica una función o predicado sobre un rango de iteradores `[begin, end)`.
 *
 * Si `func` retorna `void`, se invoca para cada elemento del rango.
 * Si `func` retorna un valor convertible a `bool`, devuelve la referencia al primer
 * elemento que satisfaga el predicado; si ninguno lo cumple, lanza `std::runtime_error`.
 *
 * @tparam Iterator Tipo de iterador del rango.
 * @tparam Func Tipo del callable a invocar.
 * @tparam Args Tipos de argumentos adicionales reenviados a `func`.
 * @param begin Iterador al inicio del rango.
 * @param end Iterador al final del rango.
 * @param func Función o predicado a aplicar sobre cada elemento.
 * @param args Argumentos adicionales reenviados mediante perfect forwarding.
 * @return `void` o referencia al primer elemento que cumple el predicado.
 * @throws std::runtime_error Si `func` es predicado y ningún elemento lo satisface.
 */
template <typename Iterator, typename Func, typename... Args>
decltype(auto) call(Iterator begin, Iterator end, Func func, Args&&... args) {
    if constexpr(is_void_v<invoke_result_t<Func, decltype(*begin), Args...>>) {
        for (auto iter = begin; iter != end; ++iter)
            func(*iter, std::forward<Args>(args)...);
    } else {
        for (auto iter = begin; iter != end; ++iter) {
            auto ret = func(*iter, std::forward<Args>(args)...);
            if(ret) return *iter;
        }
        throw std::runtime_error("call(): ningun elemento satisface el predicado");
    }
}

// Variadic template to allow passing additional arguments to the function
// template <typename Container, typename Func, typename... Args>
// Iterator Level #1
// void ApplyFunction(Container &container, Func func, Args... args) {
//     for (size_t i = 0; i < container.size(); ++i) {
//         func(container[i], args...);
//     }
// }

// Iterator Level #2
// template <typename Container, typename Func, typename... Args>
// void ApplyFunction(Container &container, Func func, Args... args) {
//     for (auto iter = container.begin(); iter != container.end(); ++iter) {
//         func(*iter, args...);
//     }
// }

/**
 * @brief Aplica `func` a cada elemento en el rango `[begin, end)`.
 * @tparam Iterator Tipo de iterador del rango.
 * @tparam Func Tipo de la función o functor.
 * @tparam Args Tipos de argumentos adicionales.
 */
template <typename Iterator, typename Func, typename... Args>
void ApplyFunction(Iterator begin, Iterator end, Func func, Args&&... args) {
    // for (auto iter = begin; iter != end; ++iter)
    //     func(*iter, std::forward<Args>(args)...);
    ::call(begin, end, func, std::forward<Args>(args)...);
}

/**
 * @brief Busca el primer elemento en `[begin, end)` que satisface el predicado `func`.
 * @tparam Iterator Tipo de iterador del rango.
 * @tparam Func Tipo del predicado.
 * @tparam Args Tipos de argumentos adicionales.
 * @return Referencia al primer elemento que satisface `func`.
 */
template <typename Iterator, typename Func, typename... Args>
decltype(auto) FirstThat(Iterator begin, Iterator end, Func func, Args&&... args) {
    // for (auto iter = begin; iter != end; ++iter)
    //     func(*iter, std::forward<Args>(args)...);
    ::call(begin, end, func, std::forward<Args>(args)...);
}

// // Iterator Level #3
// template <typename Container, typename Func, typename... Args>
// void ApplyFunction(Container &container, Func func, Args... args) {
//     ::ApplyFunction(container.begin(), container.end(), func, args...);
// }

/**
 * @brief Sobrecarga de conveniencia que aplica `func` a todos los elementos de `container`.
 * @tparam Container Tipo del contenedor iterable con range-based `for`.
 * @tparam Func Tipo de la función o functor.
 * @tparam Args Tipos de argumentos adicionales.
 */
template <typename Container, typename Func, typename... Args>
void ApplyFunction(Container &container, Func func, Args&&... args) {
    for (auto &v : container)
        func(v, std::forward<Args>(args)...);
}

#endif // __FOREACH_H__