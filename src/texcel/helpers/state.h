/**
 * @file state.h
 * @ingroup texcel
 * @brief Estado modularizado, layout 2D adaptativo, Undo/Redo, Copy/Paste relativo, búsqueda y HitMap para Texcel.
 */

#ifndef __TEXCEL_HELPERS_STATE_H__
#define __TEXCEL_HELPERS_STATE_H__

#include "../engine.h"
#include "config.h"
#include "clipboard.h"
#include "loading_anim.h"
#include "text.h"
#include "../../types.h"
#include "../../utilities/layouts/visual_graph.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

/**
 * @enum SpreadsheetFocusMode
 * @brief Ventana activa en la interfaz dual: Navegación en Matriz o Consola de Comandos.
 */
enum class SpreadsheetFocusMode {
    GridNavigation = 0, /**< Foco en la Matriz: movimiento por flechas, escritura directa y atajos con modificador. */
    CommandConsole = 1  /**< Foco en la Consola: edición de expresión/fórmula o comandos administrativos. */
};

namespace texcel_tui {

enum class SelectionKind : TX { None = 0, Cells = 1, Fill = 2, Text = 3 };
enum class ScrollDrag : uint8_t { None = 0, GridV, GridH, AstV, AstH, ColResize };
enum class ModalKind : uint8_t { None = 0, Sheets, Functions, Help };

// Catálogo de funciones expuesto a la TUI
using FunctionSpec = TexcelFuncInfo;
inline constexpr const auto& FUNCTION_CATALOG = TEXCEL_FUNCTIONS;
inline constexpr TX FUNCTION_CATALOG_SIZE = TEXCEL_FUNCTIONS_COUNT;

struct HelpEntry {
    const char* key;
    const char* desc;
    bool        is_section;
};

inline constexpr HelpEntry HELP_CATALOG[] = {
    {"Edición y fórmulas", "", true},
    {"Escribir",     "Escribe directo en la celda activa",   false},
    {"= / F2 / Enter","Iniciar fórmula o editar contenido",  false},
    {"Tab",          "Alternar Hoja y Editor / Apuntar",     false},
    {"Ctrl+F",       "Catálogo de funciones (con filtro)",   false},
    {"Ctrl+Z / Y",   "Deshacer / Rehacer operación",         false},
    {"Ctrl+U",       "Limpiar texto del editor",             false},
    {"Supr / Bksp",  "Borrar celda o rango seleccionado",    false},
    {"Navegación y selección", "", true},
    {"Ctrl+G",       "Ir a celda o rango (ej. A1:D10)",      false},
    {"Ctrl+B",       "Buscar texto o valor en la hoja",      false},
    {"Ctrl+Flecha",  "Saltar al borde de datos contiguos",   false},
    {"RePág / AvPág","Desplazar una página arriba/abajo",    false},
    {"Shift+Flecha", "Seleccionar rango (también Apuntar)",  false},
    {"Ctrl+C / V",   "Copiar / Pegar (desplaza fórmulas)",   false},
    {"Ctrl+X K",     "Cortar celda o rango seleccionado",    false},
    {"Ctrl+Clic",    "Autorrelleno (o arrastrar esquina)",   false},
    {"Columnas, hojas y vista", "", true},
    {"Alt+← / Alt+→","Estrechar / Ensanchar columna activa", false},
    {"Ctrl+X =",     "Auto-ajustar ancho de columna(s)",     false},
    {"Ctrl+O",       "Gestor de hojas (o clic en archivo)",  false},
    {"Ctrl+S",       "Guardar hoja actual (atómico)",        false},
    {"■ V  □ H",     "Clic en Inspector para orientar AST",  false},
    {"Ctrl+D",       "Salir de Texcel (o clic en [Salir])",  false},
    {"Comandos (en editor)", "", true},
    {"autofit",      "Auto-ajusta todas las columnas",       false},
    {"undo | redo",  "Deshace o rehace el último cambio",    false},
    {"clear",        "Limpia todas las celdas (con Undo)",   false},
    {"save | load",  "Guarda o recarga la hoja actual",      false},
    {"quit",         "Cierra Texcel",                        false},
};
inline constexpr TX HELP_CATALOG_SIZE = static_cast<TX>(sizeof(HELP_CATALOG) / sizeof(HELP_CATALOG[0]));

struct UiRect {
    TX x = 0, y = 0, w = 0, h = 0;
    bool contains(TX mx, TX my) const {
        return w > 0 && h > 0 && mx >= x && mx < x + w && my >= y && my < y + h;
    }
};

struct ModalRect {
    TX x, y, w, h, inner_w;
};

/** @brief Áreas rectangulares clicables de la barra superior, paneles e Inspector. */
struct HitMap {
    UiRect header_sheet_btn;
    UiRect header_auto_btn;
    UiRect header_help_btn;
    UiRect header_exit_btn;
    UiRect namebox;
    UiRect fx_btn;
    UiRect ast_v_radio;
    UiRect ast_h_radio;
    UiRect win_inner[5];
};

/** @brief Estadísticas instantáneas sobre un rango seleccionado (estilo barra inferior de Excel). */
struct RangeQuickStats {
    bool   has_cells = false;
    TX     non_empty = 0;
    TX     num_count = 0;
    double sum = 0.0;
    double min_val = 0.0;
    double max_val = 0.0;
};

/** @brief Pila transaccional de Undo / Redo basada en parches diferenciales por celda. */
struct UndoManager {
    static constexpr size_t MAX_HISTORY = 100;

    struct CellPatch {
        std::string cid;
        std::string before_raw;
        std::string after_raw;
    };

    struct Transaction {
        std::string            label;
        CellCoord              cursor_before;
        CellCoord              cursor_after;
        std::vector<CellPatch> patches;
    };

    std::vector<Transaction> undo_stack;
    std::vector<Transaction> redo_stack;

    void clear() {
        undo_stack.clear();
        redo_stack.clear();
    }

    void push(Transaction tx) {
        if (tx.patches.empty()) return;
        if (undo_stack.size() >= MAX_HISTORY) {
            undo_stack.erase(undo_stack.begin());
        }
        undo_stack.push_back(std::move(tx));
        redo_stack.clear();
    }
};

/** @brief Portapapeles estructurado interno de celdas/rangos con soporte de fórmulas relativas y modo Cortar. */
struct CellClipboard {
    bool                     active = false;
    bool                     is_cut = false;
    TX                       src_c0 = 0;
    TX                       src_r0 = 0;
    TX                       cols   = 0;
    TX                       rows   = 0;
    std::vector<std::string> raw_cells;
    std::string              tsv_snapshot;

