/**
 * @file state.h
 * @ingroup texcel
 * @brief Estado compartido y helpers de geometría/persistencia para la TUI de Texcel.
 */

#ifndef __TEXCEL_HELPERS_STATE_H__
#define __TEXCEL_HELPERS_STATE_H__

#include "../engine.h"
#include "config.h"
#include "clipboard.h"
#include "loading_anim.h"
#include "text.h"
#include "../../types.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

/**
 * @enum SpreadsheetFocusMode
 * @brief Ventana activa en la interfaz dual: Navegación en Matriz o Consola de Comandos.
 */
enum class SpreadsheetFocusMode {
    GridNavigation = 0, /**< Foco en la Matriz: movimiento por flechas/WASD, Enter para editar. */
    CommandConsole = 1  /**< Foco en la Consola: escritura libre `= A1 + 4`, `5`, `B1 = A1 + 4`. */
};

namespace texcel_tui {

enum class SelectionKind : TX { None = 0, Cells = 1, Fill = 2, Text = 3 };
enum class ScrollDrag : uint8_t { None = 0, GridV, GridH, AstV, AstH };
enum class ModalKind : uint8_t { None = 0, Sheets, Functions, Help };

struct FunctionSpec {
    const char* name;
    const char* summary;
    const char* syntax;
    const char* detail;
    const char* example;
};

inline constexpr FunctionSpec FUNCTION_CATALOG[] = {
    {"SUM",      "Suma valores o rangos",     "SUM(rango | v1, v2, ...)",       "Suma todos los valores numéricos.",                 "= SUM(A1:A5)"},
    {"AVG",      "Promedio aritmético",       "AVG(rango | v1, v2, ...)",       "Calcula la media de las celdas numéricas.",         "= AVG(B1:B5)"},
    {"MIN",      "Valor mínimo",              "MIN(rango | v1, v2, ...)",       "Devuelve el menor valor numérico.",                 "= MIN(A1:A10)"},
    {"MAX",      "Valor máximo",              "MAX(rango | v1, v2, ...)",       "Devuelve el mayor valor numérico.",                 "= MAX(A1:A10)"},
    {"IF",       "Condición lógica",          "IF(cond, si_ok, si_no)",         "Evalúa una condición y elige uno de dos valores.",  "= IF(A1>=11, \"OK\", \"NO\")"},
    {"CONCAT",   "Une textos y valores",      "CONCAT(v1, v2, ...)",            "Concatena textos, números o rangos (también &).",   "= CONCAT(\"Total: \", D4)"},
    {"COUNTIF",  "Cuenta con 1 criterio",     "COUNTIF(rango, criterio)",       "Cuenta celdas que cumplen una condición.",          "= COUNTIF(A1:A10, \">5\")"},
    {"COUNTIFS", "Cuenta con varios criterios","COUNTIFS(r1, c1, r2, c2, ...)", "Cuenta celdas que cumplen todos los criterios.",    "= COUNTIFS(A1:A5, \">10\", B1:B5, \"OK\")"},
};
inline constexpr TX FUNCTION_CATALOG_SIZE = static_cast<TX>(sizeof(FUNCTION_CATALOG) / sizeof(FUNCTION_CATALOG[0]));

struct HelpEntry {
    const char* key;
    const char* desc;
    bool        is_section;
};

inline constexpr HelpEntry HELP_CATALOG[] = {
    {"Edición y fórmulas", "", true},
    {"Escribir",     "Escribe en cualquier celda",          false},
    {"=",            "Iniciar fórmula en la celda",         false},
    {"Tab",          "Alternar Hoja y Editor / Apuntar",    false},
    {"Ctrl+X F",     "Catálogo de funciones (o clic [fx])", false},
    {"Ctrl+U",       "Limpiar texto del editor",            false},
    {"Supr / Bksp",  "Borrar celda o rango seleccionado",   false},
    {"Navegación y selección", "", true},
    {"Ctrl+X G",     "Ir a celda o rango (o Ctrl+G)",       false},
    {"Shift+Flecha", "Seleccionar rango con teclado",       false},
    {"Ctrl+Clic",    "Autorrelleno (o arrastrar esquina)",  false},
    {"Ctrl+C",       "Copiar selección (o clic derecho)",   false},
    {"Hojas y vista", "", true},
    {"Ctrl+X H",     "Gestor de hojas (o Ctrl+O)",          false},
    {"Ctrl+X S",     "Guardar hoja actual (o Ctrl+S)",      false},
    {"■ V  □ H",     "Clic en Inspector para orientar AST", false},
    {"Ctrl+X A",     "Abrir esta ayuda (o clic en [?])",    false},
    {"Ctrl+D",       "Salir de Texcel (o clic en [Salir])", false},
    {"Comandos (en editor)", "", true},
    {"clear",        "Limpia todas las celdas de la hoja",  false},
    {"save | load",  "Guarda o recarga la hoja actual",     false},
    {"quit",         "Cierra Texcel",                       false},
};
inline constexpr TX HELP_CATALOG_SIZE = static_cast<TX>(sizeof(HELP_CATALOG) / sizeof(HELP_CATALOG[0]));

struct ModalRect {
    TX x, y, w, h, inner_w;
};

/**
 * @struct TuiState
 * @brief Estado completo de la sesión TUI compartido entre el renderer, el input handler y `SpreadsheetTUI`.
 */
struct TuiState {
    enum WinId : TX { WIN_GRID = 1, WIN_INSPECTOR = 2, WIN_CONSOLE = 3, WIN_ACTIVITY = 4 };
    enum ModalItem : TX { MODAL_AUTOSAVE = 0, MODAL_SAVE = 1, MODAL_NEW = 2, MODAL_FIRST_FILE = 3 };
    static constexpr TX MIN_TOTAL_WIDTH = 40;
    static constexpr TX DEFAULT_WIDTH   = 98;
    static constexpr TX TOTAL_HEIGHT    = 28;

