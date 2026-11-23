/**
 * @file input.h
 * @ingroup texcel
 * @brief Despachadores independientes de entrada (teclado, ratón SGR, modal, Point Mode y comandos) sobre `TuiState`.
 */

#ifndef __TEXCEL_HELPERS_INPUT_H__
#define __TEXCEL_HELPERS_INPUT_H__

#include "state.h"
#include "text.h"
#include "../../utilities/gotoxy.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace texcel_tui {

inline bool handleCommandSubmit(TuiState& st, const std::string& raw_cmd) {
    std::string cmd = texcel_detail::trim(raw_cmd);
    if (cmd.empty()) return true;

    st.cmd_history.push_back(cmd);
    st.history_idx = static_cast<TX>(st.cmd_history.size());

    std::string upper = texcel_detail::toUpper(cmd);
    if (upper == "QUIT" || upper == "EXIT" || upper == "Q") return false;

    if (upper == "CLEAR") {
        st.engine.clear();
        st.maybeAutoSave();
        st.status_banner = "Hoja de calculo limpiada por completo.";
        return true;
    }
    if (upper == "SAVE") { st.saveSheet(st.config.current_sheet); return true; }
    if (upper == "LOAD") { st.loadSheet(st.config.current_sheet); return true; }

    std::string assigned_id;
    bool ok = st.engine.executeCommand(cmd, st.cursor.toId(), &assigned_id);
    if (ok) st.maybeAutoSave();

    CellCoord dest = CellCoord::parse(assigned_id);
    if (dest.valid) {
        st.cursor = dest;
        st.ensureCursorVisible();
    }
    st.status_banner = ok
        ? ("Actualizada " + assigned_id + " = " + st.engine.getCell(assigned_id).displayValue())
        : ("Error al evaluar " + assigned_id + ": verifica sintaxis o ciclos.");
    return true;
}

inline bool handleMouseWheel(TuiState& st, TX btn, TX mx, TX my) {
    bool in_inspector = (mx >= st.right_x && mx < st.right_x + st.right_w &&
                         my >= TuiState::TOP_Y && my < TuiState::TOP_Y + TuiState::TOP_H);
    bool is_horizontal_mod = (btn & (2 | 4 | 8 | 16)) != 0;
    bool forward = (btn & 1) != 0;
    if (in_inspector) {
        constexpr TX AST_V_STEP = 1, AST_H_STEP = 4;
        bool scroll_h = is_horizontal_mod || (st.ast_graph_h <= TuiState::AST_VIEW_H && st.ast_graph_w > st.astViewWidth());
        if (scroll_h) st.ast_scroll_x += forward ? AST_H_STEP : -AST_H_STEP;
        else st.ast_scroll_y += forward ? AST_V_STEP : -AST_V_STEP;
        st.clampAstScroll();
        return true;
    }

    constexpr TX V_STEP = 3, H_STEP = 2;
    bool on_col_header = (mx >= TuiState::LEFT_X + 2 && mx < TuiState::LEFT_X + st.left_w - 2 &&
                          (my == TuiState::TOP_Y + 1 || my == TuiState::TOP_Y + 2));
    if (on_col_header || is_horizontal_mod) st.scroll_col += forward ? H_STEP : -H_STEP;
    else st.scroll_row += forward ? V_STEP : -V_STEP;
    st.clampScroll();
    return true;
}

inline void finishMouseDrag(TuiState& st) {
    st.mouse_dragging = false;
    if (st.drag_win == TuiState::WIN_GRID && st.hasFillSel()) {
        st.clearSelection();
        if (st.sel_a != st.sel_b) st.applySelectionAutoFill();
    } else if (st.drag_win == TuiState::WIN_GRID && st.console_ref_inserting) {
        st.console_ref_inserting = false;
        st.clearSelection();
    } else if (st.drag_win == TuiState::WIN_GRID && st.hasCellSel()) {
        if (st.sel_a != st.sel_b) {
            st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C], [Y] o Clic Der. para copiar).";
        } else {
            st.clearSelection();
        }
    } else if (st.drag_win >= TuiState::WIN_INSPECTOR && st.drag_win <= TuiState::WIN_ACTIVITY && st.hasTextSel()) {
        if (st.tx0 == st.tx1 && st.ty0 == st.ty1) st.clearSelection();
        else st.status_banner = "Texto sombreado ([Ctrl+C] o Clic Der. para copiar).";
    }
    st.drag_win = 0;
}

