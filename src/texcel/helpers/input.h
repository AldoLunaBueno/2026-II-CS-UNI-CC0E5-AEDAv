/**
 * @file input.h
 * @ingroup texcel
 * @brief Despachadores de entrada (teclado con lookahead `poll()`, ratón SGR, HitMap, búsqueda, Undo/Copy y modales).
 */

#ifndef __TEXCEL_HELPERS_INPUT_H__
#define __TEXCEL_HELPERS_INPUT_H__

#include "state.h"
#include "text.h"
#include "../../utilities/gotoxy.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <poll.h>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>

namespace texcel_tui {

inline bool readByteWithTimeout(char& out, int timeout_ms) {
    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
    int pr = ::poll(&pfd, 1, timeout_ms);
    if (pr <= 0 || !(pfd.revents & POLLIN)) return false;
    return ::read(STDIN_FILENO, &out, 1) == 1;
}

inline bool handleCommandSubmit(TuiState& st, const std::string& raw_cmd) {
    std::string cmd = texcel_detail::trim(raw_cmd);
    if (cmd.empty()) return true;

    st.cmd_history.push_back(cmd);
    st.history_idx = static_cast<TX>(st.cmd_history.size());

    std::string upper = texcel_detail::toUpper(cmd);
    if (upper == "QUIT" || upper == "EXIT" || upper == "Q") return false;

    if (upper == "UNDO") { st.performUndo(); return true; }
    if (upper == "REDO") { st.performRedo(); return true; }
    if (upper == "AUTOFIT") { st.autoFitAllColumns(); return true; }

    if (upper == "CLEAR") {
        st.recordTransaction("Limpiar hoja", st.cursor, [&]() {
            st.engine.clear();
        });
        st.status_banner = "✔ Hoja limpiada ([Ctrl+Z] para deshacer).";
        return true;
    }
    if (upper == "SAVE") { st.saveSheet(st.config.current_sheet); return true; }
    if (upper == "LOAD") { st.loadSheet(st.config.current_sheet); return true; }

    std::string assigned_id;
    bool ok = false;
    CellCoord before_cur = st.cursor;
    {
        TexcelLoadingSpinner<TuiState> spinner(st, "Evaluando " + st.cursor.toId() + "…");
        st.recordTransaction("Editar celda", before_cur, [&]() {
            ok = st.engine.executeCommand(cmd, st.cursor.toId(), &assigned_id);
            CellCoord dest = CellCoord::parse(assigned_id);
            if (dest.valid) st.cursor = dest;
        });
    }
    CellCoord dest = CellCoord::parse(assigned_id);
    if (dest.valid) {
        st.cursor = dest;
        st.ensureCursorVisible();
    }
    const SpreadsheetCell* updated = st.engine.findCell(assigned_id);
    std::string disp = updated ? updated->displayValue() : "";
    st.status_banner = ok
        ? ("✔ " + assigned_id + " = " + disp)
        : ("⚠ Error en " + assigned_id + " (" + (disp.empty() ? "sintaxis" : disp) + ")");
    return true;
}

inline bool handleMouseWheel(TuiState& st, TX btn, TX mx, TX my) {
    bool in_inspector = (mx >= st.right_x && mx < st.right_x + st.right_w &&
                         my >= TuiState::TOP_Y && my < TuiState::TOP_Y + st.top_h);
    bool is_horizontal_mod = (btn & (2 | 4 | 8 | 16)) != 0;
    bool forward = (btn & 1) != 0;
    if (in_inspector) {
        constexpr TX AST_V_STEP = 1, AST_H_STEP = 4;
        bool scroll_h = is_horizontal_mod || (st.ast_graph_h <= st.ast_view_h && st.ast_graph_w > st.astViewWidth());
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
            st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C] copiar, [Ctrl+V] pegar, [Supr] borrar).";
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
            else {
                st.point_anchor = st.sel_a;
                st.cursor = st.sel_b;
                st.point_has_range = (st.sel_a != st.sel_b);
                st.setInput(st.point_base + ref);
            }
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

inline bool tryToggleBooleanCell(TuiState& st, TX gc, TX gr, TX rel_x_in_cell, TX cell_w) {
    TX toggle_col = 2 + ((std::max<TX>(3, cell_w) - 3) / 2); // Glifo centrado ('■' / '□') en alignCell
    if (rel_x_in_cell != toggle_col) return false;
    const SpreadsheetCell* cell = st.engine.findCellAt(gc, gr);
    if (!cell || cell->is_formula || cell->has_error || !cell->result.is_text) return false;
    std::string trim_raw = texcel_detail::trim(cell->raw_expression);
    if (!texcel_detail::equalsIgnoreCase(trim_raw, "TRUE") && !texcel_detail::equalsIgnoreCase(trim_raw, "FALSE")) {
        return false;
    }

    std::string cid = cell->id;
    std::string next_val = texcel_detail::equalsIgnoreCase(trim_raw, "TRUE") ? "FALSE" : "TRUE";
    {
        TexcelLoadingSpinner<TuiState> spinner(st, "Propagando cambios…");
        st.recordTransaction("Alternar " + cid, st.cursor, [&]() {
            st.engine.setCell(cid, next_val);
        });
    }
    st.clearSelection();
    st.cursor = {gc, gr, true};
    st.mode = SpreadsheetFocusMode::GridNavigation;
    st.setInput();
    st.status_banner = "✔ " + cid + " = " + next_val;
    return true;
}

inline bool handleGridMouseDown(TuiState& st, TX, TX, TX gc, TX gr, TX rel_x_in_cell, TX cell_w,
                                bool is_ctrl_click, bool is_extend_click) {
    st.search.close();
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

    if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.point_mode && !st.input_buffer.empty()) {
        handleCommandSubmit(st, st.input_buffer);
    }

