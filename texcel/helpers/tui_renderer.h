/**
 * @file tui_renderer.h
 * @brief Métodos de renderizado 2D de las 4 ventanas y del modal flotante para `SpreadsheetTUI`.
 */

#ifndef __TEXCEL_HELPERS_TUI_RENDERER_H__
#define __TEXCEL_HELPERS_TUI_RENDERER_H__

inline void SpreadsheetTUI::drawWindowLineWithSelection(TX win_id, TX start_x, TX abs_y,
                                                        const std::string& styled_line, size_t inner_w) {
    std::string plain = texcel_tui::stripAnsi(styled_line);
    if (abs_y >= 0 && abs_y < TOTAL_HEIGHT && win_id >= 1 && win_id <= 4) {
        m_win_lines[win_id][abs_y] = plain;
    }

    if (!m_has_text_sel || m_text_win != win_id) {
        gotoxy(ros(), start_x, abs_y);
        ros() << texcel_tui::fitWidth(styled_line, inner_w);
        return;
    }

    TX sx0, sy0, sx1, sy1;
    texcel_tui::normalizeTextRange(m_tx0, m_ty0, m_tx1, m_ty1, sx0, sy0, sx1, sy1);
    if (abs_y < sy0 || abs_y > sy1) {
        gotoxy(ros(), start_x, abs_y);
        ros() << texcel_tui::fitWidth(styled_line, inner_w);
        return;
    }

    std::string padded = plain;
    if (padded.size() < inner_w) padded += std::string(inner_w - padded.size(), ' ');
    TX c_start = (abs_y == sy0) ? std::max<TX>(0, sx0 - start_x) : 0;
    TX c_end   = (abs_y == sy1) ? std::min<TX>(static_cast<TX>(inner_w) - 1, sx1 - start_x) : static_cast<TX>(inner_w) - 1;

    gotoxy(ros(), start_x, abs_y);
    if (c_start > c_end || c_start >= static_cast<TX>(inner_w)) {
        ros() << texcel_tui::fitWidth(styled_line, inner_w);
        return;
    }

    std::string before = texcel_tui::utf8Slice(padded, 0, static_cast<size_t>(c_start));
    std::string mid    = texcel_tui::utf8Slice(padded, static_cast<size_t>(c_start), static_cast<size_t>(c_end) + 1);
    std::string after  = texcel_tui::utf8Slice(padded, static_cast<size_t>(c_end) + 1, inner_w);
    ros() << before << styled(mid).pure_black().bg_bright_white() << after;
}

inline void SpreadsheetTUI::drawWindowFrame(TX x, TX y, TX w, TX h,
                                            const std::string& title,
                                            bool active_focus,
                                            bool accent_secondary,
                                            bool clear_interior) {
    auto styleBorder = [&](const std::string& glyph) {
        if (active_focus) return styled(glyph).bright_cyan().bold().str();
        if (accent_secondary) return styled(glyph).cyan().str();
        return styled(glyph).gray().str();
    };

    std::string badge = active_focus ? " ● ACTIVA " : " ";
    size_t inner_w = (w >= 2) ? static_cast<size_t>(w - 2) : 0;
    std::string full_title = " " + title + badge;
    if (inner_w > 2 && visible_length(full_title) > inner_w - 2) {
        full_title = texcel_tui::fitWidth(full_title, inner_w - 2);
    }
    size_t title_vlen = visible_length(full_title);

    // Borde superior
    gotoxy(ros(), x, y);
    ros() << styleBorder("╭─");
    std::string styled_title = active_focus
        ? styled(full_title).pure_black().bg_bright_cyan().bold().str()
        : (accent_secondary
            ? styled(full_title).bright_white().bold().str()
            : styled(full_title).gray().str());
    ros() << styled_title;

    size_t used = 1 + title_vlen;
    for (size_t i = used; i < inner_w; ++i) ros() << styleBorder("─");
    ros() << styleBorder("╮");

    // Laterales (sin borrado interior salvo en modales flotantes para evitar parpadeo)
    if (clear_interior) {
        std::string blank_inner(inner_w, ' ');
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(ros(), x, row);
            ros() << styleBorder("│") << blank_inner << styleBorder("│");
        }
    } else {
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(ros(), x, row);
            ros() << styleBorder("│");
            gotoxy(ros(), x + w - 1, row);
            ros() << styleBorder("│");
        }
    }

    // Borde inferior
    gotoxy(ros(), x, y + h - 1);
    ros() << styleBorder("╰");
    for (size_t i = 0; i < inner_w; ++i) ros() << styleBorder("─");
    ros() << styleBorder("╯");
}