    static constexpr TX LEFT_X  = 0;
    static constexpr TX TOP_Y   = 1;
    static constexpr TX TOP_H   = 16; // filas 1..16
    static constexpr TX BOT_Y   = 17;
    static constexpr TX BOT_H   = 10; // filas 17..26

    static constexpr TX VISIBLE_ROWS = 10; // 1..10 por vista + 1 fila vacía antes de la leyenda
    static constexpr TX CELL_WIDTH   = 9;
    static constexpr TX ROW_HEADER_W = 8;  // Ancho de la esquina superior izq. (A1:D10) y columna de filas
    static constexpr TX GRID_X0      = LEFT_X + 2 + ROW_HEADER_W;
    static constexpr TX GRID_Y0      = TOP_Y + 3;
    static constexpr TX AST_Y0       = TOP_Y + 8;

    SpreadsheetEngine&   engine;
    std::ostream&        os;
    std::ostream*        render_os = &os;
    SpreadsheetFocusMode mode = SpreadsheetFocusMode::GridNavigation;

    std::ostream& ros() { return *render_os; }

    // Geometría adaptativa al ancho real de la consola
    TX   total_width  = DEFAULT_WIDTH;
    TX   term_rows    = TOTAL_HEIGHT;
    TX   left_w       = 56;
    TX   right_x      = 57;
    TX   right_w      = 41;
    TX   visible_cols = 5;
    bool interactive_alt_screen = false;

    CellCoord cursor{0, 0, true};
    TX scroll_col = 0;
    TX scroll_row = 0;

    // Point Mode
    bool        point_mode = false;
    CellCoord   point_target{0, 0, true};
    std::string point_base;

    // Selección con ratón (SGR Mouse Tracking) confinada por ventana
    bool       mouse_dragging = false;
    ScrollDrag scroll_drag    = ScrollDrag::None;
    TX         drag_win       = 0;

    // Name Box interactivo en esquina superior izquierda (clic = ir a celda/rango, doble clic = A1)
    bool        namebox_editing = false;
    std::string namebox_buffer;
    std::chrono::steady_clock::time_point last_namebox_click{};

    // Navegador 2D del AST en el Inspector (Ventana 2)
    static constexpr TX AST_VIEW_H = 6; // rel_y = 8..13
    bool ast_horizontal = false;
    TX   ast_scroll_x   = 0;
    TX   ast_scroll_y   = 0;
    TX   ast_graph_w    = 0;
    TX   ast_graph_h    = 0;