    if (!st.point_mode && !is_ctrl_click && !is_extend_click && tryToggleBooleanCell(st, gc, gr, rel_x_in_cell, cell_w)) {
        return true;
    }

    st.drag_win = TuiState::WIN_GRID;
    st.mouse_dragging = true;
    bool on_active_corner = (gc == st.cursor.col && gr == st.cursor.row && rel_x_in_cell == cell_w);

    if ((is_ctrl_click || on_active_corner) && !st.point_mode) {
        st.sel_kind = SelectionKind::Fill;
        st.sel_a = st.cursor;
        st.sel_b = {gc, gr, true};
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.setInput();
        st.status_banner = "Auto-Fill: arrastra hacia cualquier dirección y suelta para rellenar.";
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
        st.clearSelectionBanner();
    }

    if (st.point_mode) {
        st.point_anchor = st.sel_a;
        st.point_has_range = (st.sel_a != st.sel_b);
        st.setInput(st.point_base + st.selectedRangeStr(true));
    } else {
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.setInput();
    }
    return true;
}

inline void executeSheetsModalItem(TuiState& st, TX idx) {
    if (idx == TuiState::MODAL_AUTOSAVE) {
        st.config.autosave = !st.config.autosave;
        st.config.saveConfig();
        st.engine.addLog(std::string("⚙ Autoguardado: ") + (st.config.autosave ? "ON" : "OFF"));
        st.status_banner = std::string("Autoguardado ") + (st.config.autosave ? "activado." : "desactivado.");
        if (st.config.autosave) st.saveSheet(st.config.current_sheet, true);
    } else if (idx == TuiState::MODAL_SAVE) {
        st.saveSheet(st.config.current_sheet);
        st.refreshSheetList();
        st.closeModal();
    } else if (idx == TuiState::MODAL_NEW) {
        st.modal_new_mode = true;
        st.modal_new_name.clear();
    } else if (idx >= TuiState::MODAL_FIRST_FILE) {
        TX fidx = idx - TuiState::MODAL_FIRST_FILE;
        if (fidx < static_cast<TX>(st.config.sheet_files.size())) {
            st.loadSheet(st.config.sheet_files[static_cast<size_t>(fidx)].getValue());
            st.closeModal();
        }
    }
}

