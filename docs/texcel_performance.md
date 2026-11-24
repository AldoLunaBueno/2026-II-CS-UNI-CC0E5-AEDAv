# Evolución de Rendimiento — Texcel

Registro histórico de los hitos de optimización del motor de **Texcel**, medidos de forma reproducible mediante `make test-perf` (`g++ -std=c++23 -O2`) sobre dos hojas `.txl` generadas proceduralmente en [`tests/texcel/performance/`](../tests/texcel/performance/):

- **Simple (`tmp/perf_simple.txl`)**: `20,000` celdas (`200 cols × 100 filas`, `A1..GR100`) encadenadas con operaciones aritméticas simples (`+`, `-`, `*`) desde `A1` hasta `GR100`.
- **Mixta (`tmp/perf_mixed.txl`)**: `20,186` celdas combinando `+`, `-`, `*`, `/`, `^`, `CONCAT`, `SUM`, `AVG`, `MIN`, `MAX`, `IF`, comparaciones (`>=`, `<>`), `COUNTIF`, `COUNTIFS` y bloques `True`/`False`, convergiendo desde `A1` hasta `F103`.

---

## Tabla de Hitos

| Hito | Descripción | Fecha | Simple: Load | Simple: Recalc | Simple: Edit→Fin | Simple: Clear | Mixta: Load | Mixta: Recalc | Mixta: Edit→Fin | Mixta: Clear | Total (ms) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| [H0](#h0--punto-cero-línea-base) | Punto cero (línea base) | 2026-11-23 | 21.30 | 932.49 | 4724.05 | 12.22 | 19.73 | 2035.12 | 3236.44 | 9.45 | **10990.80** |
| [H1](#h1--conjunto-avl-en-bfs-y-rangos) | Conjunto AVL en BFS y rangos | 2026-11-23 | 21.43 | 903.37 | 1275.92 | 8.92 | 18.41 | 1782.47 | 2939.75 | 9.82 | **6960.09** |
| [H2](#h2--preservación-del-grafo-de-dependencias-en-propagación) | Preservación de grafo en propagación | 2026-11-23 | 21.32 | 973.88 | 1106.58 | 13.00 | 19.46 | 1854.23 | 762.14 | 8.10 | **4758.71** |
| [H3](#h3--búsqueda-directa-en-basehashtable-y-eliminación-de-lookupsformateos-redundantes) | `find_by` / `find` sin `Value{}` | 2026-11-23 | 21.32 | 470.55 | 337.59 | 10.59 | 19.23 | 1055.15 | 221.31 | 10.60 | **2146.34** |
| 🔴 [H4](#h4--caché-de-ast-compilado-descartado-empíricamente) | Caché AST (descartado: regresión) | 2026-11-23 | 21.32 | 502.92 | 343.78 | 19.33 | 20.33 | 1058.93 | 518.96 | 49.86 | 🔴 **2535.42** |
| [H5](#h5--recalculateall-de-una-sola-pasada-rpn-directo-toid-en-pila-y-fusión-de-lookups) | 1-pass `recalculateAll`, RPN/`toId` y fusión de lookups | 2026-11-23 | 22.84 | 395.57 | 267.30 | 13.42 | 17.83 | 846.97 | 179.26 | 12.12 | **1755.31** |
| [H6](#h6--índice-disperso-o1-rangos-compactos-criterios-compilados-y-cortocircuito-en-prepostorder) | Índice `m_grid_index` $O(1)$, `RangeSubVec`, criterios compilados y cortocircuito `IF` | 2026-11-24 | 21.10 | 262.45 | 219.05 | 10.25 | 18.71 | 130.34 | 15.28 | 4.16 | **681.34** |
| 🚀 [H7](#h7--el-gran-salto-concurrente-construcción-avl-paralela-buildfrominorder-thread-pool-y-kahn-dinámico-) | `buildFromInOrder` paralelo, `TexcelThreadPool` y DAG Kahn dinámico | 2026-11-24 | 19.70 | 36.26 | 42.83 | 8.14 | 17.68 | 9.45 | 5.36 | 4.19 | **143.61** |

---

## Síntesis por Hito

### H0 — Punto cero (línea base)
Estado inicial del motor ([`src/texcel/engine.h`](../src/texcel/engine.h)) antes de iniciar la serie de optimizaciones estructurales. En este punto, el motor ya cuenta con la tabla dispersa `BaseHashTable` respaldada por `BaseAVLTree` y con caché por celda (`SpreadsheetCell::dirty`), pero las operaciones de propagación y recolección de dependencias presentan costos cuadráticos ocultos:

1. **Deduplicación lineal en BFS y en rangos**: tanto al descubrir el subgrafo afectado (`collectAndMarkDirtySubgraph`), como al recolectar referencias dentro del parser (`FormulaParser::addRef`), la verificación de unicidad se realizaba con `addUniqueStr`, que escanea linealmente un `Vector` desde el inicio en cada inserción ($O(N^2)$).
2. **Reconstrucción completa de aristas en cada propagación**: cada vez que una celda cambiaba de valor aguas arriba, todas las celdas descendientes volvían a parsear sus rangos para recolectar referencias, se desuscribían de `m_dependents` (reconstruyendo cada `LinkedList`) y volvían a suscribirse exactamente a las mismas celdas de entrada.

Este estado base sirve como referencia (**10,990.80 ms** totales) para medir el impacto real de cada mejora posterior.

### H1 — Conjunto AVL en BFS y rangos
**El problema:** Al editar `A1` en una cadena de 20,000 celdas dependientes, `collectAndMarkDirtySubgraph` ([`src/texcel/engine.h`](../src/texcel/engine.h)) iba construyendo la lista `affected` de celdas a recalcular. Para evitar visitar dos veces la misma celda en grafos con diamantes (celdas alcanzables por múltiples caminos), comprobaba si el identificador ya estaba en `affected` recorriendo todo el vector con `containsStr`. Con 20,000 nodos, eso implicaba ~200 millones de comparaciones de cadenas solo para armar la cola BFS. Un cuello de botella análogo ocurría en `FormulaParser::addRef` ([`src/texcel/parser.h`](../src/texcel/parser.h)) cuando una fórmula referenciaba rangos amplios.

**La solución:** Se introdujo `texcel_detail::StringSet` en [`src/texcel/types.h`](../src/texcel/types.h), implementado directamente como un alias de nuestra propia `BaseHashTable<HashTableTraits<std::string, bool>, Callable>` (respaldada por `BaseAVLTree` en cada cubeta):
- En `collectAndMarkDirtySubgraph`, un `StringSet seen` registra en $O(1)$ promedio qué celdas ya entraron a la cola BFS, manteniendo `affected` como un `Vector` ordenado por nivel topológico aproximado.
- En `FormulaParser`, un `StringSet m_seen_refs` filtra referencias duplicadas sin recorrer el `Vector` de salida `m_refs`.

**El impacto:** El tiempo de `Edit→Fin` en la hoja Simple cayó de **4,724.05 ms** a **1,275.92 ms** (**3.7× más rápido**) y el total global bajó de **10,990.80 ms** a **6,960.09 ms**, manteniendo intacto el protagonismo de `BaseHashTable` y `BaseAVLTree`.

### H2 — Preservación del grafo de dependencias en propagación
**El problema:** Tras eliminar el $O(N^2)$ del BFS en H1, la hoja Mixta seguía tardando **2,939.75 ms** en `Edit→Fin` (y la Simple **1,275.92 ms**). La razón estaba en `SpreadsheetEngine::resolveCellValue` ([`src/texcel/engine.h`](../src/texcel/engine.h)): cada vez que se propagaba un cambio de valor por las ~20,000 celdas descendientes, el motor trataba cada celda afectada como si su fórmula hubiera sido reescrita desde cero:
1. `FormulaParser` expandía nuevamente todos los rangos (`A1:B2`, `G2:GR2` en `COUNTIFS`, `SUM`, `AVG`, etc.) mediante `registerRangeRefs`, recorriendo cientos de celdas por rango para poblar `m_seen_refs` y `refs`.
2. `unsubscribeInputs` recorría `cell.inputs` y ejecutaba `listRemove` sobre `m_dependents[in_id]`, copiando y reconstruyendo la `LinkedList` de cada celda de entrada.
3. Inmediatamente después, volvía a recorrer `refs` para reinsertar exactamente las mismas aristas en `m_dependents` haciendo un escaneo lineal `addUniqueStr` sobre cada `LinkedList`.

En otras palabras: **un cambio numérico estaba destruyendo y reconstruyendo todo el grafo de dependencias 20,000 veces**, cuando la topología del grafo no había cambiado en absoluto.

**La solución:** Separar el *cambio de valor* del *cambio de fórmula*:
1. En `FormulaParser` ([`src/texcel/parser.h`](../src/texcel/parser.h)), `m_refs` pasó a ser un puntero opcional (`texcel_detail::StringVec* m_refs = nullptr`). Cuando es `nullptr`, tanto `addRef` como `registerRangeRefs` retornan en $O(1)$ sin expandir rangos ni llenar `m_seen_refs`.
2. En `SpreadsheetEngine::resolveCellValue` ([`src/texcel/engine.h`](../src/texcel/engine.h)), antes de evaluar se comprueba `same_expr = m_cells.contains(cell_id) && m_cells[cell_id].raw_expression == raw`. Si `same_expr` es verdadero, la evaluación recibe `nullptr` para las referencias y omite por completo `unsubscribeInputs`, la copia de `inputs` y la modificación de `m_dependents`.
3. Cuando la celda editada directamente en `setCell` sí cambia (o al iniciar en `recalculateAll`), `setCell` limpia previamente `m_cells[cid].raw_expression` tras `unsubscribeInputs(cid)` para forzar que solo esa celda reconstruya sus aristas; además, dado que `unsubscribeInputs` ya removió apariciones previas y `m_seen_refs` garantiza unicidad en `refs`, la suscripción en `m_dependents[ref_id]` inserta directamente sin escaneo lineal previo.

**El impacto:** El `Edit→Fin` de la hoja Mixta cayó de **2,939.75 ms** a **762.14 ms** (**3.86× más rápido** respecto a H1 y **4.25×** respecto a H0), y el total global se redujo de **6,960.09 ms** a **4,758.71 ms**.

### H3 — Búsqueda directa en `BaseHashTable` y eliminación de lookups/formateos redundantes
**El problema:** Incluso tras preservar la topología del grafo en H2, cada evaluación seguía pagando tres costos ocultos en nuestras estructuras y en el motor:
1. `BaseHashTable` ([`src/containers/hash_table.h`](../src/containers/hash_table.h)) solo ofrecía `contains(key)` y `operator[](key)`. Ambos construían un objeto `HashEntry<Key, Value>(key, Value{})` temporal completo antes de bajar por el `BaseAVLTree` (creando `SpreadsheetCell` o `LinkedList` vacíos con sus propios `std::mutex` y `Vector` solo para consultar una clave), además de realizar dos comparaciones por nivel en `BaseBinaryTree::find` ([`src/containers/trees/binary_tree.h`](../src/containers/trees/binary_tree.h)) y adquirir un doble lock cuando `operator[]` llamaba a `find_or_insert`.
2. Al no existir `find(key) -> Value*`, [`src/texcel/engine.h`](../src/texcel/engine.h) repetía el patrón `if (tabla.contains(k)) { ... tabla[k] ... }` en `hasRawCell`, `readCellForEval`, `resolveCellValue`, `collectAndMarkDirtySubgraph` y `unsubscribeInputs`, ejecutando de 2 a 4 recorridos AVL por cada lectura lógica de celda o lista de dependientes.
3. En `recalculateAll`, el bucle llamaba a `recomputeCell`, que formateaba dos `std::ostringstream` (`prev` y `now` vía `displayValue()`) por cada una de las 20,000 celdas aunque su resultado no se usara en ese paso.

**La solución:** Mejorar directamente los contenedores del curso sin abandonar `BaseAVLTree` ni `BaseHashTable`:
1. En `BaseBinaryTree` ([`src/containers/trees/binary_tree.h`](../src/containers/trees/binary_tree.h)), se agregó `find_by(key, cmp)` con comparación de tres vías (`<=>`): busca por clave heterogénea sin construir `value_type` y reduce a la mitad las comparaciones de cadena en cada nivel del AVL.
2. En `BaseHashTable` ([`src/containers/hash_table.h`](../src/containers/hash_table.h)), se implementó `Value* find(const Key&)` apoyado en `find_by`, `contains` pasó a delegar en `find`, y `operator[]` consulta primero `find_by` bajo un único `lock_guard` antes de recurrir a `find_or_insert`.
3. En `SpreadsheetEngine` ([`src/texcel/engine.h`](../src/texcel/engine.h)), se reemplazaron los patrones `contains + operator[]` por punteros de una sola búsqueda (`find`) y se separó `evalCellQuiet` (usada en `recalculateAll` y en la celda raíz de `setCell`) de `recomputeCell` (que además compara primero los campos crudos con `cellStateEqual` antes de invocar `displayValue()`).

**El impacto:** `Recalc` y `Edit→Fin` cayeron a más de la mitad en ambos escenarios: Simple bajó de **2,114.78 ms** a **840.07 ms** (`Edit→Fin` **337.59 ms**), Mixta bajó de **2,643.93 ms** a **1,306.28 ms** (`Edit→Fin` **221.31 ms**), y la suma total se redujo de **4,758.71 ms** a **2,146.34 ms** (**5.12× más rápido** respecto a H0).

### 🔴 H4 — Caché de AST compilado (Descartado empíricamente)
**La hipótesis:** Conservar el árbol `BinaryTreeNode<AstToken>` compilado dentro de cada `SpreadsheetCell` mediante `std::shared_ptr` (con deleter `freeAst`) y omitir la reconstrucción de `postorder_rpn` cuando `raw_expression` no cambia, para evitar el re-parseo y las reservas `new`/`delete` durante `Edit→Fin`.

**El resultado empírico (regresión de +389.08 ms respecto a H3):** Aunque la suite funcional pasó íntegramente, el benchmark empeoró de **2,146.34 ms** (`H3`) a **2,535.42 ms** (`H4`):
- `Mixta Edit→Fin` subió de **221.31 ms** a **518.96 ms** (+297.65 ms).
- `Clear` subió de **10.59 ms / 10.60 ms** a **19.33 ms / 49.86 ms** (+48.00 ms combinados).

**Por qué ocurrió y dónde está el límite:**
1. **El parseo ya no era el cuello de botella:** Tras `H1`–`H3`, `FormulaParser` sobre fórmulas individuales cuesta muy poco; en cambio, mantener 20,000 árboles `BinaryTreeNode<AstToken>` vivos en el heap junto con bloques de control atómicos de `std::shared_ptr` aumenta la huella de memoria de `SpreadsheetCell`, degrada la localidad de caché L2/L3, encarece cada copia/movimiento de `SpreadsheetCell` y cuadruplica el costo de liberación en `Clear`.
2. **El costo dominante restante en `Mixta`:** No está en `FormulaParser`, sino en los recorridos masivos de rangos (`readCellForEval` dentro de `SUM`, `AVG`, `COUNTIF`, `COUNTIFS`) y en el iterador `PrePostIter`.

**Decisión:** Se descartaron los cambios de código en `src/texcel/types.h` y `src/texcel/engine.h`, conservando el óptimo alcanzado en **H3 (`2,146.34 ms`)**.

### H5 — `recalculateAll` de una sola pasada, RPN directo, `toId` en pila y fusión de lookups
**El problema:** Tras revertir `H4`, el perfil de `H3` (`2,146.34 ms`) todavía acumulaba cuatro fuentes de trabajo redundante dentro del motor, sin necesidad de incrementar la huella de memoria de `SpreadsheetCell`:
1. `SpreadsheetEngine::recalculateAll` ([`src/texcel/engine.h`](../src/texcel/engine.h)) ejecutaba tres recorridos completos sobre `m_raw_table`, construía un árbol AVL temporal `prev_display` de 20,000 cadenas, formateaba `displayValue()` antes y después, y ensamblaba un `StringVec updated` que ningún llamador del proyecto utilizaba; además volvía a buscar `m_raw_table.find(cid)` dentro de `resolveCellValue` aunque el bucle ya disponía de la clave y el valor crudo.
2. `evaluateAstPostOrder` instanciaba un `std::ostringstream` (con inicialización de `std::locale` y buffer dinámico) en cada fórmula evaluada, incluso durante `Edit→Fin` cuando `same_expr == true` y `cell.postorder_rpn` ya estaba calculado.
3. En `propagateAffected` → `recomputeCell` → `resolveCellValue`, una misma celda `cid` se buscaba hasta 5 veces entre `m_raw_table` y `m_cells` y copiaba `raw_expression` en cada comprobación de cambio.
4. `CellCoord::toId` y `CellCoord::colToLabel` ([`src/texcel/types.h`](../src/texcel/types.h)) —invocados ~230,000 veces en los rangos de la hoja Mixta— usaban `std::string::insert(begin(), ...)` y concatenaban tres `std::string` temporales por coordenada, mientras que `formatCellNumber` instanciaba un `std::ostringstream` por cada número decimal.

**La solución:**
1. En [`src/texcel/engine.h`](../src/texcel/engine.h), `recalculateAll` pasó a una única pasada directa sobre `m_raw_table`, inyectando `&kv.getValue()` en `evalCellQuiet` → `resolveCellValue`.
2. `resolveCellValue` recibió punteros opcionales `known_raw` y `known_cell` para reutilizar el `const std::string*` y el `SpreadsheetCell*` ya localizados por `propagateAffected` y `recomputeCell` (reduciendo de 5 descensos AVL por celda a 2 y evitando copiar `raw_expression`).
3. `evaluateAstPostOrder` y `evaluateRawExpression` pasaron a recibir `std::string* out_rpn`: omiten la generación de RPN cuando `same_expr == true` y, cuando se requiere, construyen la cadena directamente con `std::string::append`/`push_back` sin `std::ostringstream`.
4. En [`src/texcel/types.h`](../src/texcel/types.h), `CellCoord::colToLabel`, `CellCoord::toId` y `formatCellNumber` se reescribieron sobre buffers pequeños en pila (`char buf[...]`) y `std::snprintf`, aprovechando SSO y evitando realocaciones temporales.

**El impacto:** El tiempo bajó en todas las fases de cálculo de ambos escenarios: Simple cayó de **840.07 ms** a **699.14 ms** (`Recalc` **395.57 ms**, `Edit→Fin` **267.30 ms**), Mixta cayó de **1,306.28 ms** a **1,056.18 ms** (`Recalc` **846.97 ms**, `Edit→Fin` **179.26 ms**), y el total global bajó de **2,146.34 ms** a **1,755.31 ms** (**6.26× más rápido** respecto a H0).

### H6 — Índice disperso $O(1)$, rangos compactos, criterios compilados y cortocircuito en `PrePostOrder`
**El problema:** En `H5` (`1,755.31 ms`), la hoja Mixta todavía concentraba **1,026.23 ms** entre `Recalc` (`846.97 ms`) y `Edit→Fin` (`179.26 ms`), mientras que el arranque sobre hojas reales con rangos amplios (como `calificaciones_aeda.txl` con `$K$1:$ZZ$1`) superaba los `1.5 s`. El perfilado identificó cinco cuellos de botella restantes:
1. **Lectura de rangos mediante descenso AVL + conversión de IDs**: `forEachArgValue` y `evalCountIf` generaban la cadena `CellCoord(col, row).toId()` para cada celda de cada rango y descendían por el `BaseAVLTree` de `m_cells`/`m_raw_table` bajo `std::lock_guard` en cientos de miles de iteraciones.
2. **Expansión celda a celda de rangos en `DepGraph`**: cada referencia de rango (`G2:GR2` o `$K$1:$ZZ$1`) suscribía cientos de entradas individuales en `m_dependents` y en `cell.inputs`, inflando la reconstrucción y limpieza del grafo.
3. **Re-parseo de criterios en `COUNTIF`/`COUNTIFS`**: `matchesCriteria` analizaba el prefijo del criterio (`>=`, `<>`, número o texto) y convertía con `std::stod` en cada celda del rango en vez de hacerlo una sola vez por criterio.
4. **Evaluación ansiosa de literales y ramas descartadas**: `recalculateAll` resolvía los ~19,300 literales numéricos/booleanos de Mixta mediante `resolveCellValue` + `evaluateRawExpression`, y `IF` evaluaba ambas ramas aunque la condición descartara una de ellas.
5. **Excepciones C++ para errores de fórmula**: los errores de evaluación (`#DIV/0!`, `#VALUE!`, `#CYCLE!`) se propagaban con `throw EvalError`.

**La solución:** Sin sustituir los contenedores del curso (`BaseHashTable`, `BaseAVLTree`, `BinaryTreeNode`, `LinkedList`, `BaseVector`):
1. **Índice disperso $O(1)$ sobre `BaseVector` (`m_grid_index`)**: en [`src/texcel/engine.h`](../src/texcel/engine.h), `m_grid_index[row][col]` almacena punteros estables hacia los nodos de `m_cells` (`BaseAVLTree`), junto con el *bounding box* activo (`m_max_used_col`, `m_max_used_row`). `readCellByCoord` consulta cualquier celda en $O(1)$ sin construir `std::string` ni descender el AVL, y `evalCountIf` acota los recorridos al área con datos reales.
2. **Suscripción compacta de rangos (`CellRangeRect` + `RangeSubVec`)**: en [`src/texcel/types.h`](../src/texcel/types.h) y [`src/texcel/parser.h`](../src/texcel/parser.h), los rangos se registran como rectángulos normalizados `CellRangeRect` en `m_range_subs` (`BaseVector`), eliminando la expansión masiva de celdas vacías en `DepGraph`.
3. **Compilación previa de criterios (`compileCriterion`)**: `evalCountIf` precompila cada criterio una única vez en `ParsedCriterion` antes del bucle sobre las celdas.
4. **Dos pasadas en `recalculateAll` y cortocircuito en `Phase::Pre`**: `recalculateAll` indexa y resuelve todos los literales en $O(1)$ con `evaluateLiteralFast` en la primera pasada, y evalúa únicamente las fórmulas en la segunda. En `evaluateAstPostOrder`, `IF` e `IFERROR` marcan `skip_eval` en `Phase::Pre` sobre la rama descartada sin evaluarla.
5. **Propagación de errores por valor**: `CellEvalValue::err_code` reemplaza el uso de excepciones C++ en la evaluación normal de fórmulas.

**El impacto:** El tiempo total en `make test-perf` bajó de **1,755.31 ms** (`H5`) a **681.34 ms** (**2.58× más rápido** que `H5` y **16.13× más rápido** que `H0`):
- **Mixta (`20,186` celdas)**: `Recalc` cayó de **846.97 ms** a **130.34 ms** (**6.50×**), `Edit→Fin` cayó de **179.26 ms** a **15.28 ms** (**11.73×**) y `Clear` de **12.12 ms** a **4.16 ms** (subtotal de **1,056.18 ms** a **168.49 ms**).
- **Simple (`20,000` celdas)**: `Recalc` bajó de **395.57 ms** a **262.45 ms**, `Edit→Fin` de **267.30 ms** a **219.05 ms** y `Clear` de **13.42 ms** a **10.25 ms** (subtotal de **699.14 ms** a **512.85 ms**).

### H7 — El Gran Salto Concurrente: Construcción AVL Paralela (`buildFromInOrder`), Thread Pool y Kahn Dinámico 🚀
**El problema:** Tras alcanzar `681.34 ms` en `H6`, el perfilado fino reveló una verdad contundente: el **80%–86% de `Recalc`** (`121 ms` en Simple y `114 ms` en Mixta para `m_cells`, más `89 ms` en Simple para `m_dependents`) seguía atrapado en inserciones secuenciales nodo a nodo sobre nuestro único `BaseAVLTree` bajo `std::mutex`, mientras `Edit→Fin` en Simple (`219 ms`) recorría todo en un solo núcleo. Paralelizar con *lock-coupling* por nodo habría destruido el rendimiento, y abandonar la estructura original del curso estaba fuera de discusión.

**La solución:** Sin renunciar a nuestro único `BaseAVLTree` / `BaseHashTable`:
1. **Construcción paralela $O(N)$ con cero rotaciones y cero comparaciones (`buildFromInOrder`)**: aprovechando que `m_raw_table` ya entrega sus claves ordenadas en `InOrder` (y que las aristas de `m_dependents` pueden agruparse por clave), [`src/containers/trees/binary_tree.h`](../src/containers/trees/binary_tree.h) y [`src/containers/hash_table.h`](../src/containers/hash_table.h) ahora construyen un `BaseAVLTree` perfectamente balanceado (`|bf| <= 1`) dividiendo el trabajo en **4 subárboles disjuntos** a profundidad 2. Los 4 núcleos ensamblan su cuadrante en paralelo sin locks, moviendo nodos `Pair` in-place ([`src/containers/core/general_node.h`](../src/containers/core/general_node.h)) y calculando `m_height` al ascender.
2. **Thread Pool persistente (`TexcelThreadPool`) y DAG paralelo (Kahn dinámico con afinidad L1)**: en [`src/texcel/engine.h`](../src/texcel/engine.h), `evalDirtyGraphParallel` despierta a los 4 workers en microsegundos para subgrafos de $\ge 64$ celdas, leyendo celdas padre sin bloqueo (`find_unlocked` / `m_grid_index`) y encadenando inmediatamente en el mismo hilo el primer hijo desbloqueado para exprimir la caché L1, con respaldo secuencial automático para ciclos `#CYCLE!`.
3. **BFS ultracompacto por coordenadas**: `collectAndMarkDirtySubgraph` sustituye el `StringSet` AVL por un bitmap plano sobre el *bounding box* activo.

**El impacto:** Una hazaña histórica que lleva el tiempo total de casi **11 segundos (`10,990.80 ms`)** en `H0` a **`143.61 ms`** en `H7` (**76.53× más rápido** que el punto cero y **4.74×** sobre `H6`, procesando en conjunto **40,186 celdas** en `4` núcleos físicos con `-O2`):
- **Mixta (`20,186` celdas — `19,585` literales + `601` fórmulas con `IF`, `MIN`, `MAX`, `AVG`, `SUM`, `CONCAT`, `COUNTIF` y `COUNTIFS` sobre rangos de 192 columnas hasta `F103`)**:
  - `Recalc` (indexar las 20,186 celdas en `m_cells`, parsear las 601 fórmulas, armar `m_dependents`/`m_range_subs` y evaluar todo el árbol de expresiones) se desplomó de **130.34 ms** a apenas **9.45 ms** (**13.79×** sobre `H6` y **215×** frente a los `2,035.12 ms` de `H0`).
  - `Edit→Fin` (editar `A1` y propagar el recálculo en cascada por las 100 filas encadenadas `A3..A102` → `B..F` → `F103` re-evaluando rangos `COUNTIFS` de 192 columnas) bajó a **5.36 ms** (**603×** frente a los `3,236.44 ms` de `H0`), completando todo el ciclo de vida en **36.68 ms** (¡menos que un parpadeo!).
- **Simple (`20,000` celdas — malla `200 × 100` con `19,999` fórmulas encadenadas hasta profundidad `299` en `GR100`)**:
  - `Recalc` (construir `m_cells`, parsear las 19,999 fórmulas a AST, poblar `m_dependents` y evaluar todo el grafo) cayó de **262.45 ms** a **36.26 ms** (**7.24×** sobre `H6` y **25.7×** frente a `H0`).
  - `Edit→Fin` (editar `A1`, descubrir el subgrafo de las 20,000 celdas afectadas y propagar la cascada completa hasta `GR100`) cayó de **219.05 ms** a **42.83 ms** (**5.11×** sobre `H6` y **110×** frente a los `4,724.05 ms` de `H0`), cerrando todo el escenario en **106.93 ms**.