    SelectionKind sel_kind = SelectionKind::None;
    CellCoord sel_a{0, 0, true};
    CellCoord sel_b{0, 0, true};
    TX   text_win       = 0;
    TX   tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    TX   last_anchor_win = 0;
    TX   last_anchor_x   = 0;
    TX   last_anchor_y   = 0;
    std::string win_lines[5][TOTAL_HEIGHT];

    // Consola e historial
    std::string input_buffer;
    size_t      input_cursor = 0;
    bool        return_to_grid_on_submit = false;
    bool        console_ref_inserting = false;
    size_t      console_ref_pos = 0;
    size_t      console_ref_len = 0;
    static constexpr TX FX_LIST_ROWS   = 6;  // Filas visibles de funciones en el modal fx
    static constexpr TX HELP_LIST_ROWS = 11; // Filas visibles de ayuda en el modal ?

    std::string status_banner;
    Vector<VectorAscTraits<std::string>> cmd_history;
    TX history_idx = -1;

    // Modales flotantes (Hojas, Funciones fx, Ayuda ?)
    TexcelConfig config;
    ModalKind    modal_kind     = ModalKind::None;
    bool         modal_new_mode = false;
    std::string  modal_new_name;
    TX           modal_idx      = 0;
    TX           fx_idx         = 0;
    TX           fx_scroll      = 0;
    TX           help_scroll    = 0;
    bool         chord_x        = false;
    bool         should_exit    = false;

    bool isModalOpen() const { return modal_kind != ModalKind::None; }
    bool isFxHelpModal() const { return modal_kind == ModalKind::Functions || modal_kind == ModalKind::Help; }

    void openModal(ModalKind kind) {
        modal_kind = kind;
        modal_new_mode = false;
        modal_new_name.clear();
        if (kind == ModalKind::Sheets) refreshSheetList();
        else if (kind == ModalKind::Functions) clampFxScroll();
        else if (kind == ModalKind::Help) clampHelpScroll();
    }

    void closeModal() {
        modal_kind = ModalKind::None;
        modal_new_mode = false;
        modal_new_name.clear();
    }

    ModalRect modalRect() const {
        TX w = std::min<TX>(62, total_width - 8);
        TX h = 16;
        TX x = (total_width - w) / 2;
        TX y = (TOTAL_HEIGHT - h) / 2;
        return {x, y, w, h, static_cast<TX>(w - 4)};
    }

    bool isFxButtonPos(TX mx, TX my) const {
        TX x1 = LEFT_X + left_w - 4;
        TX x0 = x1 - 3;
        return my == BOT_Y + 2 && mx >= x0 && mx <= x1;
    }

    bool isAstVerticalRadioPos(TX mx, TX my) const {
        TX x0 = right_x + 2 + 9;
        return my == TOP_Y + 6 && mx >= x0 && mx <= x0 + 2;
    }

    bool isAstHorizontalRadioPos(TX mx, TX my) const {
        TX x0 = right_x + 2 + 14;
        return my == TOP_Y + 6 && mx >= x0 && mx <= x0 + 2;
    }

    bool isHeaderHelpBtnPos(TX mx, TX my) const {
        return my == 0 && total_width >= 64 && mx >= total_width - 11 && mx <= total_width - 9;
    }

    bool isHeaderExitBtnPos(TX mx, TX my) const {
        return my == 0 && total_width >= 64 && mx >= total_width - 7 && mx < total_width;
    }

    void clampFxScroll() {
        fx_idx = std::clamp<TX>(fx_idx, 0, FUNCTION_CATALOG_SIZE - 1);
        TX max_scroll = std::max<TX>(0, FUNCTION_CATALOG_SIZE - FX_LIST_ROWS);
        if (fx_idx < fx_scroll) fx_scroll = fx_idx;
        if (fx_idx >= fx_scroll + FX_LIST_ROWS) fx_scroll = fx_idx - FX_LIST_ROWS + 1;
        fx_scroll = std::clamp<TX>(fx_scroll, 0, max_scroll);
    }

    void clampHelpScroll() {
        TX max_scroll = std::max<TX>(0, HELP_CATALOG_SIZE - HELP_LIST_ROWS);
        help_scroll = std::clamp<TX>(help_scroll, 0, max_scroll);
    }