inline bool handleModalMouse(TuiState& st, TX btn, TX mx, TX my, bool is_release, bool is_drag) {
    if (btn & 64) {
        bool forward = (btn & 1) != 0;
        if (st.modal_kind == ModalKind::Functions) {
            auto filtered = st.filteredFunctionIndices();
            if (!filtered.empty()) {
                st.fx_idx = std::clamp<TX>(st.fx_idx + (forward ? 1 : -1), 0, static_cast<TX>(filtered.size()) - 1);
                st.clampFxScroll();
            }
            return true;
        }
        if (st.modal_kind == ModalKind::Help) {
            st.help_scroll += forward ? 1 : -1;
            st.clampHelpScroll();
            return true;
        }
        if (st.modal_kind == ModalKind::Sheets && !st.modal_new_mode) {
            st.modal_idx = std::clamp<TX>(st.modal_idx + (forward ? 1 : -1), 0, st.modalMaxIdx());
            return true;
        }
        return false;
    }
    if ((btn & 3) != 0 || is_release || is_drag) return false;

    ModalRect mr = st.modalRect();
    if (mx < mr.x || mx >= mr.x + mr.w || my < mr.y || my >= mr.y + mr.h) {
        st.closeModal();
        return true;
    }

    if (st.isFxHelpModal()) {
        if (my == mr.y + 1) {
            if (mx >= mr.x + 2 && mx <= mr.x + 18) { st.openModal(ModalKind::Functions); return true; }
            if (mx >= mr.x + 19 && mx <= mr.x + 32) { st.openModal(ModalKind::Help); return true; }
        }
        if (st.modal_kind == ModalKind::Functions &&
            my >= mr.y + 3 && my < mr.y + 3 + TuiState::FX_LIST_ROWS) {
            auto filtered = st.filteredFunctionIndices();
            TX clicked = st.fx_scroll + (my - (mr.y + 3));
            if (clicked >= 0 && clicked < static_cast<TX>(filtered.size())) {
                if (clicked == st.fx_idx) st.insertSelectedFunction();
                else { st.fx_idx = clicked; st.clampFxScroll(); }
                return true;
            }
        }
        return false;
    }

    if (st.modal_kind == ModalKind::Sheets && !st.modal_new_mode) {
        TX rel_y = my - mr.y;
        TX clicked_idx = -1;
        if (rel_y >= 1 && rel_y <= 3) {
            clicked_idx = rel_y - 1;
        } else if (rel_y >= 5 && rel_y < mr.h - 3 && st.config.sheet_files.size() > 0) {
            const TX list_rows = mr.h - 8;
            TX file_cursor = (st.modal_idx >= TuiState::MODAL_FIRST_FILE) ? (st.modal_idx - TuiState::MODAL_FIRST_FILE) : 0;
            TX scroll_f = (file_cursor >= list_rows) ? (file_cursor - list_rows + 1) : 0;
            TX fidx = scroll_f + (rel_y - 5);
            if (fidx < static_cast<TX>(st.config.sheet_files.size())) {
                clicked_idx = TuiState::MODAL_FIRST_FILE + fidx;
            }
        }
        if (clicked_idx >= 0) {
            if (clicked_idx == st.modal_idx) executeSheetsModalItem(st, clicked_idx);
            else st.modal_idx = clicked_idx;
            return true;
        }
    }
    return false;
}

