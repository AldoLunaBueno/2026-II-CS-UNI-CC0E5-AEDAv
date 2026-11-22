/**
 * @file tui_input.h
 * @brief Despachadores de entrada (teclado, ratón SGR, modal, Point Mode y comandos) para `SpreadsheetTUI`.
 */

#ifndef __TEXCEL_HELPERS_TUI_INPUT_H__
#define __TEXCEL_HELPERS_TUI_INPUT_H__

inline std::string SpreadsheetTUI::formatCellForInput(const std::string& cid) {
    SpreadsheetCell cur = m_engine.getCell(cid);
    if (cur.raw_expression.empty()) return "";
    if (cur.is_formula && cur.raw_expression[0] != '=') return "= " + cur.raw_expression;
    return cur.raw_expression;
}

inline void SpreadsheetTUI::loadActiveCellIntoBuffer() {
    std::string cid = CellCoord(m_cursor_col, m_cursor_row, true).toId();
    m_input_buffer = formatCellForInput(cid);
    m_input_cursor = m_input_buffer.size();
}

inline void SpreadsheetTUI::deleteSelectedModalSheet() {
    if (m_modal_idx < 3) return;
    TX fidx = m_modal_idx - 3;
    if (fidx < static_cast<TX>(m_sheet_files.size())) {
        std::string target = m_sheet_files[static_cast<size_t>(fidx)].getValue();
        std::error_code ec;
        std::filesystem::remove(sheetPath(target), ec);
        m_engine.addLog("🗑 Eliminada hoja: " + target + TexcelConfig::SHEET_EXT);
        refreshSheetList();
    }
}

inline bool SpreadsheetTUI::handleCommandSubmit(const std::string& raw_cmd) {
    std::string cmd = texcel_detail::trim(raw_cmd);
    if (cmd.empty()) return true;

    m_cmd_history.push_back(cmd);
    m_history_idx = static_cast<TX>(m_cmd_history.size());

    std::string upper = texcel_detail::toUpper(cmd);
    if (upper == "QUIT" || upper == "EXIT" || upper == "Q") return false;

    if (upper == "CLEAR") {
        m_engine.clear();
        maybeAutoSave();
        m_status_banner = "Hoja de calculo limpiada por completo.";
        return true;
    }
    if (upper == "DEMO") {
        m_engine.clear();
        m_engine.loadProfessorDemo();
        maybeAutoSave();
        m_cursor_col = 3;
        m_cursor_row = 3;
        ensureCursorVisible();
        m_status_banner = "Cargado ejemplo de clase: A1=5, B1=A1+4, D4=A1+B1";
        return true;
    }
    if (upper == "SAVE") { saveSheet(m_current_sheet); return true; }
    if (upper == "LOAD") { loadSheet(m_current_sheet); return true; }

    std::string current_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
    std::string assigned_id;
    bool ok = m_engine.executeCommand(cmd, current_id, &assigned_id);
    if (ok) maybeAutoSave();

    CellCoord dest = CellCoord::parse(assigned_id);
    if (dest.valid) {
        m_cursor_col = dest.col;
        m_cursor_row = dest.row;
        ensureCursorVisible();
    }
    m_status_banner = ok
        ? ("Actualizada " + assigned_id + " = " + m_engine.getCell(assigned_id).displayValue())
        : ("Error al evaluar " + assigned_id + ": verifica sintaxis o ciclos.");
    return true;
}