inline bool handleMouseDrag(TuiState& st, TX mx, TX my) {
    if (st.isScrollDragging()) return st.dragScrollbarTo(mx, my);
    if (!st.mouse_dragging) return false;

    if (st.drag_win == TuiState::WIN_GRID) {
        TX gc = 0, gr = 0;
        st.screenToGridCell(mx, my, gc, gr, true);
        if (gc == st.sel_b.col && gr == st.sel_b.row) return false;
        st.sel_b = {gc, gr, true};
        if (st.console_ref_inserting || st.point_mode) {
            std::string ref = st.selectedRangeStr(true);
            if (st.console_ref_inserting) st.insertOrReplaceRefAtCursor(ref);
            else st.setInput(st.point_base + ref);
        }
        return true;
    }
    if (st.drag_win >= TuiState::WIN_INSPECTOR && st.drag_win <= TuiState::WIN_ACTIVITY) {
        TX wx0, wy0, wx1, wy1;
        if (st.windowInnerBounds(st.drag_win, wx0, wy0, wx1, wy1)) {
            TX nx = std::clamp(mx, wx0, wx1);
            TX ny = std::clamp(my, wy0, wy1);
            if (nx == st.tx1 && ny == st.ty1) return false;
            st.tx1 = nx;
            st.ty1 = ny;
            return true;
        }
    }
    return false;
}

inline bool handleGridMouseDown(TuiState& st, TX mx, TX my, TX gc, TX gr, bool is_ctrl_click, bool is_extend_click) {
    if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.point_mode &&
        !is_ctrl_click && TuiState::isFormulaBuffer(st.input_buffer)) {
        st.drag_win = TuiState::WIN_GRID;
        st.mouse_dragging = true;
        st.sel_kind = SelectionKind::Cells;
        st.sel_a = st.sel_b = {gc, gr, true};
        st.console_ref_inserting = false;
        st.insertOrReplaceRefAtCursor(st.sel_a.toId());
        st.status_banner = "Referencia insertada en cursor (arrastra para rango).";
        return true;
    }

    // Si estaba en Consola editando un valor (no fórmula), confirmarlo en la celda activa antes de cambiar
    if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.point_mode && !st.input_buffer.empty()) {
        handleCommandSubmit(st, st.input_buffer);
    }

    st.drag_win = TuiState::WIN_GRID;
    st.mouse_dragging = true;

    TX rel_x_in_cell = (mx - TuiState::GRID_X0) % (TuiState::CELL_WIDTH + 1);
    bool on_active_corner = (gc == st.cursor.col && gr == st.cursor.row && rel_x_in_cell == TuiState::CELL_WIDTH);

    if ((is_ctrl_click || on_active_corner) && !st.point_mode) {
        st.sel_kind = SelectionKind::Fill;
        st.sel_a = st.cursor;
        st.sel_b = {gc, gr, true};
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.setInput();
        st.status_banner = "Auto-Fill: arrastra hacia cualquier direccion y suelta para rellenar.";
        return true;
    }

    bool had_sel = st.hasCellSel();
    st.sel_kind = SelectionKind::Cells;
    if (is_extend_click) {
        if (!had_sel) st.sel_a = st.cursor;
        st.sel_b = {gc, gr, true};
    } else {
        if (st.cursor.col != gc || st.cursor.row != gr) st.resetAstScroll();
        st.cursor = st.sel_a = st.sel_b = {gc, gr, true};
    }

    if (st.point_mode) {
        st.setInput(st.point_base + st.selectedRangeStr(true));
    } else {
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.setInput();
    }
    return true;
}