    void clear() {
        active = false;
        is_cut = false;
        cols = rows = 0;
        raw_cells.clear();
        tsv_snapshot.clear();
    }
};

/** @brief Estado de búsqueda interactiva (`Ctrl+B` / `Ctrl+X B`) sobre celdas activas. */
struct SearchState {
    bool                   active = false;
    std::string            query;
    std::vector<CellCoord> matches;
    TX                     match_idx = 0;

    void close() {
        active = false;
        query.clear();
        matches.clear();
        match_idx = 0;
    }
};

/** @brief Caché del grafo visual del AST en el Inspector para evitar re-parseo por cada frame. */
struct AstCache {
    std::string expr;
    bool        horizontal = false;
    bool        valid = false;
    VisualGraph graph;

    void invalidate() {
        valid = false;
        expr.clear();
        graph = VisualGraph{};
    }
};

/**
 * @struct TuiState
 * @brief Estado compartido de la sesión TUI con layout 2D dinámico y subsistemas desacoplados.
 */
struct TuiState {
    enum WinId : TX { WIN_GRID = 1, WIN_INSPECTOR = 2, WIN_CONSOLE = 3, WIN_ACTIVITY = 4 };
    enum ModalItem : TX { MODAL_AUTOSAVE = 0, MODAL_SAVE = 1, MODAL_NEW = 2, MODAL_FIRST_FILE = 3 };

    static constexpr TX MIN_TOTAL_WIDTH = 44;
    static constexpr TX MIN_TOTAL_ROWS  = 24;
    static constexpr TX DEFAULT_WIDTH   = 98;
    static constexpr TX TOTAL_HEIGHT    = 28; // Altura base por defecto

    static constexpr TX LEFT_X  = 0;
    static constexpr TX TOP_Y   = 1;
    static constexpr TX TOP_H   = 16; // Altura superior base
    static constexpr TX BOT_Y   = 17; // Coordenada Y inferior base
    static constexpr TX BOT_H   = 10; // Altura inferior base

    static constexpr TX VISIBLE_ROWS = 10; // Filas visibles base
    static constexpr TX CELL_WIDTH   = TexcelConfig::DEFAULT_COL_WIDTH;
    static constexpr TX ROW_HEADER_W = 8;
    static constexpr TX GRID_X0      = LEFT_X + 2 + ROW_HEADER_W;
    static constexpr TX GRID_Y0      = TOP_Y + 3;
    static constexpr TX AST_Y0       = TOP_Y + 8;
    static constexpr TX AST_VIEW_H   = 6;  // Altura AST base
    static constexpr TX FX_LIST_ROWS   = 6;
    static constexpr TX HELP_LIST_ROWS = 11;

    SpreadsheetEngine&   engine;
    std::ostream&        os;
    std::ostream*        render_os = &os;
    std::mutex           output_mtx;
    SpreadsheetFocusMode mode = SpreadsheetFocusMode::GridNavigation;

    std::ostream& ros() { return *render_os; }

    // Geometría 2D adaptativa (horizontal + vertical)
    TX   total_width  = DEFAULT_WIDTH;
    TX   term_rows    = TOTAL_HEIGHT;
    TX   total_height = TOTAL_HEIGHT;
    TX   top_h        = TOP_H;
    TX   bot_y        = BOT_Y;
    TX   bot_h        = BOT_H;
    TX   visible_rows = VISIBLE_ROWS;
    TX   ast_view_h   = AST_VIEW_H;
    TX   left_w       = 56;
    TX   right_x      = 57;
    TX   right_w      = 41;
    TX   visible_cols = 5;
    bool interactive_alt_screen = false;
    bool was_modal_open = false;
    bool dirty_unsaved  = false;
    bool confirm_discard = false;

    // Búfer de un byte de adelanto para teclas que siguen inmediatamente a ESC
    bool has_pending_key = false;
    char pending_key     = 0;

    HitMap hits;

    // Cursor y desplazamiento de la Matriz
    CellCoord cursor{0, 0, true};
    TX scroll_col = 0;
    TX scroll_row = 0;

    // Point Mode (con soporte de ancla para rangos Shift+Flecha)
    bool        point_mode = false;
    CellCoord   point_target{0, 0, true};
    CellCoord   point_anchor{0, 0, true};
    bool        point_has_range = false;
    std::string point_base;

    // Interacción SGR con ratón y redimensionamiento de columnas
    bool       mouse_dragging = false;
    ScrollDrag scroll_drag    = ScrollDrag::None;
    TX         drag_win       = 0;
    TX         resize_col     = 0;
    TX         resize_start_x = 0;
    TX         resize_start_w = CELL_WIDTH;
    TX         last_div_col   = -1;
    std::chrono::steady_clock::time_point last_div_click{};

    // Name Box interactivo
    bool        namebox_editing = false;
    std::string namebox_buffer;
    std::chrono::steady_clock::time_point last_namebox_click{};

    // Navegador 2D del AST y caché
    bool     ast_horizontal = false;
    TX       ast_scroll_x   = 0;
    TX       ast_scroll_y   = 0;
    TX       ast_graph_w    = 0;
    TX       ast_graph_h    = 0;
    AstCache ast_cache;

    // Selecciones (celdas, autorrelleno o texto de paneles)
    SelectionKind sel_kind = SelectionKind::None;
    CellCoord     sel_a{0, 0, true};
    CellCoord     sel_b{0, 0, true};
    TX            text_win = 0;
    TX            tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    TX            last_anchor_win = 0;
    TX            last_anchor_x   = 0;
    TX            last_anchor_y   = 0;
    std::vector<std::string> win_lines[5];

    // Consola e historial
    std::string input_buffer;
    size_t      input_cursor = 0;
    bool        return_to_grid_on_submit = false;
    bool        console_ref_inserting = false;
    size_t      console_ref_pos = 0;
    size_t      console_ref_len = 0;

    std::string status_banner;
    Vector<VectorAscTraits<std::string>> cmd_history;
    TX history_idx = -1;

    // Subsistemas de productividad
    TexcelConfig  config;
    UndoManager   undo_mgr;
    CellClipboard cell_clip;
    SearchState   search;

    // Modales flotantes (con filtro en vivo para Funciones)
    ModalKind   modal_kind     = ModalKind::None;
    bool        modal_new_mode = false;
    std::string modal_new_name;
    std::string modal_filter;
    TX          modal_idx      = 0;
    TX          fx_idx         = 0;
    TX          fx_scroll      = 0;
    TX          help_scroll    = 0;
    bool        chord_x        = false;
    bool        should_exit    = false;

    TuiState(SpreadsheetEngine& eng, std::ostream& out) : engine(eng), os(out) {
        resizeWinLines(TOTAL_HEIGHT);
        rebuildHitMap();
    }

    void resizeWinLines(TX rows) {
        size_t n = static_cast<size_t>(std::max<TX>(TOTAL_HEIGHT, rows));
        for (auto& wl : win_lines) {
            if (wl.size() != n) wl.assign(n, "");
        }
    }

