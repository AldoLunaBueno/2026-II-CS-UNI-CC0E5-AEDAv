/**
 * @file layouts.h
 * @ingroup layouts
 * @brief Cabecera unificada de layouts de renderizado gráfico y proyección 2D.
 *
 * Agrupa:
 * - `base_layout.h`: `BaseLayout<Formatter>` y `DefaultNodeFormatter`
 * - `tree_layout.h`: `TreeLayout<Formatter>` y `TreeRole`
 * - `vertical_tree_layout.h`: `VerticalTreeLayout<Formatter>`
 * - `horizontal_tree_layout.h`: `HorizontalTreeLayout<Formatter>`
 * - `circular_list_layout.h`: `CircularListLayout<Formatter>` y `CircularListRole`
 * - `merkle_tree_layout.h`: `MerkleTreeLayout<Formatter>` y `MerkleRole`
 */

#ifndef __LAYOUTS_H__
#define __LAYOUTS_H__

#include "layouts/base_layout.h"
#include "layouts/tree_layout.h"
#include "layouts/vertical_tree_layout.h"
#include "layouts/horizontal_tree_layout.h"
#include "layouts/circular_list_layout.h"
#include "layouts/merkle_tree_layout.h"

#endif // __LAYOUTS_H__
