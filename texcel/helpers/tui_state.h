/**
 * @file tui_state.h
 * @brief Estado compartido y helpers de geometría/persistencia para la TUI de Texcel.
 */

#ifndef __TEXCEL_HELPERS_TUI_STATE_H__
#define __TEXCEL_HELPERS_TUI_STATE_H__

#include "../engine.h"
#include "../config.h"
#include "../clipboard.h"
#include "tui_text.h"
#include "../../types.h"
#include <algorithm>
#include <cctype>
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

/**
 * @struct TuiState
 * @brief Estado completo de la sesión TUI compartido entre el renderer, el input handler y `SpreadsheetTUI`.
 */
struct TuiState {
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

    TX cursor_col = 0;
    TX cursor_row = 0;
    TX scroll_col = 0;
    TX scroll_row = 0;

    // Point Mode
    bool        point_mode = false;
    TX          target_col = 0;
    TX          target_row = 0;
    std::string point_base;

    // Selección con ratón (SGR Mouse Tracking) confinada por ventana
    bool mouse_dragging = false;
    TX   drag_win       = 0;
    bool has_cell_sel   = false;
    TX   sel_c0 = 0, sel_r0 = 0, sel_c1 = 0, sel_r1 = 0;
    bool has_text_sel   = false;
    TX   text_win       = 0;
    TX   tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    TX   last_anchor_win = 0;
    TX   last_anchor_x   = 0;
    TX   last_anchor_y   = 0;
    std::string win_lines[5][TOTAL_HEIGHT];

    // Consola e historial
    std::string input_buffer;
    size_t      input_cursor = 0;
    std::string status_banner = "Usa [Tab] para alternar entre Matriz y Consola.";
    Vector<VectorAscTraits<std::string>> cmd_history;
    TX history_idx = -1;

    // Gestor de hojas y configuración
    TexcelConfig config;
    bool         modal_open     = false;
    bool         modal_new_mode = false;
    std::string  modal_new_name;
    TX           modal_idx      = 0;

    TuiState(SpreadsheetEngine& eng, std::ostream& out) : engine(eng), os(out) {}

    static bool isFormulaBuffer(const std::string& s) {
        return s.find('=') != std::string::npos;
    }

    static bool canAcceptPointRef(const std::string& s) {
        size_t i = s.size();
        while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) --i;
        if (i == 0) return false;
        char last = s[i - 1];
        return last == '=' || last == '+' || last == '-' || last == '*' ||
               last == '/' || last == '^' || last == '(' || last == ':' || last == ',';
    }

    void updatePointedCell() {
        if (!point_mode) return;
        input_buffer = point_base + CellCoord(cursor_col, cursor_row, true).toId();
    }

    void copyToClipboard(const std::string& text) {
        TexcelClipboard::copy(os, text);
    }

    std::string formatCellForInput(const std::string& cid) {
        SpreadsheetCell cur = engine.getCell(cid);
        if (cur.raw_expression.empty()) return "";
        if (cur.is_formula && cur.raw_expression[0] != '=') return "= " + cur.raw_expression;
        return cur.raw_expression;
    }

    void loadActiveCellIntoBuffer() {
        std::string cid = CellCoord(cursor_col, cursor_row, true).toId();
        input_buffer = formatCellForInput(cid);
        input_cursor = input_buffer.size();
    }

    std::string extractCurrentSelectionText() {
        if (has_cell_sel) {
            TX cmin, rmin, cmax, rmax;
            normalizeCellRange(sel_c0, sel_r0, sel_c1, sel_r1, cmin, rmin, cmax, rmax);
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
        if (has_text_sel && text_win >= 2 && text_win <= 4) {
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
        return engine.getCell(CellCoord(cursor_col, cursor_row, true).toId()).displayValue();
    }

    bool windowInnerBounds(TX win_id, TX& x0, TX& y0, TX& x1, TX& y1) const {
        if (win_id == 2) { x0 = right_x + 2; y0 = TOP_Y + 1; x1 = right_x + right_w - 3; y1 = TOP_Y + TOP_H - 2; return true; }
        if (win_id == 3) { x0 = LEFT_X + 2;  y0 = BOT_Y + 1; x1 = LEFT_X + left_w - 3;   y1 = BOT_Y + BOT_H - 2; return true; }
        if (win_id == 4) { x0 = right_x + 2; y0 = BOT_Y + 1; x1 = right_x + right_w - 3; y1 = BOT_Y + BOT_H - 2; return true; }
        return false;
    }

    /** @brief Verifica si una fila dentro de una ventana es seleccionable (excluye barras divisorias horizontales). */
    bool isSelectableWindowRow(TX win_id, TX abs_y) const {
        if (win_id == 2) {
            // Excluir barra divisoria "─── BinaryTree AST ───" (TOP_Y + 6) y conector "╭──┴──╮" (TOP_Y + 9)
            if (abs_y == TOP_Y + 6 || abs_y == TOP_Y + 9) return false;
        } else if (win_id == 3) {
            // Excluir bordes superior e inferior de la caja del prompt (BOT_Y + 1 y BOT_Y + 3)
            if (abs_y == BOT_Y + 1 || abs_y == BOT_Y + 3) return false;
        }
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
        TX grid_x0 = LEFT_X + 7;
        TX grid_x1 = grid_x0 + visible_cols * (CELL_WIDTH + 1) - 1;
        TX grid_y0 = TOP_Y + 3;
        TX grid_y1 = TOP_Y + 3 + VISIBLE_ROWS - 1;

        if (!clamp_to_grid) {
            if (mx < grid_x0 || mx > grid_x1 || my < grid_y0 || my > grid_y1) return false;
            // Excluir las barras divisorias verticales '│' entre columnas de celdas
            if ((mx - grid_x0) % (CELL_WIDTH + 1) == 0) return false;
        } else {
            mx = std::clamp(mx, grid_x0, grid_x1);
            my = std::clamp(my, grid_y0, grid_y1);
        }

        TX rel_c = (mx - grid_x0) / (CELL_WIDTH + 1);
        TX rel_r = my - grid_y0;
        out_col = std::clamp(scroll_col + rel_c, static_cast<TX>(0), static_cast<TX>(25));
        out_row = std::clamp(scroll_row + rel_r, static_cast<TX>(0), static_cast<TX>(98));
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
        TX avail_grid = std::max(static_cast<TX>(14), left_w - 9);
        visible_cols = std::clamp(avail_grid / (CELL_WIDTH + 1), static_cast<TX>(1), static_cast<TX>(26));
        return changed;
    }

    void ensureCursorVisible() {
        if (cursor_col < scroll_col) scroll_col = cursor_col;
        if (cursor_col >= scroll_col + visible_cols) scroll_col = cursor_col - visible_cols + 1;
        if (cursor_row < scroll_row) scroll_row = cursor_row;
        if (cursor_row >= scroll_row + VISIBLE_ROWS) scroll_row = cursor_row - VISIBLE_ROWS + 1;
    }

    void clampScroll() {
        scroll_col = std::clamp<TX>(scroll_col, 0, std::max<TX>(0, 26 - visible_cols));
        scroll_row = std::clamp<TX>(scroll_row, 0, std::max<TX>(0, 99 - VISIBLE_ROWS));
    }

    void refreshSheetList() {
        config.refreshSheetList();
        TX max_idx = 2 + static_cast<TX>(config.sheet_files.size());
        modal_idx = std::clamp<TX>(modal_idx, 0, max_idx);
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
        engine.clear();
        ifs >> engine.rawTable();
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
        if (modal_idx < 3) return;
        TX fidx = modal_idx - 3;
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