    TX colWidth(TX col) const {
        return config.activeColWidth(col, CELL_WIDTH);
    }

    void recomputeVisibleCols() {
        TX avail_grid = std::max<TX>(10, left_w - (4 + ROW_HEADER_W));
        TX used = 0;
        TX cnt = 0;
        for (TX c = scroll_col; c < CellCoord::MAX_COLS; ++c) {
            TX need = colWidth(c) + 1;
            if (cnt > 0 && used + need > avail_grid) break;
            used += need;
            ++cnt;
            if (used >= avail_grid) break;
        }
        visible_cols = std::max<TX>(1, cnt);
    }

    void rebuildHitMap() {
        hits = HitMap{};
        hits.namebox = {LEFT_X + 2, TOP_Y + 1, ROW_HEADER_W, 1};
        hits.fx_btn  = {LEFT_X + left_w - 7, bot_y + 2, 4, 1};
        hits.ast_v_radio = {right_x + 2 + 9, TOP_Y + 6, 3, 1};
        hits.ast_h_radio = {right_x + 2 + 14, TOP_Y + 6, 3, 1};
        if (total_width >= 64) {
            hits.header_help_btn = {total_width - 11, 0, 3, 1};
            hits.header_exit_btn = {total_width - 7, 0, 7, 1};
        }
        std::string sheet_lbl = config.current_sheet + TexcelConfig::SHEET_EXT + (dirty_unsaved && !config.autosave ? " ●" : "");
        TX sheet_x = 25; // Tras " ◆ Texcel " (10) + " " + badge modo (11) + " │ " (3) = 25
        TX sheet_w = static_cast<TX>(visible_length(sheet_lbl));
        hits.header_sheet_btn = {sheet_x, 0, sheet_w, 1};
        hits.header_auto_btn  = {sheet_x + sheet_w + 1, 0, 10, 1};

        hits.win_inner[WIN_INSPECTOR] = {right_x + 2, TOP_Y + 1, std::max<TX>(0, right_w - 4), std::max<TX>(0, top_h - 2)};
        hits.win_inner[WIN_CONSOLE]   = {LEFT_X + 2,  bot_y + 1, std::max<TX>(0, left_w - 4),  std::max<TX>(0, bot_h - 2)};
        hits.win_inner[WIN_ACTIVITY]  = {right_x + 2, bot_y + 1, std::max<TX>(0, right_w - 4), std::max<TX>(0, bot_h - 2)};
    }

    bool isModalOpen() const { return modal_kind != ModalKind::None; }
    bool isFxHelpModal() const { return modal_kind == ModalKind::Functions || modal_kind == ModalKind::Help; }

    std::vector<TX> filteredFunctionIndices() const {
        std::vector<TX> out;
        out.reserve(static_cast<size_t>(FUNCTION_CATALOG_SIZE));
        std::string q = texcel_detail::toUpper(texcel_detail::trim(modal_filter));
        for (TX i = 0; i < FUNCTION_CATALOG_SIZE; ++i) {
            if (q.empty()) {
                out.push_back(i);
                continue;
            }
            std::string nm = texcel_detail::toUpper(FUNCTION_CATALOG[i].name);
            std::string sm = texcel_detail::toUpper(FUNCTION_CATALOG[i].summary);
            if (nm.find(q) != std::string::npos || sm.find(q) != std::string::npos) {
                out.push_back(i);
            }
        }
        return out;
    }

    void openModal(ModalKind kind) {
        modal_kind = kind;
        modal_new_mode = false;
        modal_new_name.clear();
        modal_filter.clear();
        confirm_discard = false;
        if (kind == ModalKind::Sheets) refreshSheetList();
        else if (kind == ModalKind::Functions) clampFxScroll();
        else if (kind == ModalKind::Help) clampHelpScroll();
    }

    void closeModal() {
        if (modal_kind != ModalKind::None) was_modal_open = true;
        modal_kind = ModalKind::None;
        modal_new_mode = false;
        modal_new_name.clear();
        modal_filter.clear();
    }

    ModalRect modalRect() const {
        TX w = std::min<TX>(64, total_width - 6);
        TX h = 16;
        TX x = (total_width - w) / 2;
        TX y = std::max<TX>(1, (total_height - h) / 2);
        return {x, y, w, h, static_cast<TX>(w - 4)};
    }

    bool isFxButtonPos(TX mx, TX my) const { return hits.fx_btn.contains(mx, my); }
    bool isAstVerticalRadioPos(TX mx, TX my) const { return hits.ast_v_radio.contains(mx, my); }
    bool isAstHorizontalRadioPos(TX mx, TX my) const { return hits.ast_h_radio.contains(mx, my); }
    bool isHeaderHelpBtnPos(TX mx, TX my) const { return hits.header_help_btn.contains(mx, my); }
    bool isHeaderExitBtnPos(TX mx, TX my) const { return hits.header_exit_btn.contains(mx, my); }
    bool isNameBoxPos(TX mx, TX my) const { return hits.namebox.contains(mx, my); }

    void clampFxScroll() {
        auto filtered = filteredFunctionIndices();
        TX cnt = static_cast<TX>(filtered.size());
        if (cnt <= 0) {
            fx_idx = 0;
            fx_scroll = 0;
            return;
        }
        fx_idx = std::clamp<TX>(fx_idx, 0, cnt - 1);
        TX max_scroll = std::max<TX>(0, cnt - FX_LIST_ROWS);
        if (fx_idx < fx_scroll) fx_scroll = fx_idx;
        if (fx_idx >= fx_scroll + FX_LIST_ROWS) fx_scroll = fx_idx - FX_LIST_ROWS + 1;
        fx_scroll = std::clamp<TX>(fx_scroll, 0, max_scroll);
    }

    void clampHelpScroll() {
        TX max_scroll = std::max<TX>(0, HELP_CATALOG_SIZE - HELP_LIST_ROWS);
        help_scroll = std::clamp<TX>(help_scroll, 0, max_scroll);
    }