inline bool handleSgrMouse(TuiState& st, const std::string& payload, char mc) {
    TX btn = 0, mx_1 = 1, my_1 = 1;
    char s1 = 0, s2 = 0;
    std::istringstream mss(payload);
    if (!(mss >> btn >> s1 >> mx_1 >> s2 >> my_1)) return false;

    TX mx = mx_1 - 1, my = my_1 - 1;
    bool is_release = (mc == 'm');
    bool is_drag    = (btn & 32) != 0;

    if (st.isModalOpen()) return handleModalMouse(st, btn, mx, my, is_release, is_drag);

    if (btn & 64) return handleMouseWheel(st, btn, mx, my);

    if (is_release && st.isAnyDragging()) {
        if (st.scroll_drag == ScrollDrag::ColResize) {
            st.config.saveConfig();
            st.clampScroll();
        }
        st.clearScrollDragging();
        if (st.mouse_dragging) finishMouseDrag(st);
        return true;
    }
    if ((btn & 3) == 2 && !is_drag && !is_release) {
        return st.copySelectionToClipboard("✔ Selección copiada al portapapeles (Clic derecho).");
    }
    if (is_drag) return handleMouseDrag(st, mx, my);
    if ((btn & 3) != 0 || is_release) return false;

    if (st.isHeaderHelpBtnPos(mx, my)) {
        if (st.modal_kind == ModalKind::Help) st.closeModal();
        else st.openModal(ModalKind::Help);
        return true;
    }
    if (st.isHeaderExitBtnPos(mx, my)) {
        st.should_exit = true;
        return false;
    }
    if (st.hits.header_sheet_btn.contains(mx, my)) {
        st.openModal(ModalKind::Sheets);
        return true;
    }
    if (st.hits.header_auto_btn.contains(mx, my)) {
        executeSheetsModalItem(st, TuiState::MODAL_AUTOSAVE);
        return true;
    }
    if (st.isFxButtonPos(mx, my)) {
        st.openModal(ModalKind::Functions);
        return true;
    }
    if (st.isAstVerticalRadioPos(mx, my)) {
        if (st.ast_horizontal) {
            st.ast_horizontal = false;
            st.resetAstScroll();
        }
        return true;
    }
    if (st.isAstHorizontalRadioPos(mx, my)) {
        if (!st.ast_horizontal) {
            st.ast_horizontal = true;
            st.resetAstScroll();
        }
        return true;
    }

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

    // Redimensionamiento o auto-ajuste (doble clic) sobre divisores de cabecera de columnas
    TX div_col = -1;
    if (st.screenToColHeaderDivider(mx, my, div_col)) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - st.last_div_click).count();
        st.last_div_click = now;
        if (st.last_div_col == div_col && ms > 0 && ms < 400) {
            st.last_div_col = -1;
            st.autoFitColumn(div_col);
        } else {
            st.last_div_col = div_col;
            st.scroll_drag = ScrollDrag::ColResize;
            st.resize_col = div_col;
            st.resize_start_x = mx;
            st.resize_start_w = st.colWidth(div_col);
            st.status_banner = "Arrastra para ajustar columna " + CellCoord::colToLabel(div_col) + " (doble clic auto-ajusta).";
        }
        return true;
    }

    if (st.tryStartScrollbarDrag(mx, my)) return true;

    bool is_ctrl_click   = (btn & 16) != 0;
    bool is_extend_click = (btn & (4 | 8)) != 0;

    TX gc = 0, gr = 0, rel_x = 1, cw = TuiState::CELL_WIDTH;
    if (st.screenToGridCell(mx, my, gc, gr, false, &rel_x, &cw)) {
        return handleGridMouseDown(st, mx, my, gc, gr, rel_x, cw, is_ctrl_click, is_extend_click);
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
            st.search.close();
            st.mode = SpreadsheetFocusMode::CommandConsole;
            if (st.input_buffer.empty()) st.loadActiveCellIntoBuffer();
        }
        return true;
    }
    return false;
}

inline void applyBracketedPaste(TuiState& st, const std::string& pasted) {
    if (st.isModalOpen()) return;
    if (st.search.active) {
        for (char c : pasted) {
            if (c == '\n' || c == '\r') break;
            if (texcel_detail::isChar<std::isprint>(c)) st.search.query.push_back(c);
        }
        st.refreshSearchMatches(true);
        return;
    }
    if (st.mode == SpreadsheetFocusMode::CommandConsole) {
        for (char c : pasted) {
            if (c == '\n' || c == '\r') break;
            if (texcel_detail::isChar<std::isprint>(c)) st.insertInputChar(c);
        }
        return;
    }
    st.pasteExternalTsvAtCursor(pasted);
}

inline void handleBracketedPaste(TuiState& st) {
    std::string pasted;
    char pch = 0;
    while (readByteWithTimeout(pch, 50)) {
        pasted.push_back(pch);
        if (pasted.size() >= 6 && pasted.ends_with(BRACKETED_PASTE_END)) {
            pasted.resize(pasted.size() - 6);
            break;
        }
    }
    applyBracketedPaste(st, pasted);
}