    void insertSelectedFunction() {
        clampFxScroll();
        const FunctionSpec& fn = FUNCTION_CATALOG[fx_idx];
        std::string call = std::string(fn.name) + "(";
        if (mode != SpreadsheetFocusMode::CommandConsole) {
            exitPointMode();
            mode = SpreadsheetFocusMode::CommandConsole;
            return_to_grid_on_submit = true;
            setInput("= " + call);
        } else if (input_buffer.empty()) {
            setInput("= " + call);
        } else {
            input_cursor = std::min(input_cursor, input_buffer.size());
            input_buffer.insert(input_cursor, call);
            input_cursor += call.size();
        }
        closeModal();
        status_banner = std::string(fn.syntax);
    }

    TuiState(SpreadsheetEngine& eng, std::ostream& out) : engine(eng), os(out) {}

    static bool isFormulaBuffer(const std::string& s) {
        return s.find('=') != std::string::npos;
    }

    static bool canAcceptPointRef(const std::string& s) {
        size_t i = s.size();
        while (i > 0 && texcel_detail::isChar<std::isspace>(s[i - 1])) --i;
        if (i == 0) return false;
        char last = s[i - 1];
        return texcel_detail::isBinaryOpChar(last) || last == '(' || last == ':' || last == ',';
    }

    /** @brief Reemplaza el contenido del buffer de entrada y ubica el cursor al final (o lo limpia si `s` está vacío). */
    void setInput(const std::string& s = "") {
        input_buffer = s;
        input_cursor = input_buffer.size();
    }

    /** @brief Inserta un carácter en `input_cursor` respetando los límites del buffer. */
    void insertInputChar(char ch) {
        input_cursor = std::min(input_cursor, input_buffer.size());
        input_buffer.insert(input_cursor, 1, ch);
        ++input_cursor;
    }

    bool hasCellSel() const { return sel_kind == SelectionKind::Cells; }
    bool hasFillSel() const { return sel_kind == SelectionKind::Fill; }
    bool hasTextSel() const { return sel_kind == SelectionKind::Text; }
    void clearSelectionBanner() {
        if (status_banner.starts_with("Rango ") || status_banner.starts_with("Texto sombreado")) {
            status_banner.clear();
        }
    }
    void clearSelection() {
        sel_kind = SelectionKind::None;
        clearSelectionBanner();
    }

    void updatePointedCell() {
        if (!point_mode) return;
        setInput(point_base + cursor.toId());
    }

    /** @brief Indica si hay un arrastre activo de cualquier scrollbar. */
    bool isScrollDragging() const { return scroll_drag != ScrollDrag::None; }

    /** @brief Indica si hay algún arrastre activo (celda, texto o scrollbar). */
    bool isAnyDragging() const { return mouse_dragging || isScrollDragging(); }

    /** @brief Limpia el estado de arrastre de scrollbars. */
    void clearScrollDragging() { scroll_drag = ScrollDrag::None; }

    /** @brief Sale de Point Mode restaurando el cursor en la celda objetivo. */
    void exitPointMode(bool ensure_visible = false) {
        if (!point_mode) return;
        point_mode = false;
        cursor = point_target;
        if (ensure_visible) ensureCursorVisible();
    }

    /** @brief Inserta o reemplaza en vivo una referencia (`A1` o `A1:C3`) justo en `input_cursor` de la Consola. */
    void insertOrReplaceRefAtCursor(const std::string& ref) {
        if (!console_ref_inserting) {
            console_ref_inserting = true;
            console_ref_pos = std::min(input_cursor, input_buffer.size());
            console_ref_len = 0;
        }
        input_buffer.replace(console_ref_pos, console_ref_len, ref);
        console_ref_len = ref.size();
        input_cursor = console_ref_pos + console_ref_len;
    }

    /** @brief Copia la selección actual al portapapeles y actualiza `status_banner` si había texto. */
    bool copySelectionToClipboard(const std::string& banner = "✔ Seleccion copiada al portapapeles.") {
        std::string txt = extractCurrentSelectionText();
        if (txt.empty()) return false;
        TexcelClipboard::copy(os, txt);
        status_banner = banner;
        return true;
    }