    void insertSelectedFunction() {
        auto filtered = filteredFunctionIndices();
        if (filtered.empty()) return;
        clampFxScroll();
        const FunctionSpec& fn = FUNCTION_CATALOG[filtered[static_cast<size_t>(fx_idx)]];
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

    void setInput(const std::string& s = "") {
        input_buffer = s;
        input_cursor = input_buffer.size();
    }

    void insertInputChar(char ch) {
        input_cursor = std::min(input_cursor, input_buffer.size());
        input_buffer.insert(input_cursor, 1, ch);
        ++input_cursor;
    }

    bool hasCellSel() const { return sel_kind == SelectionKind::Cells; }
    bool hasFillSel() const { return sel_kind == SelectionKind::Fill; }
    bool hasTextSel() const { return sel_kind == SelectionKind::Text; }
    void clearSelectionBanner() {
        if (status_banner.starts_with("Rango ") || status_banner.starts_with("Texto sombreado") || status_banner.starts_with("Σ ")) {
            status_banner.clear();
        }
    }
    void clearSelection() {
        sel_kind = SelectionKind::None;
        clearSelectionBanner();
    }

    void updatePointedCell() {
        if (!point_mode) return;
        if (point_has_range && point_anchor != cursor) {
            setInput(point_base + CellRangeRect(point_anchor, cursor).toId());
        } else {
            setInput(point_base + cursor.toId());
        }
    }

    bool isScrollDragging() const { return scroll_drag != ScrollDrag::None; }
    bool isAnyDragging() const { return mouse_dragging || isScrollDragging(); }
    void clearScrollDragging() { scroll_drag = ScrollDrag::None; }

    void exitPointMode(bool ensure_visible = false) {
        if (!point_mode) return;
        point_mode = false;
        point_has_range = false;
        cursor = point_target;
        if (ensure_visible) ensureCursorVisible();
    }

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

    /** @brief Ejecuta `mutator()` registrando automáticamente una transacción diferencial en `UndoManager`. */
    template <typename Fn>
    bool recordTransaction(const std::string& label, CellCoord before_cur, Fn&& mutator) {
        std::map<std::string, std::string> before_map;
        for (const auto& node : engine.rawTable()) {
            const auto& kv = node.getValue();
            if (!kv.getValue().empty()) before_map[kv.getKey()] = kv.getValue();
        }

        mutator();

        std::map<std::string, std::string> after_map;
        for (const auto& node : engine.rawTable()) {
            const auto& kv = node.getValue();
            if (!kv.getValue().empty()) after_map[kv.getKey()] = kv.getValue();
        }

        UndoManager::Transaction tx;
        tx.label = label;
        tx.cursor_before = before_cur;
        tx.cursor_after = cursor;

        for (const auto& [k, v_before] : before_map) {
            auto it = after_map.find(k);
            std::string v_after = (it != after_map.end()) ? it->second : "";
            if (v_before != v_after) {
                tx.patches.push_back({k, v_before, v_after});
            }
        }
        for (const auto& [k, v_after] : after_map) {
            if (before_map.find(k) == before_map.end() && !v_after.empty()) {
                tx.patches.push_back({k, "", v_after});
            }
        }

        if (tx.patches.empty()) return false;
        ast_cache.invalidate();
        undo_mgr.push(std::move(tx));
        markModifiedAndMaybeAutoSave();
        return true;
    }

    void markModifiedAndMaybeAutoSave() {
        confirm_discard = false;
        if (config.autosave) {
            saveSheet(config.current_sheet, true);
            dirty_unsaved = false;
        } else {
            dirty_unsaved = true;
            rebuildHitMap();
        }
    }

    bool performUndo() {
        if (undo_mgr.undo_stack.empty()) {
            status_banner = "Nada para deshacer.";
            return false;
        }
        UndoManager::Transaction tx = std::move(undo_mgr.undo_stack.back());
        undo_mgr.undo_stack.pop_back();
        for (auto it = tx.patches.rbegin(); it != tx.patches.rend(); ++it) {
            engine.setCell(it->cid, it->before_raw, true);
        }
        cursor = tx.cursor_before;
        ensureCursorVisible();
        ast_cache.invalidate();
        engine.addLog("↶ Deshacer: " + tx.label);
        status_banner = "✔ Deshecho: " + tx.label;
        undo_mgr.redo_stack.push_back(std::move(tx));
        markModifiedAndMaybeAutoSave();
        return true;
    }

    bool performRedo() {
        if (undo_mgr.redo_stack.empty()) {
            status_banner = "Nada para rehacer.";
            return false;
        }
        UndoManager::Transaction tx = std::move(undo_mgr.redo_stack.back());
        undo_mgr.redo_stack.pop_back();
        for (const auto& p : tx.patches) {
            engine.setCell(p.cid, p.after_raw, true);
        }
        cursor = tx.cursor_after;
        ensureCursorVisible();
        ast_cache.invalidate();
        engine.addLog("↷ Rehacer: " + tx.label);
        status_banner = "✔ Rehecho: " + tx.label;
        undo_mgr.undo_stack.push_back(std::move(tx));
        markModifiedAndMaybeAutoSave();
        return true;
    }

    /** @brief Copia (o corta) la selección actual tanto al portapapeles estructurado interno como al sistema (OSC 52 / X11 / Wayland). */
    bool copySelectionToClipboard(const std::string& banner = "✔ Selección copiada al portapapeles.", bool cut_mode = false) {
        if (!hasTextSel()) {
            TX cmin = cursor.col, rmin = cursor.row, cmax = cursor.col, rmax = cursor.row;
            if (hasCellSel()) {
                normalizeCellRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row, cmin, rmin, cmax, rmax);
            }
            cell_clip.active = true;
            cell_clip.is_cut = cut_mode;
            cell_clip.src_c0 = cmin;
            cell_clip.src_r0 = rmin;
            cell_clip.cols   = cmax - cmin + 1;
            cell_clip.rows   = rmax - rmin + 1;
            cell_clip.raw_cells.clear();
            cell_clip.raw_cells.reserve(static_cast<size_t>(cell_clip.cols * cell_clip.rows));

            std::ostringstream tsv;
            for (TX r = rmin; r <= rmax; ++r) {
                if (r > rmin) tsv << "\n";
                for (TX c = cmin; c <= cmax; ++c) {
                    if (c > cmin) tsv << "\t";
                    std::string cid = CellCoord(c, r, true).toId();
                    const SpreadsheetCell* ptr = engine.findCellAt(c, r);
                    cell_clip.raw_cells.push_back(ptr ? ptr->raw_expression : engine.getRawExpression(cid));
                    tsv << (ptr ? ptr->displayValue() : "");
                }
            }
            cell_clip.tsv_snapshot = tsv.str();
            if (!cell_clip.tsv_snapshot.empty()) {
                TexcelClipboard::copy(os, cell_clip.tsv_snapshot);
            }
            status_banner = banner;
            return true;
        }

        std::string txt = extractCurrentSelectionText();
        if (txt.empty()) return false;
        TexcelClipboard::copy(os, txt);
        status_banner = banner;
        return true;
    }