inline void SpreadsheetTUI::renderHeaderBar(const EvalPreview& preview) {
    gotoxy(ros(), 0, 0);
    std::string mode_badge;
    if (m_point_mode) {
        mode_badge = styled(texcel_tui::fitWidth(" POINT MODE: APUNTAR CELDA ", 27)).pure_black().bg_bright_yellow().bold().str();
    } else if (m_mode == SpreadsheetFocusMode::GridNavigation) {
        mode_badge = styled(texcel_tui::fitWidth(" MODO 1: NAVEGACION MATRIZ ", 27)).pure_black().bg_bright_cyan().bold().str();
    } else {
        mode_badge = styled(texcel_tui::fitWidth(" MODO 2: CONSOLA COMANDOS ", 27)).pure_black().bg_bright_green().bold().str();
    }

    TX disp_col = m_point_mode ? m_target_col : m_cursor_col;
    TX disp_row = m_point_mode ? m_target_row : m_cursor_row;
    std::string focus_id = CellCoord(disp_col, disp_row, true).toId();
    if (preview.active && !preview.target_cell.empty()) {
        focus_id = preview.target_cell;
    }

    std::string auto_badge = m_autosave
        ? styled(texcel_tui::fitWidth(" Auto:ON ", 10)).pure_black().bg_bright_green().bold().str()
        : styled(texcel_tui::fitWidth(" Auto:OFF ", 10)).gray().str();

    std::ostringstream oss;
    oss << styled(" ◆ Texcel ").bright_white().bg_blue().bold()
        << " " << mode_badge
        << styled(" │ ").gray() << styled(m_current_sheet + TexcelConfig::SHEET_EXT).bright_yellow().bold()
        << " " << auto_badge
        << styled(" │ Celda:").gray() << styled(texcel_tui::fitWidth(focus_id, 3)).bright_cyan().bold()
        << styled(" │ [Ctrl+S] Guardar [Ctrl+O] Hojas [Tab] Modo [q] Salir").gray();

    ros() << texcel_tui::fitWidth(oss.str(), static_cast<size_t>(m_total_width));
}