inline bool SpreadsheetTUI::handleSgrMouse(const std::string& payload, char mc) {
    TX btn = 0, mx_1 = 1, my_1 = 1;
    char s1 = 0, s2 = 0;
    std::istringstream mss(payload);
    if (!(mss >> btn >> s1 >> mx_1 >> s2 >> my_1)) return false;

    TX mx = mx_1 - 1;
    TX my = my_1 - 1;
    bool is_release = (mc == 'm');
    bool is_drag    = ((btn & 32) != 0);
    bool is_wheel   = ((btn & 64) != 0);

    if (m_modal_open) return false;

    if (is_wheel) {
        constexpr TX V_STEP = 3, H_STEP = 2;
        bool on_col_header = (mx >= LEFT_X + 2 && mx < LEFT_X + m_left_w - 2 &&
                              (my == TOP_Y + 1 || my == TOP_Y + 2));
        bool is_horizontal = on_col_header || ((btn & 2) != 0) || ((btn & 4) != 0) ||
                             ((btn & 8) != 0) || ((btn & 16) != 0);
        bool forward = ((btn & 1) != 0);
        if (is_horizontal) m_scroll_col += forward ? H_STEP : -H_STEP;
        else m_scroll_row += forward ? V_STEP : -V_STEP;
        clampScroll();
        return true;
    }

    if (is_release && m_mouse_dragging) {
        m_mouse_dragging = false;
        if (m_drag_win == 1 && m_has_cell_sel) {
            TX cmin, rmin, cmax, rmax;
            texcel_tui::normalizeCellRange(m_sel_c0, m_sel_r0, m_sel_c1, m_sel_r1, cmin, rmin, cmax, rmax);
            if (cmin != cmax || rmin != rmax) {
                std::string r_str = CellCoord(cmin, rmin, true).toId() + ":" + CellCoord(cmax, rmax, true).toId();
                m_status_banner = "Rango " + r_str + " seleccionado ([Ctrl+C] o [Y] para copiar).";
            } else {
                m_has_cell_sel = false;
            }
        } else if (m_drag_win >= 2 && m_drag_win <= 4 && m_has_text_sel) {
            m_status_banner = "Texto seleccionado ([Ctrl+C] para copiar).";
        }
        m_drag_win = 0;
        return true;
    }

    if ((btn & 3) != 0 || is_release) return false;

    if (!is_drag) {
        m_has_cell_sel = false;
        m_has_text_sel = false;
        m_drag_win = 0;

        TX gc = 0, gr = 0;
        if (screenToGridCell(mx, my, gc, gr, false)) {
            m_drag_win = 1;
            m_mouse_dragging = true;
            m_has_cell_sel = true;
            m_sel_c0 = m_sel_c1 = gc;
            m_sel_r0 = m_sel_r1 = gr;
            m_cursor_col = gc;
            m_cursor_row = gr;
            if (m_point_mode) {
                updatePointedCell();
            } else {
                m_mode = SpreadsheetFocusMode::GridNavigation;
                m_input_buffer.clear();
                m_input_cursor = 0;
            }
            return true;
        }

        for (TX w = 2; w <= 4; ++w) {
            TX wx0, wy0, wx1, wy1;
            if (windowInnerBounds(w, wx0, wy0, wx1, wy1) &&
                mx >= wx0 && mx <= wx1 && my >= wy0 && my <= wy1) {
                m_drag_win = w;
                m_mouse_dragging = true;
                m_has_text_sel = true;
                m_text_win = w;
                m_tx0 = m_tx1 = mx;
                m_ty0 = m_ty1 = my;
                if (w == 3 && !m_point_mode) {
                    m_mode = SpreadsheetFocusMode::CommandConsole;
                    if (m_input_buffer.empty()) loadActiveCellIntoBuffer();
                }
                return true;
            }
        }
        return true;
    }

    // Drag
    if (!m_mouse_dragging) {
        TX gc = 0, gr = 0;
        if (screenToGridCell(mx, my, gc, gr, false)) {
            m_drag_win = 1;
            m_mouse_dragging = true;
            m_has_cell_sel = true;
            m_has_text_sel = false;
            m_sel_c0 = m_sel_c1 = gc;
            m_sel_r0 = m_sel_r1 = gr;
        }
    }
    if (m_mouse_dragging && m_drag_win == 1) {
        TX gc = 0, gr = 0;
        screenToGridCell(mx, my, gc, gr, true);
        if (gc == m_sel_c1 && gr == m_sel_r1) return false;
        m_sel_c1 = gc;
        m_sel_r1 = gr;
        m_cursor_col = gc;
        m_cursor_row = gr;
        if (m_point_mode) {
            TX cmin, rmin, cmax, rmax;
            texcel_tui::normalizeCellRange(m_sel_c0, m_sel_r0, m_sel_c1, m_sel_r1, cmin, rmin, cmax, rmax);
            std::string id0 = CellCoord(cmin, rmin, true).toId();
            std::string id1 = CellCoord(cmax, rmax, true).toId();
            m_input_buffer = m_point_base + ((id0 == id1) ? id0 : (id0 + ":" + id1));
            m_input_cursor = m_input_buffer.size();
        }
        return true;
    }
    if (m_mouse_dragging && m_drag_win >= 2 && m_drag_win <= 4) {
        TX wx0, wy0, wx1, wy1;
        if (windowInnerBounds(m_drag_win, wx0, wy0, wx1, wy1)) {
            TX nx = std::clamp(mx, wx0, wx1);
            TX ny = std::clamp(my, wy0, wy1);
            if (nx == m_tx1 && ny == m_ty1) return false;
            m_tx1 = nx;
            m_ty1 = ny;
            return true;
        }
    }
    return false;
}