    /** @brief Pega el portapapeles estructurado desplazando referencias relativas en fórmulas (`shiftFormulaRefs`). */
    bool pasteClipboardAtCursor() {
        if (!cell_clip.active || cell_clip.cols <= 0 || cell_clip.rows <= 0) {
            status_banner = "Portapapeles vacío (usa [Ctrl+C] primero).";
            return false;
        }
        const TX dst_c0 = cursor.col;
        const TX dst_r0 = cursor.row;
        const TX dcol   = dst_c0 - cell_clip.src_c0;
        const TX drow   = dst_r0 - cell_clip.src_r0;
        TX pasted_cnt   = 0;

        recordTransaction("Pegar " + CellRangeRect(dst_c0, dst_r0,
                          std::min(CellCoord::MAX_COLS - 1, dst_c0 + cell_clip.cols - 1),
                          std::min(CellCoord::MAX_ROWS - 1, dst_r0 + cell_clip.rows - 1)).toId(),
                          cursor, [&]() {
            if (cell_clip.is_cut) {
                for (TX r = 0; r < cell_clip.rows; ++r) {
                    for (TX c = 0; c < cell_clip.cols; ++c) {
                        TX sc = cell_clip.src_c0 + c;
                        TX sr = cell_clip.src_r0 + r;
                        bool overlapped = (sc >= dst_c0 && sc < dst_c0 + cell_clip.cols &&
                                           sr >= dst_r0 && sr < dst_r0 + cell_clip.rows);
                        if (!overlapped) {
                            engine.setCell(CellCoord(sc, sr, true).toId(), "", true);
                        }
                    }
                }
            }
            for (TX r = 0; r < cell_clip.rows; ++r) {
                TX nr = dst_r0 + r;
                if (nr >= CellCoord::MAX_ROWS) break;
                for (TX c = 0; c < cell_clip.cols; ++c) {
                    TX nc = dst_c0 + c;
                    if (nc >= CellCoord::MAX_COLS) break;
                    const std::string& raw = cell_clip.raw_cells[static_cast<size_t>(r * cell_clip.cols + c)];
                    std::string expr = (!raw.empty() && raw[0] == '=' && (dcol != 0 || drow != 0))
                        ? SpreadsheetEngine::shiftFormulaRefs(raw, dcol, drow)
                        : raw;
                    engine.setCell(CellCoord(nc, nr, true).toId(), expr, true);
                    ++pasted_cnt;
                }
            }
            if (cell_clip.is_cut) {
                cell_clip.src_c0 = dst_c0;
                cell_clip.src_r0 = dst_r0;
                cell_clip.is_cut = false;
            }
        });

        if (cell_clip.cols > 1 || cell_clip.rows > 1) {
            sel_kind = SelectionKind::Cells;
            sel_a = {dst_c0, dst_r0, true};
            sel_b = {std::min(CellCoord::MAX_COLS - 1, dst_c0 + cell_clip.cols - 1),
                     std::min(CellCoord::MAX_ROWS - 1, dst_r0 + cell_clip.rows - 1), true};
        }
        engine.addLog("✔ Pegadas " + std::to_string(pasted_cnt) + " celdas en " + CellCoord(dst_c0, dst_r0, true).toId());
        status_banner = "✔ Pegadas " + std::to_string(pasted_cnt) + " celdas (fórmulas ajustadas).";
        return true;
    }

    /** @brief Pega una tabla TSV externa recibida por Bracketed Paste a partir de `cursor`. */
    bool pasteExternalTsvAtCursor(std::string_view tsv) {
        if (tsv.empty()) return false;
        if (cell_clip.active && !cell_clip.tsv_snapshot.empty() && tsv == cell_clip.tsv_snapshot) {
            return pasteClipboardAtCursor();
        }
        const TX dst_c0 = cursor.col;
        const TX dst_r0 = cursor.row;
        TX pasted_cnt = 0;

        recordTransaction("Pegar TSV", cursor, [&]() {
            TX r_off = 0;
            size_t pos = 0;
            while (pos < tsv.size() && dst_r0 + r_off < CellCoord::MAX_ROWS) {
                size_t nl = tsv.find('\n', pos);
                std::string_view line = (nl == std::string_view::npos) ? tsv.substr(pos) : tsv.substr(pos, nl - pos);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

                TX c_off = 0;
                size_t lpos = 0;
                while (dst_c0 + c_off < CellCoord::MAX_COLS) {
                    size_t tb = line.find('\t', lpos);
                    std::string_view cell_sv = (tb == std::string_view::npos) ? line.substr(lpos) : line.substr(lpos, tb - lpos);
                    engine.setCell(CellCoord(dst_c0 + c_off, dst_r0 + r_off, true).toId(), std::string(cell_sv), true);
                    ++pasted_cnt;
                    ++c_off;
                    if (tb == std::string_view::npos) break;
                    lpos = tb + 1;
                }
                ++r_off;
                if (nl == std::string_view::npos) break;
                pos = nl + 1;
            }
        });
        status_banner = "✔ Pegadas " + std::to_string(pasted_cnt) + " celdas desde portapapeles.";
        return pasted_cnt > 0;
    }

    /** @brief Calcula estadísticas instantáneas (`Suma`, `Prom`, `Mín`, `Máx`, `Conteo`) sobre el rango seleccionado. */
    RangeQuickStats computeSelectionStats() const {
        RangeQuickStats st_out;
        if (!hasCellSel() || sel_a == sel_b) return st_out;
        TX cmin, rmin, cmax, rmax;
        normalizeCellRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row, cmin, rmin, cmax, rmax);
        // Acotar al rectángulo con datos reales para que seleccionar A1:ZZ999 sea O(celdas activas)
        TX eff_cmax = std::min(cmax, engine.maxUsedCol());
        TX eff_rmax = std::min(rmax, engine.maxUsedRow());
        if (eff_cmax < cmin || eff_rmax < rmin) return st_out;

        for (TX r = rmin; r <= eff_rmax; ++r) {
            for (TX c = cmin; c <= eff_cmax; ++c) {
                const SpreadsheetCell* ptr = engine.findCellAt(c, r);
                if (!ptr || ptr->raw_expression.empty()) continue;
                st_out.has_cells = true;
                ++st_out.non_empty;
                if (!ptr->has_error && !ptr->result.is_text) {
                    double v = ptr->result.num;
                    if (st_out.num_count == 0) st_out.min_val = st_out.max_val = v;
                    else {
                        st_out.min_val = std::min(st_out.min_val, v);
                        st_out.max_val = std::max(st_out.max_val, v);
                    }
                    st_out.sum += v;
                    ++st_out.num_count;
                }
            }
        }
        return st_out;
    }

    /** @brief Ajusta el ancho de una columna (`delta != 0`) y persiste en `.config`. */
    void adjustActiveColWidth(TX delta) {
        TX cur_w = colWidth(cursor.col);
        config.setColWidth(config.current_sheet, cursor.col, cur_w + delta);
        config.saveConfig();
        recomputeVisibleCols();
        clampScroll();
        ensureCursorVisible();
        status_banner = "Ancho columna " + CellCoord::colToLabel(cursor.col) + ": " + std::to_string(colWidth(cursor.col));
    }