inline bool handleSgrMouse(TuiState& st, const std::string& payload, char mc) {
    TX btn = 0, mx_1 = 1, my_1 = 1;
    char s1 = 0, s2 = 0;
    std::istringstream mss(payload);
    if (!(mss >> btn >> s1 >> mx_1 >> s2 >> my_1) || st.modal_open) return false;

    TX mx = mx_1 - 1, my = my_1 - 1;
    bool is_release = (mc == 'm');
    bool is_drag    = (btn & 32) != 0;

    if (btn & 64) return handleMouseWheel(st, btn, mx, my);

    if (is_release && st.isAnyDragging()) {
        st.clearScrollDragging();
        if (st.mouse_dragging) finishMouseDrag(st);
        return true;
    }
    if ((btn & 3) == 2 && !is_drag && !is_release) {
        return st.copySelectionToClipboard("✔ Seleccion copiada al portapapeles (Clic derecho).");
    }
    if (is_drag) return handleMouseDrag(st, mx, my);
    if ((btn & 3) != 0 || is_release) return false;

    if (st.isNameBoxPos(mx, my)) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - st.last_namebox_click).count();
        st.last_namebox_click = now;
        if (ms > 0 && ms < 400) {
            st.namebox_editing = false;
            st.namebox_buffer.clear();
            st.jumpToHome();
        } else {
            st.beginNameBoxEdit();
        }
        return true;
    }
    st.namebox_editing = false;

    if (st.tryStartScrollbarDrag(mx, my)) return true;

    bool is_ctrl_click   = (btn & 16) != 0;
    bool is_extend_click = (btn & (4 | 8)) != 0;

    TX gc = 0, gr = 0;
    if (st.screenToGridCell(mx, my, gc, gr, false)) {
        return handleGridMouseDown(st, mx, my, gc, gr, is_ctrl_click, is_extend_click);
    }

    for (TX w = TuiState::WIN_INSPECTOR; w <= TuiState::WIN_ACTIVITY; ++w) {
        if (!st.isSelectableWindowPos(w, mx, my)) continue;
        st.drag_win = w;
        st.mouse_dragging = true;
        st.sel_kind = SelectionKind::Text;
        st.text_win = w;
        if (is_extend_click && st.last_anchor_win == w) {
            st.tx0 = st.last_anchor_x;
            st.ty0 = st.last_anchor_y;
            st.tx1 = mx;
            st.ty1 = my;
        } else {
            st.tx0 = st.tx1 = mx;
            st.ty0 = st.ty1 = my;
            st.last_anchor_win = w;
            st.last_anchor_x = mx;
            st.last_anchor_y = my;
        }
        if (w == TuiState::WIN_CONSOLE && !st.point_mode) {
            st.mode = SpreadsheetFocusMode::CommandConsole;
            if (st.input_buffer.empty()) st.loadActiveCellIntoBuffer();
        }
        return true;
    }
    return false;
}

inline void applyBracketedPaste(TuiState& st, const std::string& pasted) {
    bool has_external_sel = st.hasCellSel() || (st.hasTextSel() && st.text_win != TuiState::WIN_CONSOLE);
    if (!st.modal_open && (has_external_sel || st.mode != SpreadsheetFocusMode::CommandConsole)) {
        st.copySelectionToClipboard();
    } else if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.modal_open) {
        for (char c : pasted) {
            if (c == '\n' || c == '\r') break;
            if (texcel_detail::isChar<std::isprint>(c)) st.insertInputChar(c);
        }
    }
}

inline void handleBracketedPaste(TuiState& st) {
    std::string pasted;
    char pch = 0;
    while (read(STDIN_FILENO, &pch, 1) > 0) {
        pasted.push_back(pch);
        if (pasted.size() >= 6 && pasted.ends_with(BRACKETED_PASTE_END)) {
            pasted.resize(pasted.size() - 6);
            break;
        }
    }
    applyBracketedPaste(st, pasted);
}

inline void handleGridEscapeKey(TuiState& st, const std::string& csi_params, char csi_final) {
    auto stepCell = [](CellCoord& cell, char dir) {
        if (dir == 'A' && cell.row > 0) --cell.row;
        else if (dir == 'B' && cell.row < CellCoord::MAX_ROWS - 1) ++cell.row;
        else if (dir == 'C' && cell.col < CellCoord::MAX_COLS - 1) ++cell.col;
        else if (dir == 'D' && cell.col > 0) --cell.col;
        else return false;
        return true;
    };

    bool is_arrow = (csi_final == 'A' || csi_final == 'B' || csi_final == 'C' || csi_final == 'D');
    if (csi_params == "1;2" && is_arrow) {
        if (!st.hasCellSel()) {
            st.sel_a = st.sel_b = st.cursor;
            st.sel_kind = SelectionKind::Cells;
        }
        stepCell(st.sel_b, csi_final);
        st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C] o [Y] para copiar).";
    } else if (csi_params.empty() && is_arrow) {
        st.clearSelection();
        if (stepCell(st.cursor, csi_final)) st.resetAstScroll();
    } else if (csi_params == "3" && csi_final == '~' && !st.point_mode) {
        st.clearSelectedCells();
    }
    st.ensureCursorVisible();
    if (st.point_mode) st.updatePointedCell();
}

