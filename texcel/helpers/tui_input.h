/**
 * @file tui_input.h
 * @ingroup texcel
 * @brief Despachadores independientes de entrada (teclado, ratón SGR, modal, Point Mode y comandos) sobre `TuiState`.
 */

#ifndef __TEXCEL_HELPERS_TUI_INPUT_H__
#define __TEXCEL_HELPERS_TUI_INPUT_H__

#include "tui_state.h"
#include "tui_text.h"
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
    if (upper == "DEMO") {
        st.engine.clear();
        st.engine.loadProfessorDemo();
        st.maybeAutoSave();
        st.cursor_col = 3;
        st.cursor_row = 3;
        st.ensureCursorVisible();
        st.status_banner = "Cargado ejemplo de clase: A1=5, B1=A1+4, D4=A1+B1";
        return true;
    }
    if (upper == "SAVE") { st.saveSheet(st.config.current_sheet); return true; }
    if (upper == "LOAD") { st.loadSheet(st.config.current_sheet); return true; }

    std::string current_id = CellCoord(st.cursor_col, st.cursor_row, true).toId();
    std::string assigned_id;
    bool ok = st.engine.executeCommand(cmd, current_id, &assigned_id);
    if (ok) st.maybeAutoSave();

    CellCoord dest = CellCoord::parse(assigned_id);
    if (dest.valid) {
        st.cursor_col = dest.col;
        st.cursor_row = dest.row;
        st.ensureCursorVisible();
    }
    st.status_banner = ok
        ? ("Actualizada " + assigned_id + " = " + st.engine.getCell(assigned_id).displayValue())
        : ("Error al evaluar " + assigned_id + ": verifica sintaxis o ciclos.");
    return true;
}

