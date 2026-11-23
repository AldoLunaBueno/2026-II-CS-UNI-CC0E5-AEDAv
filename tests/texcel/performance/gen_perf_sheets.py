#!/usr/bin/env python3
"""Generador procedural determinista de hojas .txl (~20,000 celdas) para pruebas de rendimiento de Texcel."""

from pathlib import Path


def index_to_col(c: int) -> str:
    s = ""
    while True:
        s = chr(ord("A") + (c % 26)) + s
        c = c // 26 - 1
        if c < 0:
            break
    return s


def escape_quoted(s: str) -> str:
    s = s.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{s}"'


def write_txl(cells: dict[str, str], out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    entries = [f"(({escape_quoted(k)}:{escape_quoted(v)}),0)" for k, v in cells.items()]
    with out_path.open("w", encoding="utf-8") as f:
        f.write("[" + ",".join(entries) + "]\n")


def generate_simple_sheet(out_path: Path, cols: int = 200, rows: int = 100) -> int:
    """
    Malla 2D de 200 x 100 = 20,000 celdas (A1..GR100) con operaciones aritméticas simples encadenadas.
    Todas las celdas dependen directa o indirectamente de A1 (profundidad máxima 299).
    Con A1 = 1  -> GR100 = 234.
    Con A1 = 10 -> GR100 = 243.
    """
    cells: dict[str, str] = {"A1": "1"}
    col_labels = [index_to_col(c) for c in range(cols)]

    # Fila 1: cadena horizontal A1 -> B1 -> ... -> GR1
    for c in range(1, cols):
        prev = f"{col_labels[c - 1]}1"
        mod = c % 3
        if mod == 1:
            expr = f"= {prev} + 2"
        elif mod == 2:
            expr = f"= {prev} - 1"
        else:
            expr = f"= {prev} + 1"
        cells[f"{col_labels[c]}1"] = expr

    # Filas 2..rows: cadena vertical por cada columna
    for r in range(2, rows + 1):
        for c in range(cols):
            above = f"{col_labels[c]}{r - 1}"
            expr = f"= {above} + 1" if (r % 2 == 0) else f"= {above} * 1 + 1"
            cells[f"{col_labels[c]}{r}"] = expr

    write_txl(cells, out_path)
    return len(cells)


def generate_mixed_sheet(out_path: Path) -> int:
    """
    Hoja mixta (~20,186 celdas) combinando +, -, *, /, ^, &, CONCAT, SUM, AVG, MIN, MAX, IF,
    comparaciones (=, <>, >=), COUNTIF, COUNTIFS y True/False.
    Todas las fórmulas convergen en F103 desde la semilla A1.
    Con A1 = 10 -> F103 = 11071.
    Con A1 = 20 -> F103 = 12625.
    """
    cells: dict[str, str] = {"A1": "10"}
    data_start_col = 6    # G
    data_cols = 192       # G..GP (48 bloques de 4: A, T, E, C)
    data_rows = 100       # filas 3..102
    c_start = index_to_col(data_start_col)
    c_end = index_to_col(data_start_col + data_cols - 1)
    tipos = ("A", "T", "E", "C")

    # Filas 1 y 2: cabeceras de referencia CORTE y TIPO
    for offset in range(data_cols):
        col = index_to_col(data_start_col + offset)
        cells[f"{col}1"] = "PC1" if offset < (data_cols // 2) else "PC2"
        cells[f"{col}2"] = tipos[offset % 4]

    # Matriz de datos True/False (100 x 192 = 19,200 celdas)
    for r in range(3, 3 + data_rows):
        for offset in range(data_cols):
            col = index_to_col(data_start_col + offset)
            cells[f"{col}{r}"] = "True" if (offset % 2 == 0) else "False"

    # Bloque de fórmulas encadenadas desde A1 (filas 3..102)
    corte_rng = f"${c_start}$1:${c_end}$1"
    tipo_rng = f"${c_start}$2:${c_end}$2"
    for r in range(3, 3 + data_rows):
        prev_a = "A1" if r == 3 else f"A{r - 1}"
        row_rng = f"${c_start}{r}:${c_end}{r}"
        cells[f"A{r}"] = f"= {prev_a} + 1"
        cells[f"B{r}"] = (
            f'= IF(A{r} >= 15, MIN(20, MAX(0, 4 * COUNTIFS({row_rng}, TRUE, {corte_rng}, "PC1", {tipo_rng}, "A") '
            f'/ MAX(1, COUNTIFS({corte_rng}, "PC1", {tipo_rng}, "A")) + 10)), 5)'
        )
        cells[f"C{r}"] = f"= (A{r} * 2 - 4) / 2 + 1^2"
        cells[f"D{r}"] = f'= CONCAT("R", B{r}, "_", C{r})'
        cells[f"E{r}"] = f'= IF(D{r} <> "", AVG(B{r}:C{r}) + COUNTIF({row_rng}, TRUE) / 96, 0)'
        cells[f"F{r}"] = f"= SUM(B{r}:C{r}) + E{r}"

    cells["F103"] = "= SUM(F3:F102)"

    write_txl(cells, out_path)
    return len(cells)


def main() -> int:
    n_simple = generate_simple_sheet(Path("tmp/perf_simple.txl"))
    n_mixed = generate_mixed_sheet(Path("tmp/perf_mixed.txl"))
    print(f"Generated tmp/perf_simple.txl ({n_simple} cells) and tmp/perf_mixed.txl ({n_mixed} cells)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
