#!/usr/bin/env python3
"""Traductor universal de hojas .xlsx / .csv a formato .txl para Texcel (sin dependencias externas)."""

import argparse
import csv
import datetime
import os
import re
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

MAX_COLS = 702  # A..ZZ
MAX_ROWS = 999  # 1..999
CELL_REF_RE = re.compile(r"^([A-Z]+)(\d+)$")


def col_to_index(label: str) -> int:
    val = 0
    for ch in label:
        val = val * 26 + (ord(ch) - ord("A") + 1)
    return val - 1


def index_to_col(c: int) -> str:
    s = ""
    while True:
        s = chr(ord("A") + (c % 26)) + s
        c = c // 26 - 1
        if c < 0:
            break
    return s


def is_valid_coord(coord: str) -> bool:
    m = CELL_REF_RE.match(coord)
    if not m:
        return False
    col = col_to_index(m.group(1))
    row = int(m.group(2)) - 1
    return 0 <= col < MAX_COLS and 0 <= row < MAX_ROWS


def escape_quoted(s: str) -> str:
    s = s.replace("\r\n", " ").replace("\r", " ").replace("\n", " ")
    s = s.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{s}"'


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def load_date_style_ids(zf: zipfile.ZipFile) -> set[str]:
    if "xl/styles.xml" not in zf.namelist():
        return set()
    root = ET.fromstring(zf.read("xl/styles.xml"))
    builtin_date_ids = set(range(14, 23)) | set(range(45, 48))
    custom_date_ids: set[int] = set()

    for el in root.iter():
        if local_name(el.tag) == "numFmt":
            try:
                fid = int(el.attrib.get("numFmtId", "-1"))
            except ValueError:
                continue
            fmt = el.attrib.get("formatCode", "")
            cleaned = re.sub(r'"[^"]*"|\[[^\]]*\]|\\.', "", fmt).lower()
            if any(ch in cleaned for ch in ("y", "d")) or ("m" in cleaned and "0" not in cleaned):
                custom_date_ids.add(fid)

    date_xf_indices: set[str] = set()
    for el in root.iter():
        if local_name(el.tag) == "cellXfs":
            idx = 0
            for xf in el:
                if local_name(xf.tag) != "xf":
                    continue
                try:
                    fid = int(xf.attrib.get("numFmtId", "-1"))
                except ValueError:
                    fid = -1
                if fid in builtin_date_ids or fid in custom_date_ids:
                    date_xf_indices.add(str(idx))
                idx += 1
            break
    return date_xf_indices


def excel_serial_to_iso(val_str: str) -> str | None:
    try:
        days = int(float(val_str))
    except ValueError:
        return None
    if days < 1 or days > 100000:
        return None
    return (datetime.date(1899, 12, 30) + datetime.timedelta(days=days)).isoformat()


def load_shared_strings(zf: zipfile.ZipFile) -> list[str]:
    if "xl/sharedStrings.xml" not in zf.namelist():
        return []
    root = ET.fromstring(zf.read("xl/sharedStrings.xml"))
    strings = []
    for si in root:
        if local_name(si.tag) != "si":
            continue
        parts = [el.text for el in si.iter() if local_name(el.tag) == "t" and el.text]
        strings.append("".join(parts))
    return strings


def list_xlsx_sheets(zf: zipfile.ZipFile) -> list[tuple[str, str, bool]]:
    wb = ET.fromstring(zf.read("xl/workbook.xml"))
    rels = ET.fromstring(zf.read("xl/_rels/workbook.xml.rels"))
    rid_to_target = {}
    for rel in rels:
        rid = rel.attrib.get("Id")
        target = rel.attrib.get("Target", "").lstrip("/")
        if not target.startswith("xl/"):
            target = "xl/" + target
        if rid:
            rid_to_target[rid] = target

    sheets = []
    for el in wb.iter():
        if local_name(el.tag) == "sheet":
            name = el.attrib.get("name", "Sheet")
            state = el.attrib.get("state", "visible")
            rid = next((v for k, v in el.attrib.items() if k.endswith("}id") or k == "r:id"), "")
            target = rid_to_target.get(rid, "")
            if target:
                sheets.append((name, target, state == "visible"))
    return sheets