inline void handleConsoleEscapeKey(TuiState& st, const std::string& csi_params, char csi_final) {
    if (csi_params.empty() && csi_final == 'D') { if (st.input_cursor > 0) --st.input_cursor; }
    else if (csi_params.empty() && csi_final == 'C') { if (st.input_cursor < st.input_buffer.size()) ++st.input_cursor; }
    else if (csi_final == 'H' || (csi_params == "1" && csi_final == '~')) st.input_cursor = 0;
    else if (csi_final == 'F' || (csi_params == "4" && csi_final == '~')) st.input_cursor = st.input_buffer.size();
    else if (csi_params == "3" && csi_final == '~') {
        if (st.input_cursor < st.input_buffer.size()) st.input_buffer.erase(st.input_cursor, 1);
    } else if (csi_params.empty() && csi_final == 'A' && st.cmd_history.size() > 0) {
        if (st.history_idx > 0) --st.history_idx;
        st.setInput(st.cmd_history[static_cast<size_t>(st.history_idx)].getValue());
    } else if (csi_params.empty() && csi_final == 'B') {
        if (st.history_idx + 1 < static_cast<TX>(st.cmd_history.size())) {
            ++st.history_idx;
            st.setInput(st.cmd_history[static_cast<size_t>(st.history_idx)].getValue());
        } else {
            st.history_idx = static_cast<TX>(st.cmd_history.size());
            st.setInput();
        }
    }
}

inline bool handleEscapeSequence(TuiState& st, termios& raw_term) {
    char seq[2] = {0, 0};
    raw_term.c_cc[VMIN] = 0;
    raw_term.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
    TX n1 = static_cast<TX>(read(STDIN_FILENO, &seq[0], 1));
    TX n2 = (n1 > 0) ? static_cast<TX>(read(STDIN_FILENO, &seq[1], 1)) : 0;
    raw_term.c_cc[VMIN] = 1;
    raw_term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

    if (seq[0] == '[' && seq[1] == '<') {
        std::string payload;
        char mc = 0;
        while (read(STDIN_FILENO, &mc, 1) > 0) {
            if (mc == 'M' || mc == 'm') break;
            payload.push_back(mc);
        }
        return handleSgrMouse(st, payload, mc);
    }

    std::string csi_params;
    char csi_final = seq[1];
    if (n2 > 0 && seq[0] == '[' && seq[1] >= '0' && seq[1] <= '9') {
        csi_params.push_back(seq[1]);
        csi_final = 0;
        char ch = 0;
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch >= '@' && ch <= '~') { csi_final = ch; break; }
            csi_params.push_back(ch);
        }
    }

    if (seq[0] == '[' && csi_params == "200" && csi_final == '~') {
        handleBracketedPaste(st);
        return true;
    }

    if (st.modal_open) {
        if (n1 == 0) {
            if (st.modal_new_mode) { st.modal_new_mode = false; st.modal_new_name.clear(); }
            else st.modal_open = false;
        } else if (seq[0] == '[' && !st.modal_new_mode) {
            if (csi_final == 'A' && st.modal_idx > 0) --st.modal_idx;
            else if (csi_final == 'B' && st.modal_idx < st.modalMaxIdx()) ++st.modal_idx;
            else if (csi_params == "3" && csi_final == '~') st.deleteSelectedModalSheet();
        }
        return true;
    }

    if (n1 == 0) {
        st.namebox_editing = false;
        st.namebox_buffer.clear();
        st.clearSelection();
        st.mouse_dragging = false;
        enableMouseTracking(st.os);
        st.exitPointMode(true);
        st.setInput();
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.status_banner = "Modo Navegacion activo (usa flechas para moverte).";
        return true;
    }

    if (seq[0] == '[') {
        if (st.mode == SpreadsheetFocusMode::GridNavigation) handleGridEscapeKey(st, csi_params, csi_final);
        else handleConsoleEscapeKey(st, csi_params, csi_final);
    }
    return true;
}