inline void enterEditActiveCell(TuiState& st) {
    st.exitPointMode();
    st.search.close();
    st.mode = SpreadsheetFocusMode::CommandConsole;
    st.return_to_grid_on_submit = true;
    st.loadActiveCellIntoBuffer();
    st.status_banner.clear();
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

    // Alt + Flecha Izq/Der -> ajustar ancho de columna activa y persistir en .config
    if (csi_params == "1;3" && (csi_final == 'C' || csi_final == 'D') && !st.point_mode) {
        st.adjustActiveColWidth(csi_final == 'C' ? 2 : -2);
        return;
    }

    // Ctrl + Flechas -> saltar al extremo de datos contiguos estilo Excel
    if (csi_params == "1;5" && is_arrow) {
        int dc = (csi_final == 'C') ? 1 : ((csi_final == 'D') ? -1 : 0);
        int dr = (csi_final == 'B') ? 1 : ((csi_final == 'A') ? -1 : 0);
        st.jumpDataEdge(dc, dr);
        if (st.point_mode) { st.point_has_range = false; st.updatePointedCell(); }
        return;
    }

    // Shift + Flechas -> selección rectangular (tanto en modo Hoja normal como en Point Mode)
    if (csi_params == "1;2" && is_arrow) {
        if (st.point_mode) {
            if (!st.point_has_range) {
                st.point_anchor = st.cursor;
                st.point_has_range = true;
            }
            stepCell(st.cursor, csi_final);
            st.sel_kind = SelectionKind::Cells;
            st.sel_a = st.point_anchor;
            st.sel_b = st.cursor;
            st.ensureCursorVisible();
            st.updatePointedCell();
            return;
        }
        if (!st.hasCellSel()) {
            st.sel_a = st.sel_b = st.cursor;
            st.sel_kind = SelectionKind::Cells;
        }
        stepCell(st.sel_b, csi_final);
        st.ensureCoordVisible(st.sel_b);
        st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C] o [Y] para copiar).";
        return;
    }

    if (csi_params.empty() && is_arrow) {
        st.clearSelection();
        st.point_has_range = false;
        if (stepCell(st.cursor, csi_final)) st.resetAstScroll();
        st.ensureCursorVisible();
        if (st.point_mode) st.updatePointedCell();
        return;
    }

    // RePág (5~) / AvPág (6~)
    if ((csi_params == "5" || csi_params == "6") && csi_final == '~') {
        TX delta = (csi_params == "5") ? -st.visible_rows : st.visible_rows;
        st.clearSelection();
        st.cursor.row = std::clamp<TX>(st.cursor.row + delta, 0, CellCoord::MAX_ROWS - 1);
        st.scroll_row = std::clamp<TX>(st.scroll_row + delta, 0, std::max<TX>(0, CellCoord::MAX_ROWS - st.visible_rows));
        st.resetAstScroll();
        st.ensureCursorVisible();
        if (st.point_mode) { st.point_has_range = false; st.updatePointedCell(); }
        return;
    }

    // Home / Ctrl+Home / End
    if (csi_final == 'H' || ((csi_params == "1" || csi_params == "7") && csi_final == '~')) {
        st.clearSelection();
        st.cursor.col = 0;
        if (csi_params == "1;5") st.cursor.row = 0;
        st.ensureCursorVisible();
        if (st.point_mode) { st.point_has_range = false; st.updatePointedCell(); }
        return;
    }
    if (csi_final == 'F' || ((csi_params == "4" || csi_params == "8") && csi_final == '~')) {
        st.clearSelection();
        st.cursor.col = std::max<TX>(0, st.engine.maxUsedCol());
        if (csi_params == "1;5") st.cursor.row = std::max<TX>(0, st.engine.maxUsedRow());
        st.ensureCursorVisible();
        if (st.point_mode) { st.point_has_range = false; st.updatePointedCell(); }
        return;
    }

    // F2 (12~) -> editar celda activa sin sobrescribirla
    if (csi_params == "12" && csi_final == '~' && !st.point_mode) {
        enterEditActiveCell(st);
        return;
    }

    // Supr (3~)
    if (csi_params == "3" && csi_final == '~' && !st.point_mode) {
        st.clearSelectedCells();
    }
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

inline void applyStandaloneEscape(TuiState& st) {
    if (st.isModalOpen()) {
        if (st.modal_new_mode) {
            st.modal_new_mode = false;
            st.modal_new_name.clear();
        } else {
            st.closeModal();
        }
        return;
    }
    if (st.search.active) {
        st.search.close();
        st.status_banner.clear();
        return;
    }
    st.namebox_editing = false;
    st.namebox_buffer.clear();
    st.clearSelection();
    st.mouse_dragging = false;
    st.clearScrollDragging();
    enableMouseTracking(st.os);
    st.exitPointMode(true);
    st.setInput();
    st.mode = SpreadsheetFocusMode::GridNavigation;
    st.status_banner.clear();
}

