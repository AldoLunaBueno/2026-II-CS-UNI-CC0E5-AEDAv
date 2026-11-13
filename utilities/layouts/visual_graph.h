#ifndef __VISUAL_GRAPH_H__
#define __VISUAL_GRAPH_H__

#include "../../containers/vector.h"
#include "../../types.h"

using namespace std;

// Estructuras del grafo visual (agnósticas al contenedor)
template <typename T>
using SimpleVector = BaseVector<VectorAscTraits<T>, Callable>;

struct VisualNode {
    TX id = -1;
    std::string label;
    TX x = 0;
    TX y = 0;
    std::string css_class;
};

struct VisualEdge {
    TX from_id = -1;
    TX to_id = -1;
};

struct VisualGraph {
    SimpleVector<VisualNode> nodes;
    SimpleVector<VisualEdge> edges;
    TX width = 0;
    TX height = 0;
    bool directed = false;
};

#endif // __VISUAL_GRAPH_H__