inline void handleModalKey(TuiState& st, char ch) {
    if (st.modal_new_mode) {
        if (ch == '\n' || ch == '\r') {
            if (!st.modal_new_name.empty()) {
                st.engine.clear();
                st.config.current_sheet = st.modal_new_name;
                st.saveSheet(st.config.current_sheet);
                st.refreshSheetList();
                st.modal_new_mode = false;
                st.modal_new_name.clear();
                st.modal_open = false;
            }
        } else if (ch == 127 || ch == '\b') {
            if (!st.modal_new_name.empty()) st.modal_new_name.pop_back();
        } else if (texcel_detail::isChar<std::isalnum>(ch) || ch == '_' || ch == '-') {
            if (st.modal_new_name.size() < 24) st.modal_new_name.push_back(ch);
        }
        return;
    }

    auto executeModalItem = [&](TX idx) {
        if (idx == TuiState::MODAL_AUTOSAVE) {
            st.config.autosave = !st.config.autosave;
            st.config.saveConfig();
            st.engine.addLog(std::string("⚙ Autoguardado: ") + (st.config.autosave ? "ON" : "OFF"));
            st.status_banner = std::string("Autoguardado ") + (st.config.autosave ? "activado." : "desactivado.");
            if (st.config.autosave) st.saveSheet(st.config.current_sheet, true);
        } else if (idx == TuiState::MODAL_SAVE) {
            st.saveSheet(st.config.current_sheet);
            st.refreshSheetList();
            st.modal_open = false;
        } else if (idx == TuiState::MODAL_NEW) {
            st.modal_new_mode = true;
            st.modal_new_name.clear();
        } else if (idx >= TuiState::MODAL_FIRST_FILE) {
            TX fidx = idx - TuiState::MODAL_FIRST_FILE;
            if (fidx < static_cast<TX>(st.config.sheet_files.size())) {
                st.loadSheet(st.config.sheet_files[static_cast<size_t>(fidx)].getValue());
                st.modal_open = false;
            }
        }
    };

    switch (texcel_detail::toChar<std::tolower>(ch)) {
        case '\n': case '\r': case ' ': executeModalItem(st.modal_idx); break;
        case 'a': executeModalItem(TuiState::MODAL_AUTOSAVE); break;
        case 's': executeModalItem(TuiState::MODAL_SAVE); break;
        case 'n': executeModalItem(TuiState::MODAL_NEW); break;
        case 127: case '\b': st.deleteSelectedModalSheet(); break;
        case 'q': st.modal_open = false; break;
    }
}

inline void handleTabKey(TuiState& st) {
    if (st.mode == SpreadsheetFocusMode::GridNavigation) {
        st.exitPointMode();
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = false;
        if (st.input_buffer.empty()) st.loadActiveCellIntoBuffer();
        st.input_cursor = st.input_buffer.size();
        st.status_banner = "Consola activa. Escribe = B1 + 4, 5, o [Ctrl+U] para limpiar";
    } else {
        if (TuiState::isFormulaBuffer(st.input_buffer) && TuiState::canAcceptPointRef(st.input_buffer)) {
            st.point_mode = true;
            st.point_target = st.cursor;
            if (!st.input_buffer.empty() && st.input_buffer.back() != ' ' &&
                st.input_buffer.back() != '(' && st.input_buffer.back() != ':') {
                st.input_buffer.push_back(' ');
            }
            st.point_base = st.input_buffer;
            st.status_banner = "Point Mode activo: mueve con flechas para apuntar una celda.";
        } else {
            st.point_mode = false;
        }
        st.mode = SpreadsheetFocusMode::GridNavigation;
        if (!st.point_mode) {
            st.setInput();
            st.status_banner = "Foco en Hoja de Calculo. Muevete con flechas o pulsa Enter.";
        }
    }
}