/**
 * @brief Interpreta secuencias `ESC`/CSI/SS3 y eventos de ratón SGR mediante `poll()`, encolando en `pending_key` cualquier tecla posterior.
 */
inline bool handleEscapeSequence(TuiState& st, termios&) {
    char seq0 = 0;
    if (!readByteWithTimeout(seq0, 25)) {
        applyStandaloneEscape(st);
        return true;
    }

    // Si el siguiente byte NO inicia CSI ('[') ni SS3 ('O'), entonces fue un Esc genuino seguido rápido de otra tecla:
    // aplicamos el Esc y encolamos `seq0` para procesarlo intacto en la siguiente iteración del bucle principal.
    if (seq0 != '[' && seq0 != 'O') {
        applyStandaloneEscape(st);
        st.has_pending_key = true;
        st.pending_key = seq0;
        return true;
    }

    if (seq0 == 'O') {
        char ss3 = 0;
        if (!readByteWithTimeout(ss3, 25)) {
            applyStandaloneEscape(st);
            return true;
        }
        if (st.isModalOpen()) return true;
        if (ss3 == 'P') { st.openModal(ModalKind::Help); return true; } // F1
        if (ss3 == 'Q' && st.mode == SpreadsheetFocusMode::GridNavigation && !st.point_mode) {
            enterEditActiveCell(st); // F2
            return true;
        }
        if (st.mode == SpreadsheetFocusMode::GridNavigation) handleGridEscapeKey(st, "", ss3);
        else handleConsoleEscapeKey(st, "", ss3);
        return true;
    }

    // Secuencia CSI: ESC '[' ...
    char seq1 = 0;
    if (!readByteWithTimeout(seq1, 25)) {
        applyStandaloneEscape(st);
        return true;
    }

    if (seq1 == '<') {
        std::string payload;
        char mc = 0;
        while (readByteWithTimeout(mc, 25)) {
            if (mc == 'M' || mc == 'm') break;
            payload.push_back(mc);
        }
        return handleSgrMouse(st, payload, mc);
    }

    std::string csi_params;
    char csi_final = seq1;
    if (seq1 < '@' || seq1 > '~') {
        csi_params.push_back(seq1);
        csi_final = 0;
        char ch = 0;
        while (readByteWithTimeout(ch, 25)) {
            if (ch >= '@' && ch <= '~') { csi_final = ch; break; }
            csi_params.push_back(ch);
        }
    }

    if (csi_params == "200" && csi_final == '~') {
        handleBracketedPaste(st);
        return true;
    }

    if (st.isModalOpen()) {
        if (st.modal_kind == ModalKind::Sheets && !st.modal_new_mode) {
            if (csi_final == 'A' && st.modal_idx > 0) --st.modal_idx;
            else if (csi_final == 'B' && st.modal_idx < st.modalMaxIdx()) ++st.modal_idx;
            else if (csi_params == "3" && csi_final == '~') st.deleteSelectedModalSheet();
        } else if (st.modal_kind == ModalKind::Functions) {
            auto filtered = st.filteredFunctionIndices();
            TX fcnt = static_cast<TX>(filtered.size());
            if (csi_final == 'A' && st.fx_idx > 0) { --st.fx_idx; st.clampFxScroll(); }
            else if (csi_final == 'B' && st.fx_idx + 1 < fcnt) { ++st.fx_idx; st.clampFxScroll(); }
            else if (csi_final == 'C' || csi_final == 'D') st.openModal(ModalKind::Help);
        } else if (st.modal_kind == ModalKind::Help) {
            if (csi_final == 'A') { --st.help_scroll; st.clampHelpScroll(); }
            else if (csi_final == 'B') { ++st.help_scroll; st.clampHelpScroll(); }
            else if (csi_final == 'C' || csi_final == 'D') st.openModal(ModalKind::Functions);
        }
        return true;
    }

    if (st.search.active) {
        if (csi_final == 'B') st.stepSearchMatch(1);
        else if (csi_final == 'A') st.stepSearchMatch(-1);
        return true;
    }

    if (st.mode == SpreadsheetFocusMode::GridNavigation) handleGridEscapeKey(st, csi_params, csi_final);
    else handleConsoleEscapeKey(st, csi_params, csi_final);
    return true;
}