inline void SpreadsheetTUI::renderGridWindow(const EvalPreview& preview) {
    bool is_focused = (m_mode == SpreadsheetFocusMode::GridNavigation);
    drawWindowFrame(LEFT_X, TOP_Y, m_left_w, TOP_H, "Hoja de Calculo", is_focused);

    std::string active_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
    std::string preview_target_id = active_id;
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        preview_target_id = preview.target_cell;
        if (!m_point_mode) active_id = preview_target_id;
    }

    auto inputs = preview.active ? preview.referenced_cells : m_engine.getCellInputs(active_id);
    auto dependents = m_engine.getCellDependents(active_id);
    const size_t inner_w = static_cast<size_t>(m_left_w - 4);

    // Encabezado de columnas (A, B, C...)
    {
        std::ostringstream hdr_ss;
        hdr_ss << styled("Fila ").dim();
        for (TX c = 0; c < m_visible_cols; ++c) {
            TX col_idx = m_scroll_col + c;
            std::string hdr = texcel_tui::alignCell(CellCoord::colToLabel(col_idx), CELL_WIDTH, false);
            hdr_ss << styled("│").gray();
            hdr_ss << (col_idx == m_cursor_col ? styled(hdr).bright_cyan().bold() : styled(hdr).white().bold());
        }
        gotoxy(ros(), LEFT_X + 2, TOP_Y + 1);
        ros() << texcel_tui::fitWidth(hdr_ss.str(), inner_w);
    }

    // Separador horizontal adaptado a m_visible_cols
    {
        std::string sep_line = "─────";
        for (TX c = 0; c < m_visible_cols; ++c) {
            sep_line += (c == 0) ? "┼" : "─";
            for (TX k = 0; k < CELL_WIDTH; ++k) sep_line += "─";
        }
        gotoxy(ros(), LEFT_X + 2, TOP_Y + 2);
        ros() << texcel_tui::fitWidth(styled(sep_line).gray().str(), inner_w);
    }

    TX sel_cmin = 0, sel_rmin = 0, sel_cmax = 0, sel_rmax = 0;
    if (m_has_cell_sel) {
        texcel_tui::normalizeCellRange(m_sel_c0, m_sel_r0, m_sel_c1, m_sel_r1,
                                       sel_cmin, sel_rmin, sel_cmax, sel_rmax);
    }

    // Filas de la matriz
    for (TX r = 0; r < VISIBLE_ROWS; ++r) {
        TX row_idx = m_scroll_row + r;
        std::ostringstream row_ss;

        std::ostringstream row_lbl;
        row_lbl << std::setw(3) << (row_idx + 1) << "  ";
        row_ss << (row_idx == m_cursor_row ? styled(row_lbl.str()).bright_cyan().bold() : styled(row_lbl.str()).gray());

        for (TX c = 0; c < m_visible_cols; ++c) {
            TX col_idx = m_scroll_col + c;
            std::string cid = CellCoord(col_idx, row_idx, true).toId();
            SpreadsheetCell cell = m_engine.getCell(cid);

            bool is_cursor = (cid == active_id);
            bool is_target_cell = m_point_mode && (cid == preview_target_id);
            bool is_in_rect_sel = m_has_cell_sel &&
                                  (col_idx >= sel_cmin && col_idx <= sel_cmax && row_idx >= sel_rmin && row_idx <= sel_rmax);
            bool is_input_ref = texcel_detail::containsStr(inputs, cid);
            bool is_dependent = texcel_detail::containsStr(dependents, cid);

            std::string val_text = cell.displayValue();
            bool is_text_disp = cell.is_text;
            if ((is_target_cell || (!m_point_mode && is_cursor)) && preview.active && preview.valid) {
                val_text = preview.formatted_value;
                is_text_disp = preview.is_text;
            }
            std::string formatted = texcel_tui::alignCell(val_text, CELL_WIDTH, !cell.has_error && !is_text_disp);

            row_ss << styled("│").gray();
            if (is_in_rect_sel && !(is_cursor && m_sel_c0 == m_sel_c1 && m_sel_r0 == m_sel_r1)) {
                row_ss << styled(formatted).bright_white().bg_blue().bold();
            } else if (is_cursor) {
                if (m_point_mode) row_ss << styled(formatted).pure_black().bg_bright_yellow().bold();
                else if (is_focused) row_ss << styled(formatted).pure_black().bg_bright_cyan().bold();
                else row_ss << styled(formatted).pure_black().bg_bright_green().bold();
            } else if (is_target_cell) {
                row_ss << styled(formatted).pure_black().bg_bright_green().bold();
            } else if (is_input_ref) {
                row_ss << styled(formatted).pure_black().bg_bright_yellow().bold();
            } else if (is_dependent) {
                row_ss << styled(formatted).bright_magenta().bold().underline();
            } else if (cell.has_error) {
                row_ss << styled(formatted).bright_red().bold();
            } else if (cell.is_formula) {
                row_ss << styled(formatted).bright_green().bold();
            } else if (!val_text.empty()) {
                row_ss << styled(formatted).bright_white();
            } else {
                row_ss << styled(texcel_tui::alignCell("·", CELL_WIDTH, false)).dim();
            }
        }
        gotoxy(ros(), LEFT_X + 2, TOP_Y + 3 + r);
        ros() << texcel_tui::fitWidth(row_ss.str(), inner_w);
    }

    // Fila vacía de respiro entre la matriz y la leyenda
    gotoxy(ros(), LEFT_X + 2, TOP_Y + 3 + VISIBLE_ROWS);
    ros() << std::string(inner_w, ' ');

    // Leyenda semántica al pie de la Ventana 1
    gotoxy(ros(), LEFT_X + 2, TOP_Y + TOP_H - 2);
    std::ostringstream leg;
    leg << styled(" ■ ").pure_black().bg_bright_cyan() << styled("Activa ").gray()
        << styled(" ■ ").pure_black().bg_bright_yellow() << styled("Referencia ").gray()
        << styled(" ■ ").bright_magenta().bold() << styled("Afectada ").gray()
        << styled(" ■ ").bright_green().bold() << styled("Formula").gray();
    ros() << texcel_tui::fitWidth(leg.str(), inner_w);
}