    /** @brief Auto-ajusta el ancho de una columna según el texto más largo en ella y persiste en `.config`. */
    void autoFitColumn(TX col, bool save_cfg = true) {
        if (col < 0 || col >= CellCoord::MAX_COLS) return;
        size_t best = std::max<size_t>(5, visible_length(CellCoord::colToLabel(col)) + 2);
        TX max_r = engine.maxUsedRow();
        for (TX r = 0; r <= max_r; ++r) {
            const SpreadsheetCell* ptr = engine.findCellAt(col, r);
            if (!ptr || ptr->raw_expression.empty()) continue;
            std::string disp = ptr->displayValue();
            if (!ptr->has_error && ptr->result.is_text &&
                (texcel_detail::equalsIgnoreCase(disp, "TRUE") || texcel_detail::equalsIgnoreCase(disp, "FALSE"))) {
                disp = "■";
            }
            best = std::max(best, visible_length(disp) + 2);
        }
        TX target_w = std::clamp<TX>(static_cast<TX>(best), 6, 30);
        config.setColWidth(config.current_sheet, col, target_w);
        if (save_cfg) {
            config.saveConfig();
            recomputeVisibleCols();
            clampScroll();
            ensureCursorVisible();
            status_banner = "✔ Columna " + CellCoord::colToLabel(col) + " auto-ajustada (" + std::to_string(target_w) + ").";
        }
    }

    void autoFitAllColumns() {
        TX max_c = engine.maxUsedCol();
        for (TX c = 0; c <= max_c; ++c) autoFitColumn(c, false);
        config.saveConfig();
        recomputeVisibleCols();
        clampScroll();
        ensureCursorVisible();
        status_banner = "✔ Todas las columnas activas auto-ajustadas.";
    }

    /** @brief Salta estilo Excel (`Ctrl+Flecha`) al extremo de datos contiguos o al siguiente bloque no vacío. */
    void jumpDataEdge(int dc, int dr) {
        auto hasData = [&](TX c, TX r) {
            const SpreadsheetCell* p = engine.findCellAt(c, r);
            return p != nullptr && !p->raw_expression.empty();
        };
        TX c = cursor.col, r = cursor.row;
        TX nc = c + dc, nr = r + dr;
        if (nc < 0 || nc >= CellCoord::MAX_COLS || nr < 0 || nr >= CellCoord::MAX_ROWS) return;

        if (hasData(c, r) && hasData(nc, nr)) {
            while (c + dc >= 0 && c + dc < CellCoord::MAX_COLS &&
                   r + dr >= 0 && r + dr < CellCoord::MAX_ROWS &&
                   hasData(c + dc, r + dr)) {
                c += dc;
                r += dr;
            }
        } else {
            while (c + dc >= 0 && c + dc < CellCoord::MAX_COLS &&
                   r + dr >= 0 && r + dr < CellCoord::MAX_ROWS &&
                   !hasData(c + dc, r + dr)) {
                c += dc;
                r += dr;
                // Si superamos la región con datos hacia derecha/abajo, ir al límite usado o borde
                if (dc > 0 && c > std::max(cursor.col, engine.maxUsedCol())) { c = CellCoord::MAX_COLS - 1; break; }
                if (dr > 0 && r > std::max(cursor.row, engine.maxUsedRow())) { r = CellCoord::MAX_ROWS - 1; break; }
            }
            if (c + dc >= 0 && c + dc < CellCoord::MAX_COLS &&
                r + dr >= 0 && r + dr < CellCoord::MAX_ROWS &&
                hasData(c + dc, r + dr)) {
                c += dc;
                r += dr;
            }
        }
        clearSelection();
        cursor = {c, r, true};
        resetAstScroll();
        ensureCursorVisible();
    }

    /** @brief Recalcula coincidencias de búsqueda (`Ctrl+B`) ordenadas por fila y columna. */
    void refreshSearchMatches(bool jump_to_nearest = true) {
        search.matches.clear();
        std::string q = texcel_detail::toUpper(texcel_detail::trim(search.query));
        if (q.empty()) {
            search.match_idx = 0;
            return;
        }
        TX mr = engine.maxUsedRow();
        TX mc = engine.maxUsedCol();
        for (TX r = 0; r <= mr; ++r) {
            for (TX c = 0; c <= mc; ++c) {
                const SpreadsheetCell* p = engine.findCellAt(c, r);
                if (!p || p->raw_expression.empty()) continue;
                std::string disp = texcel_detail::toUpper(p->displayValue());
                std::string raw  = texcel_detail::toUpper(p->raw_expression);
                if (disp.find(q) != std::string::npos || raw.find(q) != std::string::npos) {
                    search.matches.push_back(CellCoord(c, r, true));
                }
            }
        }
        if (search.matches.empty()) {
            search.match_idx = 0;
            return;
        }
        if (jump_to_nearest) {
            search.match_idx = 0;
            for (size_t i = 0; i < search.matches.size(); ++i) {
                const auto& m = search.matches[i];
                if (m.row > cursor.row || (m.row == cursor.row && m.col >= cursor.col)) {
                    search.match_idx = static_cast<TX>(i);
                    break;
                }
            }
            cursor = search.matches[static_cast<size_t>(search.match_idx)];
            ensureCursorVisible();
        } else {
            search.match_idx = std::clamp<TX>(search.match_idx, 0, static_cast<TX>(search.matches.size()) - 1);
        }
    }

    void stepSearchMatch(int delta) {
        if (search.matches.empty()) return;
        TX n = static_cast<TX>(search.matches.size());
        search.match_idx = ((search.match_idx + delta) % n + n) % n;
        cursor = search.matches[static_cast<size_t>(search.match_idx)];
        resetAstScroll();
        ensureCursorVisible();
    }

    std::string activeCellId() const {
        return (point_mode ? point_target : cursor).toId();
    }