def parse_xlsx_sheet(zf: zipfile.ZipFile, sheet_path: str, shared: list[str], date_styles: set[str]) -> tuple[dict[str, str], int]:
    root = ET.fromstring(zf.read(sheet_path))
    cells: dict[str, str] = {}
    skipped = 0

    for c_el in root.iter():
        if local_name(c_el.tag) != "c":
            continue
        coord = c_el.attrib.get("r", "").upper()
        if not is_valid_coord(coord):
            skipped += 1
            continue

        ctype = c_el.attrib.get("t", "")
        f_el = v_el = is_el = None
        for child in c_el:
            ln = local_name(child.tag)
            if ln == "f":
                f_el = child
            elif ln == "v":
                v_el = child
            elif ln == "is":
                is_el = child

        raw_val = ""
        if f_el is not None and f_el.text and f_el.text.strip():
            f_txt = f_el.text.strip()
            raw_val = f_txt if f_txt.startswith("=") else ("=" + f_txt)
        elif ctype == "s" and v_el is not None and v_el.text is not None:
            idx = int(v_el.text)
            raw_val = shared[idx] if 0 <= idx < len(shared) else ""
        elif ctype == "b" and v_el is not None and v_el.text is not None:
            raw_val = "True" if v_el.text.strip() == "1" else "False"
        elif ctype == "inlineStr" and is_el is not None:
            parts = [el.text for el in is_el.iter() if local_name(el.tag) == "t" and el.text]
            raw_val = "".join(parts)
        elif v_el is not None and v_el.text is not None:
            raw_val = v_el.text
            if ctype in ("", "n") and c_el.attrib.get("s", "") in date_styles:
                iso = excel_serial_to_iso(raw_val)
                if iso:
                    raw_val = iso

        raw_val = raw_val.strip()
        if raw_val:
            cells[coord] = raw_val

    return cells, skipped


def parse_csv_file(csv_path: Path) -> tuple[dict[str, str], int]:
    cells: dict[str, str] = {}
    skipped = 0
    with csv_path.open(newline="", encoding="utf-8") as f:
        for r_idx, row in enumerate(csv.reader(f), 1):
            if r_idx > MAX_ROWS:
                skipped += len(row)
                continue
            for c_idx, val in enumerate(row):
                v = val.strip()
                if not v:
                    continue
                if c_idx >= MAX_COLS:
                    skipped += 1
                    continue
                cells[f"{index_to_col(c_idx)}{r_idx}"] = v
    return cells, skipped


def write_txl(cells: dict[str, str], out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    entries = [f"(({escape_quoted(k)}:{escape_quoted(v)}),0)" for k, v in cells.items()]
    with out_path.open("w", encoding="utf-8") as f:
        f.write("[" + ",".join(entries) + "]\n")


def sanitize_filename(name: str) -> str:
    clean = re.sub(r"[^A-Za-z0-9_-]+", "_", name.strip()).strip("_")
    return clean.lower() or "sheet"


def main() -> int:
    ap = argparse.ArgumentParser(description="Convierte .xlsx o .csv a formato .txl de Texcel.")
    ap.add_argument("input", type=Path, help="Ruta al archivo .xlsx o .csv de entrada")
    ap.add_argument("-o", "--output", type=Path, help="Ruta del .txl de salida (por defecto output/sheets/<nombre>.txl)")
    ap.add_argument("--sheet", help="Nombre o índice (1-based) de la hoja del .xlsx a exportar")
    ap.add_argument("--all-sheets", action="store_true", help="Exporta todas las hojas visibles del .xlsx a output/sheets/")
    ap.add_argument("--list-sheets", action="store_true", help="Lista las hojas del .xlsx y termina")
    args = ap.parse_args()

    in_path: Path = args.input
    if not in_path.exists():
        raise SystemExit(f"No existe el archivo: {in_path}")

    if in_path.suffix.lower() == ".csv":
        cells, skipped = parse_csv_file(in_path)
        out_path = args.output or Path("output/sheets") / f"{sanitize_filename(in_path.stem)}.txl"
        write_txl(cells, out_path)
        print(f"Generado {out_path} ({len(cells)} celdas, omitidas={skipped})")
        return 0

    with zipfile.ZipFile(in_path) as zf:
        sheets = list_xlsx_sheets(zf)
        if args.list_sheets:
            for i, (name, target, vis) in enumerate(sheets, 1):
                print(f"{i}. {name} ({'visible' if vis else 'oculta'}) -> {target}")
            return 0

        shared = load_shared_strings(zf)
        date_styles = load_date_style_ids(zf)

        if args.all_sheets:
            for name, target, vis in sheets:
                if not vis:
                    continue
                cells, skipped = parse_xlsx_sheet(zf, target, shared, date_styles)
                out_path = Path("output/sheets") / f"{sanitize_filename(name)}.txl"
                write_txl(cells, out_path)
                print(f"Generado {out_path} [{name}] ({len(cells)} celdas, omitidas={skipped})")
            return 0

        chosen = None
        if args.sheet:
            if args.sheet.isdigit():
                idx = int(args.sheet) - 1
                if 0 <= idx < len(sheets):
                    chosen = sheets[idx]
            else:
                for s in sheets:
                    if s[0] == args.sheet:
                        chosen = s
                        break
            if not chosen:
                raise SystemExit(f"Hoja no encontrada: {args.sheet}")
        else:
            chosen = next((s for s in sheets if s[2]), sheets[0] if sheets else None)
            if not chosen:
                raise SystemExit("El libro .xlsx no contiene hojas.")

        name, target, _ = chosen
        cells, skipped = parse_xlsx_sheet(zf, target, shared, date_styles)
        out_path = args.output or Path("output/sheets") / f"{sanitize_filename(in_path.stem)}.txl"
        write_txl(cells, out_path)
        print(f"Generado {out_path} [{name}] ({len(cells)} celdas, omitidas={skipped})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