inline void SpreadsheetTUI::renderInspectorWindow(const EvalPreview& preview) {
    std::string inspect_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        inspect_id = preview.target_cell;
    }

    drawWindowFrame(m_right_x, TOP_Y, m_right_w, TOP_H,
                    "Inspector & AST [" + inspect_id + "]",
                    false, true);

    SpreadsheetCell cell = m_engine.getCell(inspect_id);
    bool showing_preview = preview.active && (preview.target_cell == inspect_id);

    std::string raw_fx = showing_preview ? preview.expression : cell.raw_expression;
    std::string val_str = showing_preview
        ? (preview.valid ? preview.formatted_value : preview.error_msg)
        : (cell.raw_expression.empty() ? "(vacia)" : cell.displayValue());
    std::string rpn_str   = showing_preview ? preview.postorder_rpn : cell.postorder_rpn;
    std::string root_lbl  = showing_preview ? preview.ast_root : cell.ast_root_label;
    std::string left_lbl  = showing_preview ? preview.ast_left : cell.ast_left_label;
    std::string right_lbl = showing_preview ? preview.ast_right : cell.ast_right_label;

    const size_t inner_w = static_cast<size_t>(m_right_w - 4);
    const size_t val_max = (inner_w > 14) ? (inner_w - 13) : 20;

    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(2, m_right_x + 2, TOP_Y + rel_y, content, inner_w);
    };

    bool is_fx = cell.is_formula || (showing_preview && !preview.is_text && !raw_fx.empty() && raw_fx[0] == '=');
    bool is_txt = showing_preview ? preview.is_text : cell.is_text;
    std::string kind_badge = raw_fx.empty()
        ? styled("Celda vacia").dim().str()
        : (is_fx
            ? styled("Formula / Expresion").bright_green().bold().str()
            : (is_txt ? styled("Texto").bright_cyan().str() : styled("Valor Literal").bright_cyan().str()));
    putLine(1, styled("Celda      : ").gray().str() + styled(inspect_id).bright_yellow().bold().str() + " (" + kind_badge + ")");

    std::string fx_display = "∅";
    if (!raw_fx.empty()) {
        std::string prefix = (!raw_fx.empty() && raw_fx[0] == '=') ? "" : "= ";
        fx_display = (is_txt && !is_fx)
            ? texcel_tui::clipPlain(raw_fx, val_max)
            : (prefix + texcel_tui::clipPlain(raw_fx, val_max > 2 ? val_max - 2 : val_max));
    }
    putLine(2, styled("Contenido  : ").gray().str() + styled(fx_display).bright_cyan().bold().str());
    putLine(3, styled("Valor      : ").gray().str() + styled(texcel_tui::clipPlain(val_str, val_max)).bright_white().bold().str());

    auto formatCellList = [&](const Vector<VectorAscTraits<std::string>>& list) {
        std::ostringstream ss;
        if (list.size() == 0) ss << "ninguna";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) ss << ", ";
            std::string cid = list[i].getValue();
            ss << cid << "(" << m_engine.getCell(cid).displayValue() << ")";
        }
        return ss.str();
    };

    auto inputs = showing_preview ? preview.referenced_cells : m_engine.getCellInputs(inspect_id);
    auto deps   = m_engine.getCellDependents(inspect_id);
    putLine(4, styled("Depende de : ").gray().str() + styled(texcel_tui::clipPlain(formatCellList(inputs), val_max)).bright_yellow().str());
    putLine(5, styled("Afecta a   : ").gray().str() + styled(texcel_tui::clipPlain(formatCellList(deps), val_max)).bright_magenta().str());

    std::string ast_hdr = "─── BinaryTree AST (Euler PostOrder) ";
    while (visible_length(ast_hdr) < inner_w) ast_hdr += "─";
    putLine(6, styled(ast_hdr).gray().str());

    std::string rpn_disp = rpn_str.empty() ? "∅" : texcel_tui::clipPlain(rpn_str, inner_w > 11 ? inner_w - 11 : 20);
    putLine(7, styled("PostOrder: ").gray().str() + styled(rpn_disp).bright_green().str());

    putLine(8, "");
    putLine(9, "");
    putLine(10, "");
    putLine(11, "");
    putLine(12, "");
    if (root_lbl.empty()) {
        putLine(9,  styled("       (sin arbol de expresion)").dim().str());
    } else if (left_lbl.empty() && right_lbl.empty()) {
        putLine(9,  "             " + styled("[" + texcel_tui::clipPlain(root_lbl, 12) + "]").bright_cyan().bold().str());
        putLine(10, styled("          (Nodo hoja unico)").dim().str());
    } else {
        putLine(8,  "               " + styled("(" + texcel_tui::clipPlain(root_lbl, 6) + ")").bright_yellow().bold().str());
        putLine(9,  styled("              ╭──┴──╮").gray().str());
        std::string l_box = "[" + texcel_tui::clipPlain(left_lbl, 12) + "]";
        std::string r_box = "[" + texcel_tui::clipPlain(right_lbl, 12) + "]";
        putLine(10, "        " + styled(texcel_tui::fitWidth(l_box, 13)).bright_cyan().str() + " " + styled(r_box).bright_green().str());
    }

    putLine(13, styled("Contenedores: HashTable<AVL> + BinaryTree").dim().str());
}