    std::string selectedRangeStr(bool collapse_single = false) const {
        TX cmin, rmin, cmax, rmax;
        normalizeCellRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row, cmin, rmin, cmax, rmax);
        std::string id0 = CellCoord(cmin, rmin, true).toId();
        std::string id1 = CellCoord(cmax, rmax, true).toId();
        return (collapse_single && id0 == id1) ? id0 : (id0 + ":" + id1);
    }

    void applySelectionAutoFill() {
        TX filled = 0;
        recordTransaction("Auto-Fill " + selectedRangeStr(), cursor, [&]() {
            filled = engine.autoFillRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row);
        });
        status_banner = "✔ Auto-Fill (" + std::to_string(filled) + " celdas) en " + selectedRangeStr();
    }

    std::string formatCellForInput(const std::string& cid) {
        return engine.getRawExpression(cid);
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
                    const SpreadsheetCell* p = engine.findCellAt(c, r);
                    tsv << (p ? p->displayValue() : "");
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
                for (TX y = sy0; y <= sy1 && static_cast<size_t>(y) < win_lines[text_win].size(); ++y) {
                    if (!isSelectableWindowRow(text_win, y)) continue;
                    const std::string& line = win_lines[text_win][static_cast<size_t>(y)];
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
        const SpreadsheetCell* p = engine.findCellAt(cursor.col, cursor.row);
        return p ? p->displayValue() : "";
    }

    bool windowInnerBounds(TX win_id, TX& x0, TX& y0, TX& x1, TX& y1) const {
        if (win_id < WIN_INSPECTOR || win_id > WIN_ACTIVITY) return false;
        const UiRect& r = hits.win_inner[win_id];
        if (r.w <= 0 || r.h <= 0) return false;
        x0 = r.x;
        y0 = r.y;
        x1 = r.x + r.w - 1;
        y1 = r.y + r.h - 1;
        return true;
    }

    bool isSelectableWindowRow(TX win_id, TX abs_y) const {
        if (win_id == WIN_INSPECTOR) return abs_y != TOP_Y + 6;
        if (win_id == WIN_CONSOLE)   return abs_y != bot_y + 1 && abs_y != bot_y + 3;
        return true;
    }

    bool isSelectableWindowPos(TX win_id, TX mx, TX my) const {
        if (!isSelectableWindowRow(win_id, my)) return false;
        TX wx0, wy0, wx1, wy1;
        if (!windowInnerBounds(win_id, wx0, wy0, wx1, wy1)) return false;
        if (mx < wx0 || mx > wx1 || my < wy0 || my > wy1) return false;
        if (my >= 0 && static_cast<size_t>(my) < win_lines[win_id].size()) {
            const std::string& line = win_lines[win_id][static_cast<size_t>(my)];
            size_t cp_idx = static_cast<size_t>(mx - wx0);
            std::string g = utf8At(line, cp_idx);
            if (isDividerGlyph(g)) return false;
        }
        return true;
    }

    /** @brief Detecta si `(mx, my)` cae sobre el divisor vertical derecho de una cabecera de columna para redimensionarla. */
    bool screenToColHeaderDivider(TX mx, TX my, TX& out_col) const {
        if (my != TOP_Y + 1 && my != TOP_Y + 2) return false;
        TX x = GRID_X0;
        for (TX i = 0; i < visible_cols && scroll_col + i < CellCoord::MAX_COLS; ++i) {
            TX c = scroll_col + i;
            TX w = colWidth(c);
            TX right_div = x + 1 + w; // Posición del siguiente '│' o fin de columna
            if (mx == right_div || mx == right_div - 1) {
                out_col = c;
                return true;
            }
            x += w + 1;
        }
        return false;
    }

    bool screenToGridCell(TX mx, TX my, TX& out_col, TX& out_row, bool clamp_to_grid, TX* out_rel_x_in_cell = nullptr, TX* out_cell_w = nullptr) const {
        TX total_grid_w = 0;
        for (TX i = 0; i < visible_cols && scroll_col + i < CellCoord::MAX_COLS; ++i) {
            total_grid_w += colWidth(scroll_col + i) + 1;
        }
        TX grid_x1 = GRID_X0 + std::max<TX>(1, total_grid_w) - 1;
        TX grid_y1 = GRID_Y0 + visible_rows - 1;

        if (!clamp_to_grid) {
            if (mx < GRID_X0 || mx > grid_x1 || my < GRID_Y0 || my > grid_y1) return false;
        } else {
            mx = std::clamp(mx, GRID_X0, grid_x1);
            my = std::clamp(my, GRID_Y0, grid_y1);
        }

        TX cur_x = GRID_X0;
        TX found_col = scroll_col;
        TX rel_x = 1;
        TX cw = colWidth(scroll_col);
        for (TX i = 0; i < visible_cols && scroll_col + i < CellCoord::MAX_COLS; ++i) {
            TX c = scroll_col + i;
            TX w = colWidth(c);
            if (!clamp_to_grid && mx == cur_x) return false; // Barra divisoria '│'
            if (mx <= cur_x + w || i + 1 == visible_cols) {
                found_col = c;
                rel_x = std::clamp<TX>(mx - cur_x, 0, w);
                cw = w;
                break;
            }
            cur_x += w + 1;
        }

        TX rel_r = my - GRID_Y0;
        out_col = std::clamp(found_col, static_cast<TX>(0), CellCoord::MAX_COLS - 1);
        out_row = std::clamp(scroll_row + rel_r, static_cast<TX>(0), CellCoord::MAX_ROWS - 1);
        if (out_rel_x_in_cell) *out_rel_x_in_cell = rel_x;
        if (out_cell_w) *out_cell_w = cw;
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
        TX new_total_h = interactive_alt_screen ? std::max(MIN_TOTAL_ROWS, rows) : TOTAL_HEIGHT;

        bool changed = (new_total_w != total_width) || (rows != term_rows) || (new_total_h != total_height);

        term_rows    = rows;
        total_width  = new_total_w;
        total_height = new_total_h;

        // Reparte la altura disponible entre los paneles superiores e inferiores
        TX delta_h = total_height - TOTAL_HEIGHT;
        if (delta_h == 0) {
            top_h        = TOP_H;
            bot_y        = BOT_Y;
            bot_h        = BOT_H;
            visible_rows = VISIBLE_ROWS;
            ast_view_h   = AST_VIEW_H;
        } else if (delta_h > 0) {
            TX extra_top = (delta_h * 7 + 5) / 10;
            TX extra_bot = delta_h - extra_top;
            top_h        = TOP_H + extra_top;
            bot_y        = TOP_Y + top_h;
            bot_h        = BOT_H + extra_bot;
            visible_rows = VISIBLE_ROWS + extra_top;
            ast_view_h   = AST_VIEW_H + extra_top;
        } else {
            // Terminales compactas de 24..27 filas
            top_h        = std::max<TX>(12, TOP_H + delta_h + 1);
            bot_y        = TOP_Y + top_h;
            bot_h        = std::max<TX>(9, total_height - 1 - bot_y);
            visible_rows = std::max<TX>(6, top_h - 6);
            ast_view_h   = std::max<TX>(2, top_h - 10);
        }

        right_w = std::clamp((total_width * 38) / 100, static_cast<TX>(26), static_cast<TX>(72));
        left_w  = total_width - right_w - 1;
        right_x = left_w + 1;

        recomputeVisibleCols();
        resizeWinLines(total_height);
        rebuildHitMap();
        return changed;
    }

    void ensureCoordVisible(const CellCoord& c) {
        if (c.col < scroll_col) {
            scroll_col = c.col;
            recomputeVisibleCols();
        }
        while (c.col >= scroll_col + visible_cols && scroll_col < CellCoord::MAX_COLS - 1) {
            ++scroll_col;
            recomputeVisibleCols();
        }
        if (c.row < scroll_row) scroll_row = c.row;
        if (c.row >= scroll_row + visible_rows) scroll_row = c.row - visible_rows + 1;
    }

    void ensureCursorVisible() {
        ensureCoordVisible(cursor);
    }

    void clampScroll() {
        recomputeVisibleCols();
        scroll_col = std::clamp<TX>(scroll_col, 0, std::max<TX>(0, CellCoord::MAX_COLS - visible_cols));
        recomputeVisibleCols();
        scroll_row = std::clamp<TX>(scroll_row, 0, std::max<TX>(0, CellCoord::MAX_ROWS - visible_rows));
    }

    void clearSelectedCells() {
        if (hasCellSel()) {
            std::string r_str = selectedRangeStr();
            recordTransaction("Borrar " + r_str, cursor, [&]() {
                engine.clearRange(sel_a.col, sel_a.row, sel_b.col, sel_b.row);
            });
            clearSelection();
            status_banner = "Rango " + r_str + " borrado ([Ctrl+Z] deshacer).";
        } else {
            std::string cid = cursor.toId();
            recordTransaction("Borrar " + cid, cursor, [&]() {
                engine.setCell(cid, "");
            });
            status_banner = "Celda " + cid + " borrada ([Ctrl+Z] deshacer).";
        }
    }

    void beginNameBoxEdit() {
        search.close();
        namebox_editing = true;
        namebox_buffer.clear();
        mode = SpreadsheetFocusMode::GridNavigation;
        status_banner = "Ir a celda o rango (ej. ZZ500 o A1:C5) y pulsa [Enter].";
    }

    void beginSearchMode() {
        namebox_editing = false;
        clearSelection();
        exitPointMode();
        search.active = true;
        search.query.clear();
        search.matches.clear();
        search.match_idx = 0;
        mode = SpreadsheetFocusMode::GridNavigation;
        status_banner = "Escribe para buscar en la hoja │ [Enter/↓] Siguiente │ [↑] Anterior │ [Esc] Cerrar";
    }

    void jumpToHome() {
        cursor = {0, 0, true};
        scroll_col = 0;
        scroll_row = 0;
        recomputeVisibleCols();
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
                status_banner = "Rango inválido: " + s;
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
            status_banner = "Celda inválida: " + s;
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
        scroll_row = mapScrollTrack(my, GRID_Y0, visible_rows - 1, CellCoord::MAX_ROWS - visible_rows);
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
        TX max_y = std::max<TX>(0, ast_graph_h - ast_view_h);
        ast_scroll_x = std::clamp<TX>(ast_scroll_x, 0, max_x);
        ast_scroll_y = std::clamp<TX>(ast_scroll_y, 0, max_y);
    }

    void resetAstScroll() {
        ast_scroll_x = 0;
        ast_scroll_y = 0;
    }

    void scrollAstVerticalToY(TX my) {
        ast_scroll_y = mapScrollTrack(my, AST_Y0, ast_view_h - 1, ast_graph_h - ast_view_h);
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
            case ScrollDrag::ColResize: {
                TX prev_w = colWidth(resize_col);
                TX next_w = std::clamp<TX>(resize_start_w + (mx - resize_start_x),
                                           TexcelConfig::MIN_COL_WIDTH, TexcelConfig::MAX_COL_WIDTH);
                if (next_w == prev_w) return false;
                config.setColWidth(config.current_sheet, resize_col, next_w);
                recomputeVisibleCols();
                status_banner = "Columna " + CellCoord::colToLabel(resize_col) + ": ancho " + std::to_string(next_w);
                return true;
            }
            default: return false;
        }
    }

    bool tryStartScrollbarDrag(TX mx, TX my) {
        if (mx == LEFT_X + left_w - 1 && my >= GRID_Y0 && my < GRID_Y0 + visible_rows) {
            scroll_drag = ScrollDrag::GridV;
        } else if (my == TOP_Y + top_h - 1 && mx >= GRID_X0 + 1 && mx <= LEFT_X + left_w - 2) {
            scroll_drag = ScrollDrag::GridH;
        } else if (ast_graph_h > ast_view_h && mx == right_x + right_w - 1 &&
                   my >= AST_Y0 && my < AST_Y0 + ast_view_h) {
            scroll_drag = ScrollDrag::AstV;
        } else if (ast_graph_w > astViewWidth() && my == TOP_Y + top_h - 1 &&
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

    /** @brief Persistencia atómica (`.txl.tmp` + `rename`) para evitar corrupción ante cierres inesperados. */
    void saveSheet(const std::string& name, bool silent = false) {
        config.ensureSheetsDir();
        std::filesystem::path p     = config.sheetPath(name);
        std::filesystem::path tmp_p = std::filesystem::path(config.sheets_dir) / (name + ".txl.tmp");
        bool ok = false;
        {
            std::ofstream ofs(tmp_p, std::ios::trunc);
            if (ofs) {
                ofs << engine.rawTable() << std::endl;
                ok = ofs.good();
            }
        }
        if (ok) {
            std::error_code ec;
            std::filesystem::rename(tmp_p, p, ec);
            if (ec) {
                std::filesystem::copy_file(tmp_p, p, std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(tmp_p, ec);
            }
            config.current_sheet = name;
            dirty_unsaved = false;
            confirm_discard = false;
            config.saveConfig();
            rebuildHitMap();
            if (!silent) {
                engine.addLog("✔ Guardada hoja: " + name + TexcelConfig::SHEET_EXT);
                status_banner = "✔ Hoja '" + name + "' guardada en " + p.string();
            }
        } else if (!silent) {
            engine.addLog("✖ Error al guardar " + name + TexcelConfig::SHEET_EXT);
            status_banner = "Error: No se pudo guardar '" + name + "'.";
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
        dirty_unsaved = false;
        confirm_discard = false;
        undo_mgr.clear();
        ast_cache.invalidate();
        search.close();
        config.saveConfig();
        recomputeVisibleCols();
        clampScroll();
        rebuildHitMap();
        engine.addLog("▸ Cargada hoja: " + name + TexcelConfig::SHEET_EXT);
        status_banner = "Hoja '" + name + "' cargada correctamente.";
        return true;
    }

    void maybeAutoSave() {
        markModifiedAndMaybeAutoSave();
    }

    void deleteSelectedModalSheet() {
        if (modal_idx < MODAL_FIRST_FILE) return;
        TX fidx = modal_idx - MODAL_FIRST_FILE;
        if (fidx < static_cast<TX>(config.sheet_files.size())) {
            std::string target = config.sheet_files[static_cast<size_t>(fidx)].getValue();
            std::error_code ec;
            std::filesystem::remove(config.sheetPath(target), ec);
            config.resetSheetColWidths(target);
            config.saveConfig();
            engine.addLog("⌫ Eliminada hoja: " + target + TexcelConfig::SHEET_EXT);
            refreshSheetList();
        }
    }
};

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_STATE_H__