inline bool SpreadsheetTUI::handleEscapeSequence(termios& raw_term) {
    char seq[3] = {0, 0, 0};
    raw_term.c_cc[VMIN] = 0;
    raw_term.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);
    TX n1 = static_cast<TX>(read(STDIN_FILENO, &seq[0], 1));
    TX n2 = (n1 > 0) ? static_cast<TX>(read(STDIN_FILENO, &seq[1], 1)) : 0;
    if (n2 > 0 && seq[1] >= '0' && seq[1] <= '9') {
        (void)read(STDIN_FILENO, &seq[2], 1);
    }
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
        return handleSgrMouse(payload, mc);
    }

    if (m_modal_open) {
        if (n1 == 0) {
            if (m_modal_new_mode) { m_modal_new_mode = false; m_modal_new_name.clear(); }
            else m_modal_open = false;
        } else if (seq[0] == '[' && !m_modal_new_mode) {
            TX max_idx = 2 + static_cast<TX>(m_sheet_files.size());
            if (seq[1] == 'A' && m_modal_idx > 0) --m_modal_idx;
            else if (seq[1] == 'B' && m_modal_idx < max_idx) ++m_modal_idx;
            else if (seq[1] == '3' && seq[2] == '~') deleteSelectedModalSheet();
        }
        return true;
    }

    if (n1 == 0) {
        m_has_cell_sel = false;
        m_has_text_sel = false;
        m_mouse_dragging = false;
        m_os << "\033[?1000h\033[?1002h\033[?1006h" << std::flush;
        if (m_point_mode) {
            m_point_mode = false;
            m_cursor_col = m_target_col;
            m_cursor_row = m_target_row;
            ensureCursorVisible();
        }
        m_input_buffer.clear();
        m_input_cursor = 0;
        m_mode = SpreadsheetFocusMode::GridNavigation;
        m_status_banner = "Modo Navegacion activo (usa flechas para moverte).";
        return true;
    }

    if (seq[0] == '[') {
        if (m_mode == SpreadsheetFocusMode::GridNavigation) {
            if (seq[1] == 'A' || seq[1] == 'B' || seq[1] == 'C' || seq[1] == 'D') m_has_cell_sel = false;
            if (seq[1] == 'A' && m_cursor_row > 0) --m_cursor_row;
            else if (seq[1] == 'B' && m_cursor_row < 98) ++m_cursor_row;
            else if (seq[1] == 'C' && m_cursor_col < 25) ++m_cursor_col;
            else if (seq[1] == 'D' && m_cursor_col > 0) --m_cursor_col;
            else if (seq[1] == '3' && seq[2] == '~' && !m_point_mode) {
                std::string cid = CellCoord(m_cursor_col, m_cursor_row, true).toId();
                m_engine.setCell(cid, "");
                maybeAutoSave();
            }
            ensureCursorVisible();
            if (m_point_mode) updatePointedCell();
        } else {
            if (seq[1] == 'D') { if (m_input_cursor > 0) --m_input_cursor; }
            else if (seq[1] == 'C') { if (m_input_cursor < m_input_buffer.size()) ++m_input_cursor; }
            else if (seq[1] == 'H' || (seq[1] == '1' && seq[2] == '~')) m_input_cursor = 0;
            else if (seq[1] == 'F' || (seq[1] == '4' && seq[2] == '~')) m_input_cursor = m_input_buffer.size();
            else if (seq[1] == '3' && seq[2] == '~') {
                if (m_input_cursor < m_input_buffer.size()) m_input_buffer.erase(m_input_cursor, 1);
            } else if (seq[1] == 'A' && m_cmd_history.size() > 0) {
                if (m_history_idx > 0) --m_history_idx;
                m_input_buffer = m_cmd_history[static_cast<size_t>(m_history_idx)].getValue();
                m_input_cursor = m_input_buffer.size();
            } else if (seq[1] == 'B') {
                if (m_history_idx + 1 < static_cast<TX>(m_cmd_history.size())) {
                    ++m_history_idx;
                    m_input_buffer = m_cmd_history[static_cast<size_t>(m_history_idx)].getValue();
                    m_input_cursor = m_input_buffer.size();
                } else {
                    m_history_idx = static_cast<TX>(m_cmd_history.size());
                    m_input_buffer.clear();
                    m_input_cursor = 0;
                }
            }
        }
    }
    return true;
}

