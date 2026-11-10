#ifndef __LAYOUTS_H__
#define __LAYOUTS_H__

// ============================================================================
// Cabecera unificada de Layouts de Renderizado Gráfico
// Agrupa la jerarquía base, roles semánticos y algoritmos de proyección 2D:
//  - base_layout.h: BaseLayout<Formatter> y DefaultNodeFormatter
//  - tree_layout.h: TreeLayout<Formatter> y TreeRole
//  - vertical_tree_layout.h: VerticalTreeLayout<Formatter>
//  - horizontal_tree_layout.h: HorizontalTreeLayout<Formatter>
//  - circular_list_layout.h: CircularListLayout<Formatter> y CircularListRole
//  - merkle_tree_layout.h: MerkleTreeLayout<Formatter> y MerkleRole
// ============================================================================

#include "layouts/base_layout.h"
#include "layouts/tree_layout.h"
#include "layouts/vertical_tree_layout.h"
#include "layouts/horizontal_tree_layout.h"
#include "layouts/circular_list_layout.h"
#include "layouts/merkle_tree_layout.h"

#endif // __LAYOUTS_H__