inline void handleModalKey(TuiState& st, char ch) {
    if (st.modal_kind == ModalKind::Functions) {
        if (ch == '\t') { st.openModal(ModalKind::Help); return; }
        if (ch == '\n' || ch == '\r') { st.insertSelectedFunction(); return; }
        if (ch == 21) { st.modal_filter.clear(); st.fx_idx = 0; st.clampFxScroll(); return; } // Ctrl+U
        if (ch == 127 || ch == '\b') {
            if (!st.modal_filter.empty()) {
                st.modal_filter.pop_back();
                st.fx_idx = 0;
                st.clampFxScroll();
            }
            return;
        }
        if (texcel_detail::isChar<std::isprint>(ch)) {
            if (st.modal_filter.size() < 20) {
                st.modal_filter.push_back(ch);
                st.fx_idx = 0;
                st.clampFxScroll();
            }
        }
        return;
    }
    if (st.modal_kind == ModalKind::Help) {
        if (ch == '\t') { st.openModal(ModalKind::Functions); return; }
        if (texcel_detail::toChar<std::tolower>(ch) == 'q' || ch == '?') st.closeModal();
        return;
    }

    if (st.modal_new_mode) {
        if (ch == '\n' || ch == '\r') {
            if (!st.modal_new_name.empty()) {
                st.engine.clear();
                st.undo_mgr.clear();
                st.config.current_sheet = st.modal_new_name;
                st.saveSheet(st.config.current_sheet);
                st.refreshSheetList();
                st.closeModal();
            }
        } else if (ch == 127 || ch == '\b') {
            if (!st.modal_new_name.empty()) st.modal_new_name.pop_back();
        } else if (texcel_detail::isChar<std::isalnum>(ch) || ch == '_' || ch == '-') {
            if (st.modal_new_name.size() < 24) st.modal_new_name.push_back(ch);
        }
        return;
    }

    switch (texcel_detail::toChar<std::tolower>(ch)) {
        case '\n': case '\r': case ' ': executeSheetsModalItem(st, st.modal_idx); break;
        case 'a': executeSheetsModalItem(st, TuiState::MODAL_AUTOSAVE); break;
        case 's': executeSheetsModalItem(st, TuiState::MODAL_SAVE); break;
        case 'n': executeSheetsModalItem(st, TuiState::MODAL_NEW); break;
        case 127: case '\b': st.deleteSelectedModalSheet(); break;
        case 'q': st.closeModal(); break;
    }
}

inline void handleTabKey(TuiState& st) {
    if (st.search.active) { st.stepSearchMatch(1); return; }
    if (st.mode == SpreadsheetFocusMode::GridNavigation) {
        st.exitPointMode();
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = false;
        if (st.input_buffer.empty()) st.loadActiveCellIntoBuffer();
        st.input_cursor = st.input_buffer.size();
        st.status_banner.clear();
    } else {
        if (TuiState::isFormulaBuffer(st.input_buffer) && TuiState::canAcceptPointRef(st.input_buffer)) {
            st.point_mode = true;
            st.point_has_range = false;
            st.point_target = st.cursor;
            st.point_anchor = st.cursor;
            if (!st.input_buffer.empty() && st.input_buffer.back() != ' ' &&
                st.input_buffer.back() != '(' && st.input_buffer.back() != ':') {
                st.input_buffer.push_back(' ');
            }
            st.point_base = st.input_buffer;
            st.status_banner.clear();
        } else {
            st.point_mode = false;
        }
        st.mode = SpreadsheetFocusMode::GridNavigation;
        if (!st.point_mode) {
            st.setInput();
            st.status_banner.clear();
        }
    }
}