    /** @brief Retorna el ID de la celda activa (o la celda objetivo si está en Point Mode). */
    std::string activeCellId() const {
        return (point_mode ? point_target : cursor).toId();
    }

    /** @brief Formatea el rango seleccionado normalizado (`A1:C3`, o `A1` si `collapse_single` y es 1 celda). */
    std::string selectedRangeStr(bool collapse_single = false) const {
        TX cmin, rmin, cmax, rmax;
        normalizeCellRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row, cmin, rmin, cmax, rmax);
        std::string id0 = CellCoord(cmin, rmin, true).toId();
        std::string id1 = CellCoord(cmax, rmax, true).toId();
        return (collapse_single && id0 == id1) ? id0 : (id0 + ":" + id1);
    }

    /** @brief Ejecuta Auto-Fill sobre la selección rectangular actual, autoguarda y actualiza el banner. */
    void applySelectionAutoFill() {
        TX filled = engine.autoFillRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row);
        maybeAutoSave();
        status_banner = "✔ Auto-Fill (" + std::to_string(filled) + " celdas) en " + selectedRangeStr();
    }

    std::string formatCellForInput(const std::string& cid) {
        return engine.getCell(cid).raw_expression;
    }

    void loadActiveCellIntoBuffer() {
        setInput(formatCellForInput(cursor.toId()));
    }

    std::string extractCurrentSelectionText() {
        if (hasCellSel()) {
            TX cmin, rmin, cmax, rmax;
            normalizeCellRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row, cmin, rmin, cmax, rmax);
            std::ostringstream tsv;
            for (TX r = rmin; r <= rmax; ++r) {
                if (r > rmin) tsv << "\n";
                for (TX c = cmin; c <= cmax; ++c) {
                    if (c > cmin) tsv << "\t";
                    tsv << engine.getCell(CellCoord(c, r, true).toId()).displayValue();
                }
            }
            return tsv.str();
        }
        if (hasTextSel() && text_win >= WIN_INSPECTOR && text_win <= WIN_ACTIVITY) {
            TX wx0, wy0, wx1, wy1;
            if (windowInnerBounds(text_win, wx0, wy0, wx1, wy1)) {
                TX sx0, sy0, sx1, sy1;
                normalizeTextRange(tx0, ty0, tx1, ty1, sx0, sy0, sx1, sy1);
                std::ostringstream copied;
                bool first_line = true;
                for (TX y = sy0; y <= sy1 && y < TOTAL_HEIGHT; ++y) {
                    if (!isSelectableWindowRow(text_win, y)) continue;
                    const std::string& line = win_lines[text_win][y];
                    size_t line_cp = visible_length(line);
                    TX c0 = (y == sy0) ? std::max<TX>(0, sx0 - wx0) : 0;
                    TX c1 = (y == sy1) ? std::max<TX>(0, sx1 - wx0) : static_cast<TX>(line_cp);
                    if (c0 <= c1 && static_cast<size_t>(c0) < line_cp) {
                        std::string slice = utf8Slice(line, static_cast<size_t>(c0), static_cast<size_t>(c1) + 1);
                        std::string clean = stripDividerGlyphs(slice);
                        if (!first_line) copied << "\n";
                        copied << clean;
                        first_line = false;
                    }
                }
                return copied.str();
            }
        }
        return engine.getCell(cursor.toId()).displayValue();
    }

    bool windowInnerBounds(TX win_id, TX& x0, TX& y0, TX& x1, TX& y1) const {
        if (win_id == WIN_INSPECTOR) { x0 = right_x + 2; y0 = TOP_Y + 1; x1 = right_x + right_w - 3; y1 = TOP_Y + TOP_H - 2; return true; }
        if (win_id == WIN_CONSOLE)   { x0 = LEFT_X + 2;  y0 = BOT_Y + 1; x1 = LEFT_X + left_w - 3;   y1 = BOT_Y + BOT_H - 2; return true; }
        if (win_id == WIN_ACTIVITY)  { x0 = right_x + 2; y0 = BOT_Y + 1; x1 = right_x + right_w - 3; y1 = BOT_Y + BOT_H - 2; return true; }
        return false;
    }

    /** @brief Verifica si una fila dentro de una ventana es seleccionable (excluye barras divisorias horizontales). */
    bool isSelectableWindowRow(TX win_id, TX abs_y) const {
        if (win_id == WIN_INSPECTOR) return abs_y != TOP_Y + 6 && abs_y != TOP_Y + 9;
        if (win_id == WIN_CONSOLE)   return abs_y != BOT_Y + 1 && abs_y != BOT_Y + 3;
        return true;
    }

    /** @brief Verifica si una coordenada (mx, my) dentro de `win_id` cae sobre texto seleccionable (no sobre una barra `│` o `─`). */
    bool isSelectableWindowPos(TX win_id, TX mx, TX my) const {
        if (!isSelectableWindowRow(win_id, my)) return false;
        TX wx0, wy0, wx1, wy1;
        if (!windowInnerBounds(win_id, wx0, wy0, wx1, wy1)) return false;
        if (mx < wx0 || mx > wx1 || my < wy0 || my > wy1) return false;
        if (my >= 0 && my < TOTAL_HEIGHT) {
            const std::string& line = win_lines[win_id][my];
            size_t cp_idx = static_cast<size_t>(mx - wx0);
            std::string g = utf8At(line, cp_idx);
            if (isDividerGlyph(g)) return false;
        }
        return true;
    }

    bool screenToGridCell(TX mx, TX my, TX& out_col, TX& out_row, bool clamp_to_grid) const {
        TX grid_x1 = GRID_X0 + visible_cols * (CELL_WIDTH + 1) - 1;
        TX grid_y1 = GRID_Y0 + VISIBLE_ROWS - 1;

        if (!clamp_to_grid) {
            if (mx < GRID_X0 || mx > grid_x1 || my < GRID_Y0 || my > grid_y1) return false;
            // Excluir las barras divisorias verticales '│' entre columnas de celdas
            if ((mx - GRID_X0) % (CELL_WIDTH + 1) == 0) return false;
        } else {
            mx = std::clamp(mx, GRID_X0, grid_x1);
            my = std::clamp(my, GRID_Y0, grid_y1);
        }

        TX rel_c = (mx - GRID_X0) / (CELL_WIDTH + 1);
        TX rel_r = my - GRID_Y0;
        out_col = std::clamp(scroll_col + rel_c, static_cast<TX>(0), CellCoord::MAX_COLS - 1);
        out_row = std::clamp(scroll_row + rel_r, static_cast<TX>(0), CellCoord::MAX_ROWS - 1);
        return true;
    }

    bool updateLayout() {
        TX cols = DEFAULT_WIDTH, rows = TOTAL_HEIGHT;
        if (isatty(STDOUT_FILENO)) {
            struct winsize ws{};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
                cols = static_cast<TX>(ws.ws_col);
                if (ws.ws_row > 0) rows = static_cast<TX>(ws.ws_row);
            }
        }
        TX new_total_w = std::max(MIN_TOTAL_WIDTH, cols);
        bool changed = (new_total_w != total_width) || (rows != term_rows);

        term_rows    = rows;
        total_width  = new_total_w;
        right_w      = std::clamp((total_width * 38) / 100, static_cast<TX>(26), static_cast<TX>(68));
        left_w       = total_width - right_w - 1;
        right_x      = left_w + 1;
        TX avail_grid = std::max(static_cast<TX>(14), left_w - (4 + ROW_HEADER_W));
        visible_cols = std::clamp(avail_grid / (CELL_WIDTH + 1), static_cast<TX>(1), CellCoord::MAX_COLS);
        return changed;
    }

    void ensureCursorVisible() {
        if (cursor.col < scroll_col) scroll_col = cursor.col;
        if (cursor.col >= scroll_col + visible_cols) scroll_col = cursor.col - visible_cols + 1;
        if (cursor.row < scroll_row) scroll_row = cursor.row;
        if (cursor.row >= scroll_row + VISIBLE_ROWS) scroll_row = cursor.row - VISIBLE_ROWS + 1;
    }

    void clampScroll() {
        scroll_col = std::clamp<TX>(scroll_col, 0, std::max<TX>(0, CellCoord::MAX_COLS - visible_cols));
        scroll_row = std::clamp<TX>(scroll_row, 0, std::max<TX>(0, CellCoord::MAX_ROWS - VISIBLE_ROWS));
    }

    void clearSelectedCells() {
        if (hasCellSel()) {
            std::string r_str = selectedRangeStr();
            engine.clearRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row);
            clearSelection();
            maybeAutoSave();
            status_banner = "Rango " + r_str + " borrado.";
        } else {
            std::string cid = cursor.toId();
            engine.setCell(cid, "");
            maybeAutoSave();
            status_banner = "Celda " + cid + " borrada.";
        }
    }

    bool isNameBoxPos(TX mx, TX my) const {
        return mx >= LEFT_X + 2 && mx < LEFT_X + 2 + ROW_HEADER_W && my == TOP_Y + 1;
    }

    void beginNameBoxEdit() {
        namebox_editing = true;
        namebox_buffer.clear();
        mode = SpreadsheetFocusMode::GridNavigation;
        status_banner = "Ir a celda o rango (ej. ZZ500 o A1:C5) y pulsa [Enter].";
    }

    void jumpToHome() {
        cursor = {0, 0, true};
        scroll_col = 0;
        scroll_row = 0;
        clearSelection();
        point_mode = false;
        status_banner = "Salto al inicio (A1).";
    }

    bool goToCellOrRange(const std::string& raw) {
        std::string s = texcel_detail::trim(raw);
        if (s.empty()) return false;
        size_t colon = s.find(':');
        if (colon != std::string::npos) {
            CellCoord a = CellCoord::parse(s.substr(0, colon));
            CellCoord b = CellCoord::parse(s.substr(colon + 1));
            if (!a.valid || !b.valid) {
                status_banner = "Rango invalido: " + s;
                return false;
            }
            sel_kind = SelectionKind::Cells;
            sel_a = a;
            sel_b = b;
            cursor = a;
            ensureCursorVisible();
            status_banner = "Seleccionado rango " + a.toId() + ":" + b.toId();
            return true;
        }
        CellCoord c = CellCoord::parse(s);
        if (!c.valid) {
            status_banner = "Celda invalida: " + s;
            return false;
        }
        clearSelection();
        cursor = c;
        ensureCursorVisible();
        status_banner = "Posicionada en " + c.toId();
        return true;
    }

    static TX mapScrollTrack(TX pos, TX track_0, TX track_len, TX max_scroll) {
        TX len = std::max<TX>(1, track_len);
        TX rel = std::clamp<TX>(pos - track_0, 0, len);
        return (rel * std::max<TX>(0, max_scroll)) / len;
    }

    void scrollVerticalToScreenY(TX my) {
        scroll_row = mapScrollTrack(my, GRID_Y0, VISIBLE_ROWS - 1, CellCoord::MAX_ROWS - VISIBLE_ROWS);
        clampScroll();
    }

    void scrollHorizontalToScreenX(TX mx) {
        TX track_x0 = GRID_X0 + 1;
        scroll_col = mapScrollTrack(mx, track_x0, (LEFT_X + left_w - 2) - track_x0, CellCoord::MAX_COLS - visible_cols);
        clampScroll();
    }

    TX astViewWidth() const {
        return std::max<TX>(1, right_w - 4);
    }

    void clampAstScroll() {
        TX vw = astViewWidth();
        TX max_x = std::max<TX>(0, ast_graph_w - vw);
        TX max_y = std::max<TX>(0, ast_graph_h - AST_VIEW_H);
        ast_scroll_x = std::clamp<TX>(ast_scroll_x, 0, max_x);
        ast_scroll_y = std::clamp<TX>(ast_scroll_y, 0, max_y);
    }

    void resetAstScroll() {
        ast_scroll_x = 0;
        ast_scroll_y = 0;
    }

    void scrollAstVerticalToY(TX my) {
        ast_scroll_y = mapScrollTrack(my, AST_Y0, AST_VIEW_H - 1, ast_graph_h - AST_VIEW_H);
        clampAstScroll();
    }

    void scrollAstHorizontalToX(TX mx) {
        TX track_x0 = right_x + 2;
        ast_scroll_x = mapScrollTrack(mx, track_x0, (right_x + right_w - 3) - track_x0, ast_graph_w - astViewWidth());
        clampAstScroll();
    }

    bool dragScrollbarTo(TX mx, TX my) {
        switch (scroll_drag) {
            case ScrollDrag::GridV: { TX p = scroll_row;   scrollVerticalToScreenY(my);   return scroll_row != p; }
            case ScrollDrag::GridH: { TX p = scroll_col;   scrollHorizontalToScreenX(mx); return scroll_col != p; }
            case ScrollDrag::AstV:  { TX p = ast_scroll_y; scrollAstVerticalToY(my);      return ast_scroll_y != p; }
            case ScrollDrag::AstH:  { TX p = ast_scroll_x; scrollAstHorizontalToX(mx);    return ast_scroll_x != p; }
            default: return false;
        }
    }

    bool tryStartScrollbarDrag(TX mx, TX my) {
        if (mx == LEFT_X + left_w - 1 && my >= GRID_Y0 && my < GRID_Y0 + VISIBLE_ROWS) {
            scroll_drag = ScrollDrag::GridV;
        } else if (my == TOP_Y + TOP_H - 1 && mx >= GRID_X0 + 1 && mx <= LEFT_X + left_w - 2) {
            scroll_drag = ScrollDrag::GridH;
        } else if (ast_graph_h > AST_VIEW_H && mx == right_x + right_w - 1 &&
                   my >= AST_Y0 && my < AST_Y0 + AST_VIEW_H) {
            scroll_drag = ScrollDrag::AstV;
        } else if (ast_graph_w > astViewWidth() && my == TOP_Y + TOP_H - 1 &&
                   mx >= right_x + 2 && mx <= right_x + right_w - 3) {
            scroll_drag = ScrollDrag::AstH;
        } else {
            return false;
        }
        dragScrollbarTo(mx, my);
        return true;
    }

    TX modalMaxIdx() const {
        return MODAL_NEW + static_cast<TX>(config.sheet_files.size());
    }

    void refreshSheetList() {
        config.refreshSheetList();
        modal_idx = std::clamp<TX>(modal_idx, 0, modalMaxIdx());
    }

    void saveSheet(const std::string& name, bool silent = false) {
        config.ensureSheetsDir();
        std::filesystem::path p = config.sheetPath(name);
        std::ofstream ofs(p, std::ios::trunc);
        if (ofs) {
            ofs << engine.rawTable() << std::endl;
            config.current_sheet = name;
            config.saveConfig();
            if (!silent) {
                engine.addLog("💾 Guardada hoja: " + name + TexcelConfig::SHEET_EXT);
                status_banner = "Hoja '" + name + "' guardada en " + p.string();
            }
        } else if (!silent) {
            engine.addLog("✖ Error al guardar " + name + TexcelConfig::SHEET_EXT);
            status_banner = "No se pudo guardar '" + name + "'.";
        }
    }

    bool loadSheet(const std::string& name) {
        std::ifstream ifs(config.sheetPath(name));
        if (!ifs) {
            engine.addLog("✖ No existe " + name + TexcelConfig::SHEET_EXT);
            status_banner = "Archivo '" + name + TexcelConfig::SHEET_EXT + "' no encontrado.";
            return false;
        }
        TexcelLoadingSpinner<TuiState> spinner(*this, "Leyendo \"" + name + TexcelConfig::SHEET_EXT + "\"…");
        engine.clear();
        ifs >> engine.rawTable();
        spinner.setLabel("Evaluando fórmulas…");
        engine.recalculateAll();
        config.current_sheet = name;
        config.saveConfig();
        engine.addLog("📂 Cargada hoja: " + name + TexcelConfig::SHEET_EXT);
        status_banner = "Hoja '" + name + "' cargada correctamente.";
        return true;
    }

    void maybeAutoSave() {
        if (config.autosave) saveSheet(config.current_sheet, true);
    }

    void deleteSelectedModalSheet() {
        if (modal_idx < MODAL_FIRST_FILE) return;
        TX fidx = modal_idx - MODAL_FIRST_FILE;
        if (fidx < static_cast<TX>(config.sheet_files.size())) {
            std::string target = config.sheet_files[static_cast<size_t>(fidx)].getValue();
            std::error_code ec;
            std::filesystem::remove(config.sheetPath(target), ec);
            engine.addLog("🗑 Eliminada hoja: " + target + TexcelConfig::SHEET_EXT);
            refreshSheetList();
        }
    }
};

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_TUI_STATE_H__
