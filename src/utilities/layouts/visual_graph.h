/**
 * @file visual_graph.h
 * @ingroup layouts
 * @brief Estructuras intermedias agnósticas al contenedor (`VisualNode`, `VisualEdge`, `VisualGraph`).
 */

#ifndef __VISUAL_GRAPH_H__
#define __VISUAL_GRAPH_H__

#include "../../containers/vector.h"
#include "../../types.h"

using namespace std;

/** @brief Vector ligero sin mutex ni mixins pesados para almacenar nodos y aristas visuales. */
template <typename T>
using SimpleVector = BaseVector<VectorAscTraits<T>, Callable>;

/** @brief Representación posicionada en 2D de un nodo para renderizado. */
struct VisualNode {
    TX id = -1;            /**< Identificador único del nodo dentro del grafo. */
    std::string label;     /**< Etiqueta formateada (puede incluir secuencias ANSI). */
    TX x = 0;              /**< Coordenada horizontal en celdas de lienzo. */
    TX y = 0;              /**< Coordenada vertical en filas de lienzo. */
};

/** @brief Arista dirigida o no dirigida entre dos `VisualNode` identificados por su `id`. */
struct VisualEdge {
    TX from_id = -1; /**< Identificador del nodo origen. */
    TX to_id = -1;   /**< Identificador del nodo destino. */
};

/** @brief Grafo 2D completo producido por un layout y consumido por un `GraphRenderer`. */
struct VisualGraph {
    SimpleVector<VisualNode> nodes; /**< Lista de nodos posicionados. */
    SimpleVector<VisualEdge> edges; /**< Lista de aristas entre nodos. */
    TX width = 0;                   /**< Ancho total del lienzo requerido. */
    TX height = 0;                  /**< Alto total del lienzo requerido. */
    bool directed = false;          /**< Indica si las aristas deben dibujarse con flechas. */
};

#endif // __VISUAL_GRAPH_H__