inline void SpreadsheetTUI::renderCommandWindow(const EvalPreview& preview) {
    bool is_focused = (m_mode == SpreadsheetFocusMode::CommandConsole);
    drawWindowFrame(LEFT_X, BOT_Y, m_left_w, BOT_H,
                    "Consola & Editor de Formulas",
                    is_focused);

    const size_t inner_w = static_cast<size_t>(m_left_w - 4);
    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(3, LEFT_X + 2, BOT_Y + rel_y, content, inner_w);
    };

    TX disp_col = m_point_mode ? m_target_col : m_cursor_col;
    TX disp_row = m_point_mode ? m_target_row : m_cursor_row;
    std::string active_id = CellCoord(disp_col, disp_row, true).toId();

    const size_t box_inner = (inner_w > 2) ? (inner_w - 2) : 20;
    std::string prompt_label = active_id + " ❯ ";
    const size_t prefix_len = prompt_label.size();
    const size_t text_slot = (box_inner > prefix_len + 1) ? (box_inner - prefix_len - 1) : 12;

    std::string prompt_prefix = is_focused
        ? styled(prompt_label).bright_green().bold().str()
        : styled(prompt_label).cyan().str();

    std::string cursor_block = is_focused ? styled("█").bright_green().blink().str() : "";
    std::string cell_preview_raw = (!is_focused && m_input_buffer.empty())
        ? formatCellForInput(active_id)
        : "";
    const size_t max_in = (text_slot > 1) ? (text_slot - 1) : text_slot;
    m_input_cursor = std::min(m_input_cursor, m_input_buffer.size());

    std::string input_rendered;
    if (!m_input_buffer.empty()) {
        if (is_focused) {
            size_t view_start = (m_input_cursor > max_in) ? (m_input_cursor - max_in) : 0;
            std::string vis = m_input_buffer.substr(view_start, max_in);
            size_t rel_cur = m_input_cursor - view_start;

            std::string before = vis.substr(0, rel_cur);
            if (rel_cur < vis.size()) {
                std::string cur_ch(1, vis[rel_cur]);
                std::string after = vis.substr(rel_cur + 1);
                input_rendered = styled(before).bright_white().bold().str() +
                                 styled(cur_ch).pure_black().bg_bright_green().bold().str() +
                                 styled(after).bright_white().bold().str();
            } else {
                input_rendered = styled(vis).bright_white().bold().str() + cursor_block;
            }
        } else {
            input_rendered = styled(texcel_tui::clipPlain(m_input_buffer, max_in)).bright_white().bold().str();
        }
    } else if (is_focused) {
        input_rendered = cursor_block + styled("Ej: = A1 + 4  o  5  o  Ctrl+U limpiar").dim().str();
    } else if (!cell_preview_raw.empty()) {
        input_rendered = styled(texcel_tui::clipPlain(cell_preview_raw, max_in)).dim().str();
    } else {
        input_rendered = styled("[Enter] o [=] para editar " + active_id).dim().str();
    }

    std::string top_box = "╭", bot_box = "╰";
    for (size_t i = 0; i < box_inner; ++i) { top_box += "─"; bot_box += "─"; }
    top_box += "╮"; bot_box += "╯";

    putLine(1, styled(top_box).gray().str());
    putLine(2, styled("│ ").gray().str() + prompt_prefix + texcel_tui::fitWidth(input_rendered, text_slot) + styled("│").gray().str());
    putLine(3, styled(bot_box).gray().str());

    if (m_point_mode) {
        std::string pinfo = preview.valid
            ? ("= " + preview.formatted_value + "  (Point Mode: operador +-*/ o [Enter])")
            : ("Point Mode [" + active_id + "]");
        putLine(4, styled(texcel_tui::clipPlain(pinfo, inner_w)).dim().str());
    } else if (preview.active) {
        putLine(4, (isFormulaBuffer(m_input_buffer) && preview.valid)
            ? styled(texcel_tui::clipPlain("= " + preview.formatted_value, inner_w)).dim().str()
            : "");
    } else {
        SpreadsheetCell cur_cell = m_engine.getCell(active_id);
        putLine(4, (cur_cell.is_formula && !cur_cell.has_error)
            ? styled(texcel_tui::clipPlain("= " + cur_cell.displayValue(), inner_w)).dim().str()
            : "");
    }

    putLine(5, "");
    putLine(6, styled("Ejemplos: ").gray().bold().str() +
               styled("5 │ = A1 + 4 │ = A1 + B1 │ [Tab] Point Mode").dim().str());
    putLine(7, styled("Comandos: ").gray().bold().str() +
               styled("SUM / CONCAT / COUNTIF(S) / & │ demo │ clear │ quit").dim().str());
}