inline void SpreadsheetTUI::handleModalKey(char ch) {
    if (m_modal_new_mode) {
        if (ch == '\n' || ch == '\r') {
            if (!m_modal_new_name.empty()) {
                m_engine.clear();
                m_current_sheet = m_modal_new_name;
                saveSheet(m_current_sheet);
                refreshSheetList();
                m_modal_new_mode = false;
                m_modal_new_name.clear();
                m_modal_open = false;
            }
        } else if (ch == 127 || ch == '\b') {
            if (!m_modal_new_name.empty()) m_modal_new_name.pop_back();
        } else if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
            if (m_modal_new_name.size() < 24) m_modal_new_name.push_back(ch);
        }
        return;
    }

    auto executeModalItem = [&](TX idx) {
        if (idx == 0) {
            m_autosave = !m_autosave;
            saveConfig();
            m_engine.addLog(std::string("⚙ Autoguardado: ") + (m_autosave ? "ON" : "OFF"));
            m_status_banner = std::string("Autoguardado ") + (m_autosave ? "activado." : "desactivado.");
            if (m_autosave) saveSheet(m_current_sheet, true);
        } else if (idx == 1) {
            saveSheet(m_current_sheet);
            refreshSheetList();
            m_modal_open = false;
        } else if (idx == 2) {
            m_modal_new_mode = true;
            m_modal_new_name.clear();
        } else if (idx >= 3) {
            TX fidx = idx - 3;
            if (fidx < static_cast<TX>(m_sheet_files.size())) {
                loadSheet(m_sheet_files[static_cast<size_t>(fidx)].getValue());
                m_modal_open = false;
            }
        }
    };

    if (ch == '\n' || ch == '\r' || ch == ' ') executeModalItem(m_modal_idx);
    else if (ch == 'a' || ch == 'A') executeModalItem(0);
    else if (ch == 's' || ch == 'S') executeModalItem(1);
    else if (ch == 'n' || ch == 'N') executeModalItem(2);
    else if (ch == 127 || ch == '\b') deleteSelectedModalSheet();
    else if (ch == 'q' || ch == 'Q') m_modal_open = false;
}

inline void SpreadsheetTUI::handleTabKey() {
    if (m_mode == SpreadsheetFocusMode::GridNavigation) {
        if (m_point_mode) {
            m_point_mode = false;
            m_cursor_col = m_target_col;
            m_cursor_row = m_target_row;
        }
        m_mode = SpreadsheetFocusMode::CommandConsole;
        if (m_input_buffer.empty()) loadActiveCellIntoBuffer();
        m_input_cursor = m_input_buffer.size();
        m_status_banner = "Consola activa. Escribe = B1 + 4, 5, o [Ctrl+U] para limpiar";
    } else {
        if (isFormulaBuffer(m_input_buffer) && canAcceptPointRef(m_input_buffer)) {
            m_point_mode = true;
            m_target_col = m_cursor_col;
            m_target_row = m_cursor_row;
            if (!m_input_buffer.empty() && m_input_buffer.back() != ' ' &&
                m_input_buffer.back() != '(' && m_input_buffer.back() != ':') {
                m_input_buffer.push_back(' ');
            }
            m_point_base = m_input_buffer;
            m_status_banner = "Point Mode activo: mueve con flechas para apuntar una celda.";
        } else {
            m_point_mode = false;
        }
        m_mode = SpreadsheetFocusMode::GridNavigation;
        if (!m_point_mode) {
            m_input_buffer.clear();
            m_input_cursor = 0;
            m_status_banner = "Foco en Hoja de Calculo. Muevete con flechas o pulsa Enter.";
        }
    }
}

inline bool SpreadsheetTUI::handlePointModeKey(char ch) {
    if (ch == '\n' || ch == '\r') {
        m_point_mode = false;
        m_cursor_col = m_target_col;
        m_cursor_row = m_target_row;
        if (!handleCommandSubmit(m_input_buffer)) return false;
        m_input_buffer.clear();
        m_input_cursor = 0;
    } else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^' ||
               ch == '(' || ch == ')' || ch == ':' || ch == ',') {
        if (ch == ':' || ch == ')' || ch == ',') m_input_buffer.push_back(ch);
        else { m_input_buffer += " "; m_input_buffer.push_back(ch); m_input_buffer += " "; }
        m_point_base = m_input_buffer;
        if (ch == ')') {
            m_point_mode = false;
            m_cursor_col = m_target_col;
            m_cursor_row = m_target_row;
            m_mode = SpreadsheetFocusMode::CommandConsole;
            m_input_cursor = m_input_buffer.size();
        }
    } else if (ch == 127 || ch == '\b') {
        m_input_buffer = m_point_base;
    } else if (ch == 'w' || ch == 'k') { if (m_cursor_row > 0) { --m_cursor_row; updatePointedCell(); } }
    else if (ch == 's' || ch == 'j') { if (m_cursor_row < 98) { ++m_cursor_row; updatePointedCell(); } }
    else if (ch == 'a' || ch == 'h') { if (m_cursor_col > 0) { --m_cursor_col; updatePointedCell(); } }
    else if (ch == 'd' || ch == 'l') { if (m_cursor_col < 25) { ++m_cursor_col; updatePointedCell(); } }
    return true;
}

