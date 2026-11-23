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

---

## Síntesis por Hito

### H0 — Punto cero (línea base)
Estado inicial del motor (`src/texcel/engine.h`) antes de iniciar la serie de optimizaciones estructurales, con `BaseHashTable` sobre `BaseAVLTree`, caché `dirty` en `SpreadsheetCell`, recolección de subgrafo BFS sobre `Vector` (`addUniqueStr` lineal) y re-parseo/re-suscripción de dependencias en cada recálculo de celda. Sirve como referencia base (**10,990.80 ms** totales) para medir objetivamente cada mejora posterior.

### H1 — Conjunto AVL en BFS y rangos
Se añadió `texcel_detail::StringSet` (`BaseHashTable<HashTableTraits<std::string, bool>, Callable>` sobre `BaseAVLTree`) en [`src/texcel/types.h`](../src/texcel/types.h#L214) para reemplazar las búsquedas lineales $O(N^2)$ de `addUniqueStr` sobre `Vector` en `collectAndMarkDirtySubgraph`, `propagateAffected` ([`src/texcel/engine.h`](../src/texcel/engine.h#L478-L520)) y `FormulaParser::addRef` ([`src/texcel/parser.h`](../src/texcel/parser.h#L56-L61)). Esto redujo el `Edit→Fin` simple de **4,724.05 ms** a **1,275.92 ms** (3.7× más rápido) y el total global de **10,990.80 ms** a **6,960.09 ms**.
