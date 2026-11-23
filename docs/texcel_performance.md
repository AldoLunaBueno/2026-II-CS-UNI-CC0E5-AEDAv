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