inline bool handlePointModeKey(TuiState& st, char ch) {
    if (ch == '\n' || ch == '\r') {
        st.exitPointMode();
        if (!handleCommandSubmit(st, st.input_buffer)) return false;
        st.setInput();
    } else if (texcel_detail::isBinaryOpChar(ch) ||
               ch == '(' || ch == ')' || ch == ':' || ch == ',') {
        if (ch == ':' || ch == ')' || ch == ',') st.input_buffer.push_back(ch);
        else { st.input_buffer += " "; st.input_buffer.push_back(ch); st.input_buffer += " "; }
        st.point_base = st.input_buffer;
        if (ch == ')') {
            st.exitPointMode();
            st.mode = SpreadsheetFocusMode::CommandConsole;
            st.input_cursor = st.input_buffer.size();
        }
    } else if (ch == 127 || ch == '\b') {
        st.input_buffer = st.point_base;
    }
    return true;
}

inline bool handleGridKey(TuiState& st, char ch) {
    if (st.point_mode) return handlePointModeKey(st, ch);

    std::string cid = st.cursor.toId();
    switch (texcel_detail::toChar<std::tolower>(ch)) {
        case 'q': return false;
        case 'y':
            st.copySelectionToClipboard("✔ Copiado al portapapeles (Ctrl+C o Y).");
            break;
        case 'o':
            st.ast_horizontal = !st.ast_horizontal;
            st.resetAstScroll();
            st.status_banner = std::string("Orientacion AST: ") + (st.ast_horizontal ? "Horizontal [H]." : "Vertical [V].");
            break;
        case 'f': case '+':
            if (st.hasCellSel()) st.applySelectionAutoFill();
            break;
        case '=':
            st.mode = SpreadsheetFocusMode::CommandConsole;
            st.return_to_grid_on_submit = true;
            st.setInput("= ");
            st.status_banner = "Editando formula en " + cid + " (pulsa [Tab] para Point Mode).";
            break;
        case '\n': case '\r':
            st.mode = SpreadsheetFocusMode::CommandConsole;
            st.return_to_grid_on_submit = true;
            st.loadActiveCellIntoBuffer();
            st.status_banner = "Editando celda " + cid + " en vivo...";
            break;
        case ':':
            st.mode = SpreadsheetFocusMode::CommandConsole;
            st.return_to_grid_on_submit = false;
            st.setInput();
            st.status_banner = "Modo comando directo activado.";
            break;
        case 127: case '\b':
            st.clearSelectedCells();
            break;
        default:
            if (texcel_detail::isChar<std::isdigit>(ch)) {
                st.mode = SpreadsheetFocusMode::CommandConsole;
                st.return_to_grid_on_submit = true;
                st.setInput(std::string(1, ch));
            }
            break;
    }
    return true;
}

inline bool handleConsoleKey(TuiState& st, char ch) {
    if (ch == 21) { // Ctrl+U
        st.setInput();
    } else if (ch == '\n' || ch == '\r') {
        if (!handleCommandSubmit(st, st.input_buffer)) return false;
        st.setInput();
        if (st.return_to_grid_on_submit) {
            st.return_to_grid_on_submit = false;
            st.mode = SpreadsheetFocusMode::GridNavigation;
        }
    } else if (ch == 127 || ch == '\b') {
        if (st.input_cursor > 0 && st.input_cursor <= st.input_buffer.size()) {
            st.input_buffer.erase(st.input_cursor - 1, 1);
            --st.input_cursor;
        }
    } else if (texcel_detail::isChar<std::isprint>(ch)) {
        st.insertInputChar(ch);
        size_t eq = st.input_buffer.find('=');
        if (eq != std::string::npos) {
            CellCoord target = CellCoord::parse(st.input_buffer.substr(0, eq));
            if (target.valid) st.cursor = target;
        }
    }
    return true;
}

inline void handleNameBoxKey(TuiState& st, char ch) {
    if (ch == '\n' || ch == '\r') {
        if (!st.namebox_buffer.empty()) {
            st.goToCellOrRange(st.namebox_buffer);
        }
        st.namebox_editing = false;
        st.namebox_buffer.clear();
    } else if (ch == 127 || ch == '\b') {
        if (!st.namebox_buffer.empty()) st.namebox_buffer.pop_back();
    } else if (texcel_detail::isChar<std::isalnum>(ch) || ch == ':' || ch == '$') {
        if (st.namebox_buffer.size() < 16) {
            st.namebox_buffer.push_back(texcel_detail::toChar<std::toupper>(ch));
        }
    }
}

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_TUI_INPUT_H__