inline void SpreadsheetTUI::renderActivityWindow() {
    drawWindowFrame(m_right_x, BOT_Y, m_right_w, BOT_H,
                    "Eventos & Propagacion",
                    false, true);

    const size_t inner_w = static_cast<size_t>(m_right_w - 4);
    const auto& logs = m_engine.getLogs();
    const size_t max_lines = BOT_H - 3;
    size_t total = logs.size();
    size_t start_idx = (total > max_lines) ? (total - max_lines) : 0;

    for (size_t i = 0; i < max_lines; ++i) {
        TX abs_y = BOT_Y + 1 + static_cast<TX>(i);
        if (start_idx + i < total) {
            std::string line = logs[start_idx + i].getValue();
            std::string clipped = texcel_tui::clipPlain(line, inner_w);
            std::string styled_line;
            if (line.starts_with("✔")) styled_line = styled(clipped).bright_green().str();
            else if (line.starts_with("⚠") || line.starts_with("✖")) styled_line = styled(clipped).bright_red().str();
            else styled_line = styled(clipped).white().str();
            drawWindowLineWithSelection(4, m_right_x + 2, abs_y, styled_line, inner_w);
        } else {
            drawWindowLineWithSelection(4, m_right_x + 2, abs_y, "", inner_w);
        }
    }

    std::string footer = "Total ops: " + std::to_string(total) + " │ Ciclos: protegidos";
    drawWindowLineWithSelection(4, m_right_x + 2, BOT_Y + BOT_H - 2, styled(footer).dim().str(), inner_w);
}