inline bool handleSgrMouse(TuiState& st, const std::string& payload, char mc) {
    TX btn = 0, mx_1 = 1, my_1 = 1;
    char s1 = 0, s2 = 0;
    std::istringstream mss(payload);
    if (!(mss >> btn >> s1 >> mx_1 >> s2 >> my_1)) return false;

    TX mx = mx_1 - 1;
    TX my = my_1 - 1;
    bool is_release = (mc == 'm');
    bool is_drag    = ((btn & 32) != 0);
    bool is_wheel   = ((btn & 64) != 0);

    if (st.modal_open) return false;

    if (is_wheel) {
        bool in_inspector = (mx >= st.right_x && mx < st.right_x + st.right_w &&
                             my >= TuiState::TOP_Y && my < TuiState::TOP_Y + TuiState::TOP_H);
        bool is_horizontal_mod = ((btn & 2) != 0) || ((btn & 4) != 0) ||
                                 ((btn & 8) != 0) || ((btn & 16) != 0);
        bool forward = ((btn & 1) != 0);
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
        bool is_horizontal = on_col_header || is_horizontal_mod;
        if (is_horizontal) st.scroll_col += forward ? H_STEP : -H_STEP;
        else st.scroll_row += forward ? V_STEP : -V_STEP;
        st.clampScroll();
        return true;
    }

    auto finishDrag = [&]() {
        st.mouse_dragging = false;
        if (st.drag_win == 1 && st.fill_dragging) {
            st.fill_dragging = false;
            st.has_fill_sel = false;
            if (st.sel_c0 != st.sel_c1 || st.sel_r0 != st.sel_r1) {
                st.applySelectionAutoFill();
            }
        } else if (st.drag_win == 1 && st.console_ref_inserting) {
            st.console_ref_inserting = false;
            st.has_cell_sel = false;
        } else if (st.drag_win == 1 && st.has_cell_sel) {
            if (st.sel_c0 != st.sel_c1 || st.sel_r0 != st.sel_r1) {
                st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C], [Y] o Clic Der. para copiar).";
            } else {
                st.has_cell_sel = false;
            }
        } else if (st.drag_win >= 2 && st.drag_win <= 4 && st.has_text_sel) {
            if (st.tx0 == st.tx1 && st.ty0 == st.ty1) {
                st.has_text_sel = false;
            } else {
                st.status_banner = "Texto sombreado ([Ctrl+C] o Clic Der. para copiar).";
            }
        }
        st.drag_win = 0;
    };

    if (is_release && st.isAnyDragging()) {
        st.clearScrollDragging();
        if (st.mouse_dragging) finishDrag();
        return true;
    }

    // Clic derecho ((btn & 3) == 2): copiar lo sombreado al portapapeles
    if ((btn & 3) == 2 && !is_drag && !is_release) {
        return st.copySelectionToClipboard("✔ Seleccion copiada al portapapeles (Clic derecho).");
    }

    if (is_drag) {
        if (st.vscroll_dragging) { TX prev = st.scroll_row; st.scrollVerticalToScreenY(my);   return st.scroll_row != prev; }
        if (st.hscroll_dragging) { TX prev = st.scroll_col; st.scrollHorizontalToScreenX(mx); return st.scroll_col != prev; }
        if (st.ast_vscroll_dragging) { TX prev = st.ast_scroll_y; st.scrollAstVerticalToY(my);   return st.ast_scroll_y != prev; }
        if (st.ast_hscroll_dragging) { TX prev = st.ast_scroll_x; st.scrollAstHorizontalToX(mx); return st.ast_scroll_x != prev; }
        if (!st.mouse_dragging) return false;
        if (st.drag_win == 1) {
            TX gc = 0, gr = 0;
            st.screenToGridCell(mx, my, gc, gr, true);
            if (gc == st.sel_c1 && gr == st.sel_r1) return false;
            st.sel_c1 = gc;
            st.sel_r1 = gr;
            if (st.console_ref_inserting || st.point_mode) {
                std::string ref = st.selectedRangeStr(true);
                if (st.console_ref_inserting) {
                    st.insertOrReplaceRefAtCursor(ref);
                } else {
                    st.setInput(st.point_base + ref);
                }
            }
            return true;
        }
        if (st.drag_win >= 2 && st.drag_win <= 4) {
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

    if ((btn & 3) != 0 || is_release) return false;

    // 1. Clic o doble clic en el Cuadro de Nombres (esquina superior izquierda de la matriz)
    if (st.isNameBoxPos(mx, my)) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - st.last_namebox_click).count();
        st.last_namebox_click = now;
        if (ms > 0 && ms < 400) {
            st.namebox_editing = false;
            st.namebox_buffer.clear();
            st.jumpToHome();
        } else {
            st.namebox_editing = true;
            st.namebox_buffer.clear();
            st.mode = SpreadsheetFocusMode::GridNavigation;
            st.status_banner = "Ir a celda o rango (ej. ZZ500 o A1:C5) y pulsa [Enter].";
        }
        return true;
    }
    st.namebox_editing = false;

    // 2. Clic en mini-scrollbar vertical u horizontal de la Matriz
    if (mx == TuiState::LEFT_X + st.left_w - 1 &&
        my >= TuiState::TOP_Y + 3 && my < TuiState::TOP_Y + 3 + TuiState::VISIBLE_ROWS) {
        st.vscroll_dragging = true;
        st.scrollVerticalToScreenY(my);
        return true;
    }
    if (my == TuiState::TOP_Y + TuiState::TOP_H - 1 &&
        mx >= TuiState::LEFT_X + 2 + TuiState::ROW_HEADER_W + 1 && mx <= TuiState::LEFT_X + st.left_w - 2) {
        st.hscroll_dragging = true;
        st.scrollHorizontalToScreenX(mx);
        return true;
    }

    // 2b. Clic en mini-scrollbars del navegador AST (Ventana 2)
    if (st.ast_graph_h > TuiState::AST_VIEW_H &&
        mx == st.right_x + st.right_w - 1 &&
        my >= TuiState::TOP_Y + 8 && my < TuiState::TOP_Y + 8 + TuiState::AST_VIEW_H) {
        st.ast_vscroll_dragging = true;
        st.scrollAstVerticalToY(my);
        return true;
    }
    if (st.ast_graph_w > st.astViewWidth() &&
        my == TuiState::TOP_Y + TuiState::TOP_H - 1 &&
        mx >= st.right_x + 2 && mx <= st.right_x + st.right_w - 3) {
        st.ast_hscroll_dragging = true;
        st.scrollAstHorizontalToX(mx);
        return true;
    }

    bool is_ctrl_click   = ((btn & 16) != 0);          // Ctrl + Clic/Arrastrar -> Auto-Fill
    bool is_extend_click = ((btn & (4 | 8)) != 0);     // Shift o Alt + Clic -> extender selección

    // Mouse Down (botón izquierdo): ignorar si cae sobre cualquier barra de división
    TX gc = 0, gr = 0;
    if (st.screenToGridCell(mx, my, gc, gr, false)) {
        // Si estamos en Consola editando una fórmula, clic/arrastre en la matriz inserta la celda/rango en `input_cursor`
        if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.point_mode &&
            !is_ctrl_click && TuiState::isFormulaBuffer(st.input_buffer)) {
            st.has_text_sel = false;
            st.has_fill_sel = false;
            st.drag_win = 1;
            st.mouse_dragging = true;
            st.has_cell_sel = true;
            st.sel_c0 = st.sel_c1 = gc;
            st.sel_r0 = st.sel_r1 = gr;
            st.console_ref_inserting = false;
            st.insertOrReplaceRefAtCursor(CellCoord(gc, gr, true).toId());
            st.status_banner = "Referencia insertada en cursor (arrastra para rango).";
            return true;
        }

        // Si estaba en Consola editando un valor (no fórmula), confirmarlo en la celda activa antes de cambiar
        if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.point_mode && !st.input_buffer.empty()) {
            handleCommandSubmit(st, st.input_buffer);
        }

        st.has_text_sel = false;
        st.drag_win = 1;
        st.mouse_dragging = true;

        TX grid_x0 = TuiState::LEFT_X + 2 + TuiState::ROW_HEADER_W;
        TX rel_x_in_cell = (mx - grid_x0) % (TuiState::CELL_WIDTH + 1);
        bool on_active_corner = (gc == st.cursor_col && gr == st.cursor_row && rel_x_in_cell == TuiState::CELL_WIDTH);

        if ((is_ctrl_click || on_active_corner) && !st.point_mode) {
            // Auto-Fill interactivo: origen fijo en celda activa, extremo opuesto en (gc, gr) con rampa pequeña
            st.fill_dragging = true;
            st.has_fill_sel  = true;
            st.has_cell_sel  = false;
            st.sel_c0 = st.cursor_col;
            st.sel_r0 = st.cursor_row;
            st.sel_c1 = gc;
            st.sel_r1 = gr;
            st.mode = SpreadsheetFocusMode::GridNavigation;
            st.input_buffer.clear();
            st.input_cursor = 0;
            st.status_banner = "Auto-Fill: arrastra hacia cualquier direccion y suelta para rellenar.";
            return true;
        }

        st.fill_dragging = false;
        st.has_fill_sel  = false;
        bool had_sel = st.has_cell_sel;
        st.has_cell_sel = true;
        if (is_extend_click) {
            // Shift/Alt/Ctrl + Clic: extiende el rectángulo sombreado manteniendo fija la celda activa inicial
            if (!had_sel) {
                st.sel_c0 = st.cursor_col;
                st.sel_r0 = st.cursor_row;
            }
            st.sel_c1 = gc;
            st.sel_r1 = gr;
        } else {
            if (st.cursor_col != gc || st.cursor_row != gr) st.resetAstScroll();
            st.sel_c0 = st.sel_c1 = gc;
            st.sel_r0 = st.sel_r1 = gr;
            st.cursor_col = gc;
            st.cursor_row = gr;
        }
        if (st.point_mode) {
            st.setInput(st.point_base + st.selectedRangeStr(true));
        } else {
            st.mode = SpreadsheetFocusMode::GridNavigation;
            st.setInput();
        }
        return true;
    }

    for (TX w = 2; w <= 4; ++w) {
        if (st.isSelectableWindowPos(w, mx, my)) {
            st.has_cell_sel = false;
            st.drag_win = w;
            st.mouse_dragging = true;
            st.has_text_sel = true;
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
            if (w == 3 && !st.point_mode) {
                st.mode = SpreadsheetFocusMode::CommandConsole;
                if (st.input_buffer.empty()) st.loadActiveCellIntoBuffer();
            }
            return true;
        }
    }
    return false;
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

    // Si es una secuencia CSI con parámetros numéricos (ej. \033[3~, \033[1;2A, \033[200~ Bracketed Paste)
    std::string csi_params;
    char csi_final = seq[1];
    if (n2 > 0 && seq[0] == '[' && seq[1] >= '0' && seq[1] <= '9') {
        csi_params.push_back(seq[1]);
        csi_final = 0;
        char ch = 0;
        while (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch >= '@' && ch <= '~') {
                csi_final = ch;
                break;
            }
            csi_params.push_back(ch);
        }
    }

    // Bracketed Paste Mode (\033[200~ ... \033[201~):
    // En VS Code el clic derecho intenta pegar el portapapeles en stdin; lo interceptamos aquí
    // para que jamás mueva la celda activa por la matriz, y en Modo 1 lo convertimos en Copiar.
    if (seq[0] == '[' && csi_params == "200" && csi_final == '~') {
        std::string pasted;
        char pch = 0;
        while (read(STDIN_FILENO, &pch, 1) > 0) {
            pasted.push_back(pch);
            if (pasted.size() >= 6 && pasted.ends_with("\033[201~")) {
                pasted.resize(pasted.size() - 6);
                break;
            }
        }
        bool has_external_sel = st.has_cell_sel || (st.has_text_sel && st.text_win != 3);
        if (!st.modal_open && (has_external_sel || st.mode != SpreadsheetFocusMode::CommandConsole)) {
            st.copySelectionToClipboard();
        } else if (st.mode == SpreadsheetFocusMode::CommandConsole && !st.modal_open) {
            for (char c : pasted) {
                if (c == '\n' || c == '\r') break;
                if (texcel_detail::isChar<std::isprint>(c)) {
                    st.insertInputChar(c);
                }
            }
        }
        return true;
    }

    if (st.modal_open) {
        if (n1 == 0) {
            if (st.modal_new_mode) { st.modal_new_mode = false; st.modal_new_name.clear(); }
            else st.modal_open = false;
        } else if (seq[0] == '[' && !st.modal_new_mode) {
            TX max_idx = 2 + static_cast<TX>(st.config.sheet_files.size());
            if (csi_final == 'A' && st.modal_idx > 0) --st.modal_idx;
            else if (csi_final == 'B' && st.modal_idx < max_idx) ++st.modal_idx;
            else if (csi_params == "3" && csi_final == '~') st.deleteSelectedModalSheet();
        }
        return true;
    }

    if (n1 == 0) {
        st.namebox_editing = false;
        st.namebox_buffer.clear();
        st.has_cell_sel = false;
        st.has_text_sel = false;
        st.mouse_dragging = false;
        st.os << "\033[?2004h\033[?1003l\033[?1000h\033[?1002h\033[?1006h" << std::flush;
        st.exitPointMode(true);
        st.setInput();
        st.mode = SpreadsheetFocusMode::GridNavigation;
        st.status_banner = "Modo Navegacion activo (usa flechas para moverte).";
        return true;
    }

    if (seq[0] == '[') {
        if (st.mode == SpreadsheetFocusMode::GridNavigation) {
            auto stepGridCoord = [](TX& col, TX& row, char dir) {
                if (dir == 'A' && row > 0) --row;
                else if (dir == 'B' && row < CellCoord::MAX_ROWS - 1) ++row;
                else if (dir == 'C' && col < CellCoord::MAX_COLS - 1) ++col;
                else if (dir == 'D' && col > 0) --col;
                else return false;
                return true;
            };
            bool is_arrow = (csi_final == 'A' || csi_final == 'B' || csi_final == 'C' || csi_final == 'D');
            bool is_shift_arrow = (csi_params == "1;2" && is_arrow);

            if (is_shift_arrow) {
                if (!st.has_cell_sel) {
                    st.sel_c0 = st.sel_c1 = st.cursor_col;
                    st.sel_r0 = st.sel_r1 = st.cursor_row;
                    st.has_cell_sel = true;
                }
                st.has_text_sel = false;
                stepGridCoord(st.sel_c1, st.sel_r1, csi_final);
                st.status_banner = "Rango " + st.selectedRangeStr() + " sombreado ([Ctrl+C] o [Y] para copiar).";
            } else {
                if (csi_params.empty() && is_arrow) {
                    st.has_cell_sel = false;
                    if (stepGridCoord(st.cursor_col, st.cursor_row, csi_final)) st.resetAstScroll();
                } else if (csi_params == "3" && csi_final == '~' && !st.point_mode) {
                    st.clearSelectedCells();
                }
            }
            st.ensureCursorVisible();
            if (st.point_mode) st.updatePointedCell();
        } else {
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
        if (idx == 0) {
            st.config.autosave = !st.config.autosave;
            st.config.saveConfig();
            st.engine.addLog(std::string("⚙ Autoguardado: ") + (st.config.autosave ? "ON" : "OFF"));
            st.status_banner = std::string("Autoguardado ") + (st.config.autosave ? "activado." : "desactivado.");
            if (st.config.autosave) st.saveSheet(st.config.current_sheet, true);
        } else if (idx == 1) {
            st.saveSheet(st.config.current_sheet);
            st.refreshSheetList();
            st.modal_open = false;
        } else if (idx == 2) {
            st.modal_new_mode = true;
            st.modal_new_name.clear();
        } else if (idx >= 3) {
            TX fidx = idx - 3;
            if (fidx < static_cast<TX>(st.config.sheet_files.size())) {
                st.loadSheet(st.config.sheet_files[static_cast<size_t>(fidx)].getValue());
                st.modal_open = false;
            }
        }
    };

    if (ch == '\n' || ch == '\r' || ch == ' ') executeModalItem(st.modal_idx);
    else if (ch == 'a' || ch == 'A') executeModalItem(0);
    else if (ch == 's' || ch == 'S') executeModalItem(1);
    else if (ch == 'n' || ch == 'N') executeModalItem(2);
    else if (ch == 127 || ch == '\b') st.deleteSelectedModalSheet();
    else if (ch == 'q' || ch == 'Q') st.modal_open = false;
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
            st.target_col = st.cursor_col;
            st.target_row = st.cursor_row;
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

    std::string cid = CellCoord(st.cursor_col, st.cursor_row, true).toId();
    if (ch == 'q' || ch == 'Q') return false;

    if (ch == 'y' || ch == 'Y') {
        st.copySelectionToClipboard("✔ Copiado al portapapeles (Ctrl+C o Y).");
    } else if (ch == 'o' || ch == 'O') {
        st.ast_horizontal = !st.ast_horizontal;
        st.resetAstScroll();
        st.status_banner = std::string("Orientacion AST: ") + (st.ast_horizontal ? "Horizontal [H]." : "Vertical [V].");
    } else if ((ch == 'f' || ch == 'F' || ch == '+') && st.has_cell_sel) {
        st.applySelectionAutoFill();
    } else if (ch == '=') {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.setInput("= ");
        st.status_banner = "Editando formula en " + cid + " (pulsa [Tab] para Point Mode).";
    } else if (ch == '\n' || ch == '\r') {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.loadActiveCellIntoBuffer();
        st.status_banner = "Editando celda " + cid + " en vivo...";
    } else if (ch == ':') {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = false;
        st.setInput();
        st.status_banner = "Modo comando directo activado.";
    } else if (ch == 127 || ch == '\b') {
        st.clearSelectedCells();
    } else if (texcel_detail::isChar<std::isdigit>(ch)) {
        st.mode = SpreadsheetFocusMode::CommandConsole;
        st.return_to_grid_on_submit = true;
        st.setInput(std::string(1, ch));
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
            if (target.valid) {
                st.cursor_col = target.col;
                st.cursor_row = target.row;
            }
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