inline bool handlePointModeKey(TuiState& st, char ch) {
    if (ch == '\n' || ch == '\r') {
        st.clearSelection();
        st.exitPointMode();
        if (!handleCommandSubmit(st, st.input_buffer)) return false;
        st.setInput();
    } else if (texcel_detail::isBinaryOpChar(ch) ||
               ch == '(' || ch == ')' || ch == ':' || ch == ',') {
        st.clearSelection();
        st.point_has_range = false;
        if (ch == ':' || ch == ')') st.input_buffer.push_back(ch);
        else if (ch == ',') st.input_buffer += ", ";
        else { st.input_buffer += " "; st.input_buffer.push_back(ch); st.input_buffer += " "; }
        st.point_base = st.input_buffer;
        if (ch == ')') {
            st.exitPointMode();
            st.mode = SpreadsheetFocusMode::CommandConsole;
            st.input_cursor = st.input_buffer.size();
        }
    } else if (ch == 127 || ch == '\b') {
        st.clearSelection();
        st.point_has_range = false;
        st.input_buffer = st.point_base;
    }
    return true;
}

inline void handleSearchKey(TuiState& st, char ch) {
    if (ch == '\n' || ch == '\r') {
        st.stepSearchMatch(1);
    } else if (ch == 21) { // Ctrl+U
        st.search.query.clear();
        st.refreshSearchMatches(false);
    } else if (ch == 127 || ch == '\b') {
        if (!st.search.query.empty()) {
            st.search.query.pop_back();
            st.refreshSearchMatches(true);
        }
    } else if (texcel_detail::isChar<std::isprint>(ch)) {
        if (st.search.query.size() < 40) {
            st.search.query.push_back(ch);
            st.refreshSearchMatches(true);
        }
    }
}

/**
 * @brief Despacha teclas en `GridNavigation`: `=` o `Enter` inician edición, `Backspace` borra e imprimibles escriben directo en la celda.
 */
inline bool handleGridKey(TuiState& st, char ch) {
    if (st.point_mode) return handlePointModeKey(st, ch);

    if (ch == '=') {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.setInput("= ");
        st.status_banner.clear();
    } else if (ch == '\n' || ch == '\r') {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.loadActiveCellIntoBuffer();
        st.status_banner.clear();
    } else if (ch == 127 || ch == '\b') {
        st.clearSelectedCells();
    } else if (texcel_detail::isChar<std::isprint>(ch)) {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.setInput(std::string(1, ch));
        st.status_banner.clear();
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
            CellCoord target = CellCoord::parse(std::string_view(st.input_buffer).substr(0, eq));
            if (target.valid) st.cursor = target;
        }
    }
    return true;
}

inline void handleChordXKey(TuiState& st, char ch) {
    st.chord_x = false;
    switch (texcel_detail::toChar<std::tolower>(ch)) {
        case 'f': case 'x':
            st.openModal(ModalKind::Functions);
            break;
        case 'h': case 'o':
            st.openModal(ModalKind::Sheets);
            break;
        case 's':
            st.saveSheet(st.config.current_sheet);
            st.refreshSheetList();
            break;
        case 'g':
            st.beginNameBoxEdit();
            break;
        case 'b': case '/':
            st.beginSearchMode();
            break;
        case 'a': case '?':
            st.openModal(ModalKind::Help);
            break;
        case 'z': case 'u':
            st.performUndo();
            break;
        case 'y': case 'r':
            st.performRedo();
            break;
        case 'c':
            st.copySelectionToClipboard();
            break;
        case 'k':
            st.copySelectionToClipboard("✂ Selección cortada — navega al destino y pulsa [Ctrl+V].", true);
            break;
        case 'v':
            st.pasteClipboardAtCursor();
            break;
        case '=':
            if (st.hasCellSel()) {
                TX cmin, rmin, cmax, rmax;
                normalizeCellRange(st.sel_a.col, st.sel_a.row, st.sel_b.col, st.sel_b.row, cmin, rmin, cmax, rmax);
                for (TX c = cmin; c <= cmax; ++c) st.autoFitColumn(c, false);
                st.config.saveConfig();
                st.recomputeVisibleCols();
                st.clampScroll();
                st.status_banner = "✔ Columnas del rango auto-ajustadas.";
            } else {
                st.autoFitColumn(st.cursor.col);
            }
            break;
        case '+':
            st.adjustActiveColWidth(2);
            break;
        case '-':
            st.adjustActiveColWidth(-2);
            break;
        default:
            break;
    }
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

#endif // __TEXCEL_HELPERS_INPUT_H__