inline void SpreadsheetTUI::renderFileManagerModal() {
    const TX modal_w = std::min<TX>(60, m_total_width - 8);
    const TX modal_h = 16;
    const TX mx = (m_total_width - modal_w) / 2;
    const TX my = (TOTAL_HEIGHT - modal_h) / 2;
    const size_t inner_w = static_cast<size_t>(modal_w - 4);

    drawWindowFrame(mx, my, modal_w, modal_h, "Gestor de Hojas & Configuracion", true, false, true);

    auto putRow = [&](TX rel_y, bool selected, const std::string& content) {
        gotoxy(ros(), mx + 2, my + rel_y);
        std::string line = (selected ? " ▸ " : "   ") + content;
        if (selected) ros() << styled(texcel_tui::fitWidth(line, inner_w)).pure_black().bg_bright_cyan().bold();
        else ros() << texcel_tui::fitWidth(line, inner_w);
    };

    std::string auto_txt = m_autosave
        ? styled("[ON] ").bright_green().bold().str()
        : styled("[OFF]").gray().str();
    putRow(1, m_modal_idx == 0, "[A] Autoguardado al editar : " + (m_modal_idx == 0 ? std::string(m_autosave ? "[ON]" : "[OFF]") : auto_txt));
    putRow(2, m_modal_idx == 1, "[S] Guardar hoja actual (" + m_current_sheet + TexcelConfig::SHEET_EXT + ")");

    if (m_modal_new_mode) {
        putRow(3, true, "Nueva hoja (Enter): " + m_modal_new_name + "█");
    } else {
        putRow(3, m_modal_idx == 2, "[N] Crear nueva hoja limpia...");
    }

    gotoxy(ros(), mx + 2, my + 4);
    std::string sep = "─── Hojas guardadas en output/sheets/ ";
    while (visible_length(sep) < inner_w) sep += "─";
    ros() << texcel_tui::fitWidth(styled(sep).gray().str(), inner_w);

    const TX list_rows = modal_h - 8;
    TX file_cursor = (m_modal_idx >= 3) ? (m_modal_idx - 3) : 0;
    TX scroll_f = (file_cursor >= list_rows) ? (file_cursor - list_rows + 1) : 0;

    if (m_sheet_files.size() == 0) {
        gotoxy(ros(), mx + 2, my + 6);
        ros() << texcel_tui::fitWidth(styled("   (No hay hojas guardadas aun. Pulsa [S] o [N])").dim().str(), inner_w);
    } else {
        for (TX i = 0; i < list_rows; ++i) {
            TX idx = scroll_f + i;
            if (idx < static_cast<TX>(m_sheet_files.size())) {
                std::string fname = m_sheet_files[static_cast<size_t>(idx)].getValue();
                std::string tag = (fname == m_current_sheet) ? " ★ (actual)" : "";
                putRow(5 + i, m_modal_idx == 3 + idx, fname + TexcelConfig::SHEET_EXT + tag);
            } else {
                gotoxy(ros(), mx + 2, my + 5 + i);
                ros() << std::string(inner_w, ' ');
            }
        }
    }

    gotoxy(ros(), mx + 2, my + modal_h - 2);
    std::string help = m_modal_new_mode
        ? "Escribe el nombre y pulsa [Enter] │ [Esc] Cancelar"
        : "[↑/↓] Navegar │ [Enter] Seleccionar │ [Supr] Borrar │ [Esc] Cerrar";
    ros() << texcel_tui::fitWidth(styled(texcel_tui::clipPlain(help, inner_w)).dim().str(), inner_w);
}

inline void SpreadsheetTUI::renderAll(bool force_clear) {
    bool resized = updateLayout();
    clampScroll();
    TX target_c = m_point_mode ? m_target_col : m_cursor_col;
    TX target_r = m_point_mode ? m_target_row : m_cursor_row;
    std::string active_id = CellCoord(target_c, target_r, true).toId();
    EvalPreview preview = m_engine.previewCommand(m_input_buffer, active_id);

    std::ostringstream frame_buf;
    if ((resized || force_clear) && m_interactive_alt_screen) {
        frame_buf << "\033[2J\033[1;1H\033[s";
    }
    m_render_os = &frame_buf;

    renderHeaderBar(preview);
    renderGridWindow(preview);
    renderInspectorWindow(preview);
    renderCommandWindow(preview);
    renderActivityWindow();
    if (m_modal_open) {
        renderFileManagerModal();
    }
    TX end_y = m_interactive_alt_screen
        ? std::min<TX>(TOTAL_HEIGHT - 1, std::max<TX>(0, m_term_rows - 1))
        : TOTAL_HEIGHT;
    gotoxy(ros(), 0, end_y);

    m_render_os = &m_os;
    m_os << "\033[?2026h" << frame_buf.str() << "\033[?2026l" << std::flush;
}

#endif // __TEXCEL_HELPERS_TUI_RENDERER_H__