inline bool SpreadsheetTUI::handleGridKey(char ch) {
    if (m_point_mode) return handlePointModeKey(ch);

    std::string cid = CellCoord(m_cursor_col, m_cursor_row, true).toId();
    if (ch == 'q' || ch == 'Q') return false;

    if (ch == 'y' || ch == 'Y') {
        std::string txt = extractCurrentSelectionText();
        if (!txt.empty()) {
            copyToClipboardOsc52(txt);
            m_status_banner = "✔ Copiado al portapapeles (Ctrl+C o Y).";
        }
    } else if (ch == '=') {
        m_mode = SpreadsheetFocusMode::CommandConsole;
        m_input_buffer = "= ";
        m_input_cursor = m_input_buffer.size();
        m_status_banner = "Editando formula en " + cid + " (pulsa [Tab] para Point Mode).";
    } else if (ch == '\n' || ch == '\r') {
        m_mode = SpreadsheetFocusMode::CommandConsole;
        loadActiveCellIntoBuffer();
        m_status_banner = "Editando celda " + cid + " en vivo...";
    } else if (ch == ':') {
        m_mode = SpreadsheetFocusMode::CommandConsole;
        m_input_buffer.clear();
        m_input_cursor = 0;
        m_status_banner = "Modo comando directo activado.";
    } else if (ch == 127 || ch == '\b') {
        m_engine.setCell(cid, "");
        maybeAutoSave();
        m_status_banner = "Celda " + cid + " borrada.";
    } else if (ch == 'w' || ch == 'k') { m_has_cell_sel = false; if (m_cursor_row > 0) --m_cursor_row; ensureCursorVisible(); }
    else if (ch == 's' || ch == 'j')   { m_has_cell_sel = false; if (m_cursor_row < 98) ++m_cursor_row; ensureCursorVisible(); }
    else if (ch == 'a' || ch == 'h')   { m_has_cell_sel = false; if (m_cursor_col > 0) --m_cursor_col; ensureCursorVisible(); }
    else if (ch == 'd' || ch == 'l')   { m_has_cell_sel = false; if (m_cursor_col < 25) ++m_cursor_col; ensureCursorVisible(); }
    else if (std::isdigit(static_cast<unsigned char>(ch))) {
        m_mode = SpreadsheetFocusMode::CommandConsole;
        m_input_buffer = std::string(1, ch);
        m_input_cursor = 1;
    }
    return true;
}

inline bool SpreadsheetTUI::handleConsoleKey(char ch) {
    if (ch == 21) { // Ctrl+U
        m_input_buffer.clear();
        m_input_cursor = 0;
    } else if (ch == '\n' || ch == '\r') {
        if (!handleCommandSubmit(m_input_buffer)) return false;
        m_input_buffer.clear();
        m_input_cursor = 0;
    } else if (ch == 127 || ch == '\b') {
        if (m_input_cursor > 0 && m_input_cursor <= m_input_buffer.size()) {
            m_input_buffer.erase(m_input_cursor - 1, 1);
            --m_input_cursor;
        }
    } else if (std::isprint(static_cast<unsigned char>(ch))) {
        m_input_cursor = std::min(m_input_cursor, m_input_buffer.size());
        m_input_buffer.insert(m_input_cursor, 1, ch);
        ++m_input_cursor;
        size_t eq = m_input_buffer.find('=');
        if (eq != std::string::npos) {
            CellCoord target = CellCoord::parse(m_input_buffer.substr(0, eq));
            if (target.valid) {
                m_cursor_col = target.col;
                m_cursor_row = target.row;
            }
        }
    }
    return true;
}

#endif // __TEXCEL_HELPERS_TUI_INPUT_H__
