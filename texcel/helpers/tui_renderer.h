/**
 * @file tui_renderer.h
 * @ingroup texcel
 * @brief Funciones puras de renderizado 2D de las 4 ventanas y del modal flotante sobre `TuiState`.
 */

#ifndef __TEXCEL_HELPERS_TUI_RENDERER_H__
#define __TEXCEL_HELPERS_TUI_RENDERER_H__

#include "tui_state.h"
#include "tui_text.h"
#include "../../utilities/gotoxy.h"
#include "../../utilities/ansi_style.h"
#include "../../utilities/ansi_metrics.h"
#include "../../utilities/clipped_graph_renderer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace texcel_tui {

inline void drawVerticalScrollbar(TuiState& st, TX x, TX y, TX h, TX scroll_pos, TX max_scroll) {
    TX thumb_h = std::min<TX>(2, h);
    TX thumb = (scroll_pos * (h - thumb_h)) / std::max<TX>(1, max_scroll);
    for (TX r = 0; r < h; ++r) {
        gotoxy(st.ros(), x, y + r);
        st.ros() << ((r >= thumb && r < thumb + thumb_h) ? styled("█").bright_cyan().bold() : styled("│").dim());
    }
}

inline void drawHorizontalScrollbar(TuiState& st, TX x0, TX y, TX len, TX thumb_w, TX scroll_pos, TX max_scroll, bool active_border = false) {
    TX thumb = (scroll_pos * (len - thumb_w)) / std::max<TX>(1, max_scroll);
    gotoxy(st.ros(), x0, y);
    for (TX i = 0; i < len; ++i) {
        if (i >= thumb && i < thumb + thumb_w) st.ros() << styled("▬").bright_cyan().bold();
        else st.ros() << (active_border ? styled("─").bright_cyan().bold() : styled("─").gray());
    }
}

inline void drawWindowLineWithSelection(TuiState& st, TX win_id, TX start_x, TX abs_y,
                                        const std::string& styled_line, size_t inner_w) {
    std::string plain = stripAnsi(styled_line);
    if (abs_y >= 0 && abs_y < TuiState::TOTAL_HEIGHT && win_id >= 1 && win_id <= 4) {
        st.win_lines[win_id][abs_y] = plain;
    }

    if (!st.has_text_sel || st.text_win != win_id || !st.isSelectableWindowRow(win_id, abs_y)) {
        gotoxy(st.ros(), start_x, abs_y);
        st.ros() << fitWidth(styled_line, inner_w);
        return;
    }

    TX sx0, sy0, sx1, sy1;
    normalizeTextRange(st.tx0, st.ty0, st.tx1, st.ty1, sx0, sy0, sx1, sy1);
    if (abs_y < sy0 || abs_y > sy1) {
        gotoxy(st.ros(), start_x, abs_y);
        st.ros() << fitWidth(styled_line, inner_w);
        return;
    }

    std::string padded = plain;
    size_t p_vlen = visible_length(padded);
    if (p_vlen < inner_w) padded += std::string(inner_w - p_vlen, ' ');
    TX c_start = (abs_y == sy0) ? std::max<TX>(0, sx0 - start_x) : 0;
    TX c_end   = (abs_y == sy1) ? std::min<TX>(static_cast<TX>(inner_w) - 1, sx1 - start_x) : static_cast<TX>(inner_w) - 1;

    gotoxy(st.ros(), start_x, abs_y);
    if (c_start > c_end || c_start >= static_cast<TX>(inner_w)) {
        st.ros() << fitWidth(styled_line, inner_w);
        return;
    }

    // Tono de sombreado de fondo propio según la ventana:
    // Ventana 2 (Inspector): petróleo cian (color8 23), Ventana 3 (Consola): verde bosque (color8 22), Ventana 4 (Eventos): índigo (color8 53)
    uint8_t bg_tone = (win_id == 2) ? 23 : ((win_id == 3) ? 22 : 53);

    std::string before = utf8Slice(padded, 0, static_cast<size_t>(c_start));
    std::string after  = utf8Slice(padded, static_cast<size_t>(c_end) + 1, inner_w);

    // Pintar el tramo seleccionado excluyendo cualquier barra de división interna ('│', '─', etc.)
    std::ostringstream mid_ss;
    for (size_t cp = static_cast<size_t>(c_start); cp <= static_cast<size_t>(c_end); ++cp) {
        std::string g = utf8At(padded, cp);
        if (isDividerGlyph(g)) {
            mid_ss << styled(g).gray();
        } else {
            mid_ss << styled(g).bright_white().bg_color8(bg_tone).bold();
        }
    }
    st.ros() << before << mid_ss.str() << after;
}

inline void drawWindowFrame(TuiState& st, TX x, TX y, TX w, TX h,
                            const std::string& title,
                            bool active_focus,
                            bool accent_secondary = false,
                            bool clear_interior = false) {
    auto styleBorder = [&](const std::string& glyph) {
        if (active_focus) return styled(glyph).bright_cyan().bold().str();
        if (accent_secondary) return styled(glyph).cyan().str();
        return styled(glyph).gray().str();
    };

    std::string badge = active_focus ? " ● ACTIVA " : " ";
    size_t inner_w = (w >= 2) ? static_cast<size_t>(w - 2) : 0;
    std::string full_title = " " + title + badge;
    if (inner_w > 2 && visible_length(full_title) > inner_w - 2) {
        full_title = fitWidth(full_title, inner_w - 2);
    }
    size_t title_vlen = visible_length(full_title);

    gotoxy(st.ros(), x, y);
    st.ros() << styleBorder("╭─");
    std::string styled_title = active_focus
        ? styled(full_title).badge_cyan().str()
        : (accent_secondary
            ? styled(full_title).bright_white().bold().str()
            : styled(full_title).gray().str());
    st.ros() << styled_title;

    size_t used = 1 + title_vlen;
    if (inner_w > used) st.ros() << styleBorder(repeatGlyph("─", inner_w - used));
    st.ros() << styleBorder("╮");

    if (clear_interior) {
        std::string blank_inner(inner_w, ' ');
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(st.ros(), x, row);
            st.ros() << styleBorder("│") << blank_inner << styleBorder("│");
        }
    } else {
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(st.ros(), x, row);
            st.ros() << styleBorder("│");
            gotoxy(st.ros(), x + w - 1, row);
            st.ros() << styleBorder("│");
        }
    }

    gotoxy(st.ros(), x, y + h - 1);
    st.ros() << styleBorder("╰" + repeatGlyph("─", inner_w) + "╯");
}

inline void renderHeaderBar(TuiState& st, const EvalPreview& preview) {
    gotoxy(st.ros(), 0, 0);
    std::string mode_badge;
    if (st.point_mode) {
        mode_badge = styled(fitWidth(" POINT MODE: APUNTAR CELDA ", 27)).badge_yellow();
    } else if (st.mode == SpreadsheetFocusMode::GridNavigation) {
        mode_badge = styled(fitWidth(" MODO 1: NAVEGACION MATRIZ ", 27)).badge_cyan();
    } else {
        mode_badge = styled(fitWidth(" MODO 2: CONSOLA COMANDOS ", 27)).badge_green();
    }

    std::string focus_id = st.activeCellId();
    if (preview.active && !preview.target_cell.empty()) {
        focus_id = preview.target_cell;
    }

    std::string auto_badge = st.config.autosave
        ? styled(fitWidth(" Auto:ON ", 10)).badge_green().str()
        : styled(fitWidth(" Auto:OFF ", 10)).gray().str();

    std::ostringstream oss;
    oss << styled(" ◆ Texcel ").badge_blue()
        << " " << mode_badge
        << styled(" │ ").gray() << styled(st.config.current_sheet + TexcelConfig::SHEET_EXT).bright_yellow().bold()
        << " " << auto_badge
        << styled(" │ Celda:").gray() << styled(fitWidth(focus_id, 3)).bright_cyan().bold()
        << styled(" │ [Ctrl+S] Guardar [Ctrl+O] Hojas [Tab] Modo [q] Salir").gray();

    st.ros() << fitWidth(oss.str(), static_cast<size_t>(st.total_width));
}

inline void renderGridWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = (st.mode == SpreadsheetFocusMode::GridNavigation);
    drawWindowFrame(st, TuiState::LEFT_X, TuiState::TOP_Y, st.left_w, TuiState::TOP_H, "Hoja de Calculo", is_focused);

    std::string active_id = CellCoord(st.cursor_col, st.cursor_row, true).toId();
    std::string preview_target_id = active_id;
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        preview_target_id = preview.target_cell;
        if (!st.point_mode) active_id = preview_target_id;
    }

    auto inputs     = preview.active ? preview.referenced_cells : st.engine.getCellInputs(active_id);
    auto dependents = st.engine.getCellDependents(active_id);
    const size_t inner_w = static_cast<size_t>(st.left_w - 4);

    TX sel_cmin = 0, sel_rmin = 0, sel_cmax = 0, sel_rmax = 0;
    bool any_rect_sel = st.has_cell_sel || st.has_fill_sel;
    if (any_rect_sel) {
        normalizeCellRange(st.sel_c0, st.sel_r0, st.sel_c1, st.sel_r1, sel_cmin, sel_rmin, sel_cmax, sel_rmax);
    }
    bool has_multi_sel = any_rect_sel && (sel_cmin != sel_cmax || sel_rmin != sel_rmax);

    {
        std::ostringstream hdr_ss;
        if (st.namebox_editing) {
            std::string nb = st.namebox_buffer + "█";
            if (visible_length(nb) > TuiState::ROW_HEADER_W) {
                nb = fitWidth(nb, TuiState::ROW_HEADER_W);
            }
            hdr_ss << styled(fitWidth(nb, TuiState::ROW_HEADER_W)).badge_yellow();
        } else {
            std::string corner_txt = has_multi_sel ? st.selectedRangeStr() : active_id;
            hdr_ss << styled(alignCell(corner_txt, TuiState::ROW_HEADER_W, false)).bright_yellow().bold();
        }
        for (TX c = 0; c < st.visible_cols; ++c) {
            TX col_idx = st.scroll_col + c;
            std::string hdr = alignCell(CellCoord::colToLabel(col_idx), TuiState::CELL_WIDTH, false);
            hdr_ss << styled("│").gray();
            bool col_in_sel = st.has_cell_sel && (col_idx >= sel_cmin && col_idx <= sel_cmax);
            if (col_in_sel) {
                hdr_ss << styled(hdr).badge_blue();
            } else if (col_idx == st.cursor_col) {
                hdr_ss << styled(hdr).bright_cyan().bold();
            } else {
                hdr_ss << styled(hdr).white().bold();
            }
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + 1);
        st.ros() << fitWidth(hdr_ss.str(), inner_w);
    }

    {
        std::string sep_line = repeatGlyph("─", TuiState::ROW_HEADER_W);
        for (TX c = 0; c < st.visible_cols; ++c) {
            sep_line += (c == 0) ? "┼" : "─";
            sep_line += repeatGlyph("─", TuiState::CELL_WIDTH);
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + 2);
        st.ros() << fitWidth(styled(sep_line).gray().str(), inner_w);
    }

    for (TX r = 0; r < TuiState::VISIBLE_ROWS; ++r) {
        TX row_idx = st.scroll_row + r;
        std::ostringstream row_ss;

        std::ostringstream row_lbl;
        row_lbl << std::setw(4) << (row_idx + 1) << "    ";
        bool row_in_sel = st.has_cell_sel && (row_idx >= sel_rmin && row_idx <= sel_rmax);
        if (row_in_sel) {
            row_ss << styled(row_lbl.str()).badge_blue();
        } else if (row_idx == st.cursor_row) {
            row_ss << styled(row_lbl.str()).bright_cyan().bold();
        } else {
            row_ss << styled(row_lbl.str()).gray();
        }

        for (TX c = 0; c < st.visible_cols; ++c) {
            TX col_idx = st.scroll_col + c;
            std::string cid = CellCoord(col_idx, row_idx, true).toId();
            SpreadsheetCell cell = st.engine.getCell(cid);

            bool is_cursor      = (cid == active_id);
            bool is_target_cell = st.point_mode && (cid == preview_target_id);
            bool is_in_rect_sel = st.has_cell_sel &&
                                  (col_idx >= sel_cmin && col_idx <= sel_cmax && row_idx >= sel_rmin && row_idx <= sel_rmax);
            bool is_in_fill_sel = st.has_fill_sel &&
                                  (col_idx >= sel_cmin && col_idx <= sel_cmax && row_idx >= sel_rmin && row_idx <= sel_rmax);
            bool is_fill_tip    = st.has_fill_sel && (col_idx == st.sel_c1 && row_idx == st.sel_r1);
            bool is_input_ref   = texcel_detail::containsStr(inputs, cid);
            bool is_dependent   = texcel_detail::containsStr(dependents, cid);

            std::string val_text = cell.displayValue();
            bool is_text_disp = cell.is_text;
            if ((is_target_cell || (!st.point_mode && is_cursor)) && preview.active && preview.valid) {
                val_text = preview.formatted_value;
                is_text_disp = preview.is_text;
            }
            bool is_bool_true = false, is_bool_false = false;
            if (!cell.has_error && is_text_disp) {
                std::string upper_val = texcel_detail::toUpper(val_text);
                if (upper_val == "TRUE") {
                    val_text = "■";
                    is_bool_true = true;
                } else if (upper_val == "FALSE") {
                    val_text = "□";
                    is_bool_false = true;
                }
            }
            std::string formatted = alignCell(val_text, TuiState::CELL_WIDTH, !cell.has_error && !is_text_disp);

            if (is_fill_tip && formatted.size() >= 2) {
                bool left = (st.sel_c1 < st.sel_c0);
                bool up   = (st.sel_r1 < st.sel_r0);
                std::string ramp = up ? (left ? "◤" : "◥") : (left ? "◣" : "◢");
                if (left) formatted = ramp + formatted.substr(1);
                else      formatted = formatted.substr(0, formatted.size() - 1) + ramp;
            }

            row_ss << styled("│").gray();
            if (is_cursor) {
                row_ss << styled(formatted).badge_cyan();
            } else if (is_in_fill_sel) {
                row_ss << styled(formatted).badge_green();
            } else if (is_in_rect_sel) {
                row_ss << styled(formatted).badge_blue();
            } else if (is_target_cell) {
                row_ss << styled(formatted).badge_green();
            } else if (is_input_ref) {
                row_ss << styled(formatted).badge_yellow();
            } else if (is_dependent) {
                row_ss << styled(formatted).bright_magenta().bold().underline();
            } else if (cell.has_error) {
                row_ss << styled(formatted).bright_red().bold();
            } else if (is_bool_true) {
                row_ss << styled(formatted).bright_green().bold();
            } else if (is_bool_false) {
                row_ss << styled(formatted).dim();
            } else if (cell.is_formula) {
                row_ss << styled(formatted).bright_green().bold();
            } else if (!val_text.empty()) {
                row_ss << styled(formatted).bright_white();
            } else {
                row_ss << styled(alignCell("·", TuiState::CELL_WIDTH, false)).dim();
            }
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + 3 + r);
        st.ros() << fitWidth(row_ss.str(), inner_w);
    }

    gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + 3 + TuiState::VISIBLE_ROWS);
    st.ros() << std::string(inner_w, ' ');

    gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + TuiState::TOP_H - 2);
    std::ostringstream leg;
    leg << styled(" ■ ").badge_cyan() << styled("Activa ").gray()
        << styled(" ■ ").badge_yellow() << styled("Referencia ").gray()
        << styled(" ■ ").bright_magenta().bold() << styled("Afectada ").gray()
        << styled(" ■ ").bright_green().bold() << styled("Formula").gray();
    st.ros() << fitWidth(leg.str(), inner_w);

    // Mini-scrollbars de la Matriz (vertical y horizontal)
    drawVerticalScrollbar(st, TuiState::LEFT_X + st.left_w - 1, TuiState::TOP_Y + 3,
                          TuiState::VISIBLE_ROWS, st.scroll_row, CellCoord::MAX_ROWS - TuiState::VISIBLE_ROWS);

    TX h_x0 = TuiState::LEFT_X + 2 + TuiState::ROW_HEADER_W + 1;
    TX h_len = (TuiState::LEFT_X + st.left_w - 2) - h_x0 + 1;
    if (h_len >= 10) {
        drawHorizontalScrollbar(st, h_x0, TuiState::TOP_Y + TuiState::TOP_H - 1,
                                h_len, std::min<TX>(9, h_len / 2),
                                st.scroll_col, CellCoord::MAX_COLS - st.visible_cols, is_focused);
    }
}

inline void renderInspectorWindow(TuiState& st, const EvalPreview& preview) {
    std::string inspect_id = CellCoord(st.cursor_col, st.cursor_row, true).toId();
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        inspect_id = preview.target_cell;
    }

    drawWindowFrame(st, st.right_x, TuiState::TOP_Y, st.right_w, TuiState::TOP_H,
                    "Inspector & AST [" + inspect_id + "]",
                    false, true);

    SpreadsheetCell cell = st.engine.getCell(inspect_id);
    bool showing_preview = preview.active && (preview.target_cell == inspect_id);

    std::string raw_fx = showing_preview ? preview.expression : cell.raw_expression;
    std::string val_str = showing_preview
        ? (preview.valid ? preview.formatted_value : preview.error_msg)
        : (cell.raw_expression.empty() ? "(vacia)" : cell.displayValue());
    std::string rpn_str   = showing_preview ? preview.postorder_rpn : cell.postorder_rpn;

    const size_t inner_w = static_cast<size_t>(st.right_w - 4);
    const size_t val_max = (inner_w > 14) ? (inner_w - 13) : 20;

    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(st, 2, st.right_x + 2, TuiState::TOP_Y + rel_y, content, inner_w);
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
            ? clipPlain(raw_fx, val_max)
            : (prefix + clipPlain(raw_fx, val_max > 2 ? val_max - 2 : val_max));
    }
    putLine(2, styled("Contenido  : ").gray().str() + styled(fx_display).bright_cyan().bold().str());
    putLine(3, styled("Valor      : ").gray().str() + styled(clipPlain(val_str, val_max)).bright_white().bold().str());

    auto formatCellList = [&](const Vector<VectorAscTraits<std::string>>& list) {
        std::ostringstream ss;
        if (list.size() == 0) ss << "ninguna";
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) ss << ", ";
            std::string cid = list[i].getValue();
            ss << cid << "(" << st.engine.getCell(cid).displayValue() << ")";
        }
        return ss.str();
    };

    auto inputs = showing_preview ? preview.referenced_cells : st.engine.getCellInputs(inspect_id);
    auto deps   = st.engine.getCellDependents(inspect_id);
    putLine(4, styled("Depende de : ").gray().str() + styled(clipPlain(formatCellList(inputs), val_max)).bright_yellow().str());
    putLine(5, styled("Afecta a   : ").gray().str() + styled(clipPlain(formatCellList(deps), val_max)).bright_magenta().str());

    std::string orient_tag = st.ast_horizontal ? "[H]" : "[V]";
    putLine(6, styled(makeSectionHeader("BinaryTree AST " + orient_tag + " ([O] rotar)", inner_w)).gray().str());

    std::string rpn_disp = rpn_str.empty() ? "∅" : clipPlain(rpn_str, inner_w > 11 ? inner_w - 11 : 20);
    putLine(7, styled("PostOrder: ").gray().str() + styled(rpn_disp).bright_green().str());

    VisualGraph vg = (!is_txt || is_fx)
        ? st.engine.buildAstVisualGraph(raw_fx, st.ast_horizontal)
        : VisualGraph{};
    st.ast_graph_w = vg.width;
    st.ast_graph_h = vg.height;
    st.clampAstScroll();

    if (vg.nodes.empty() || vg.width <= 0 || vg.height <= 0) {
        for (TX row = 0; row < TuiState::AST_VIEW_H; ++row) putLine(8 + row, "");
        putLine(9, styled("       (sin arbol de expresion)").dim().str());
    } else {
        ClippedGraphRenderer ast_renderer(
            st.ros(), static_cast<TX>(inner_w), TuiState::AST_VIEW_H,
            st.ast_scroll_x, st.ast_scroll_y, 0, 0, true, ConsoleLineStyle::Rounded);
        std::vector<std::string> ast_lines = ast_renderer.renderToLines(vg);
        for (TX row = 0; row < TuiState::AST_VIEW_H; ++row) {
            const std::string line = (row < static_cast<TX>(ast_lines.size()))
                ? ast_lines[static_cast<size_t>(row)]
                : std::string();
            putLine(8 + row, line);
        }
    }

    // Scrollbars del navegador AST en los bordes de Ventana 2 cuando hay desbordamiento
    if (vg.height > TuiState::AST_VIEW_H) {
        drawVerticalScrollbar(st, st.right_x + st.right_w - 1, TuiState::TOP_Y + 8,
                              TuiState::AST_VIEW_H, st.ast_scroll_y, vg.height - TuiState::AST_VIEW_H);
    }
    if (vg.width > static_cast<TX>(inner_w)) {
        TX h_len = (st.right_x + st.right_w - 3) - (st.right_x + 2) + 1;
        if (h_len >= 6) {
            drawHorizontalScrollbar(st, st.right_x + 2, TuiState::TOP_Y + TuiState::TOP_H - 1,
                                    h_len, std::min<TX>(9, std::max<TX>(3, h_len / 3)),
                                    st.ast_scroll_x, vg.width - static_cast<TX>(inner_w));
        }
    }
}

inline void renderCommandWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = (st.mode == SpreadsheetFocusMode::CommandConsole);
    drawWindowFrame(st, TuiState::LEFT_X, TuiState::BOT_Y, st.left_w, TuiState::BOT_H,
                    "Consola & Editor de Formulas",
                    is_focused);

    const size_t inner_w = static_cast<size_t>(st.left_w - 4);
    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(st, 3, TuiState::LEFT_X + 2, TuiState::BOT_Y + rel_y, content, inner_w);
    };

    std::string active_id = st.activeCellId();

    const size_t box_inner = (inner_w > 2) ? (inner_w - 2) : 20;
    std::string prompt_label = active_id + " ❯ ";
    const size_t prefix_len = visible_length(prompt_label);
    const size_t text_slot = (box_inner > prefix_len + 1) ? (box_inner - prefix_len - 1) : 12;

    std::string prompt_prefix = is_focused
        ? styled(prompt_label).bright_green().bold().str()
        : styled(prompt_label).cyan().str();

    std::string cursor_block = is_focused ? styled("█").bright_green().blink().str() : "";
    std::string cell_preview_raw = (!is_focused && st.input_buffer.empty())
        ? st.formatCellForInput(active_id)
        : "";
    const size_t max_in = (text_slot > 1) ? (text_slot - 1) : text_slot;
    st.input_cursor = std::min(st.input_cursor, st.input_buffer.size());

    std::string input_rendered;
    if (!st.input_buffer.empty()) {
        if (is_focused) {
            size_t view_start = (st.input_cursor > max_in) ? (st.input_cursor - max_in) : 0;
            std::string vis = st.input_buffer.substr(view_start, max_in);
            size_t rel_cur = st.input_cursor - view_start;

            std::string before = vis.substr(0, rel_cur);
            if (rel_cur < vis.size()) {
                std::string cur_ch(1, vis[rel_cur]);
                std::string after = vis.substr(rel_cur + 1);
                input_rendered = styled(before).bright_white().bold().str() +
                                 styled(cur_ch).badge_green().str() +
                                 styled(after).bright_white().bold().str();
            } else {
                input_rendered = styled(vis).bright_white().bold().str() + cursor_block;
            }
        } else {
            input_rendered = styled(clipPlain(st.input_buffer, max_in)).bright_white().bold().str();
        }
    } else if (is_focused) {
        input_rendered = cursor_block + styled("Ej: = A1 + 4  o  5  o  Ctrl+U limpiar").dim().str();
    } else if (!cell_preview_raw.empty()) {
        input_rendered = styled(clipPlain(cell_preview_raw, max_in)).dim().str();
    } else {
        input_rendered = styled("[Enter] o [=] para editar " + active_id).dim().str();
    }

    std::string h_bar = repeatGlyph("─", box_inner);
    putLine(1, styled("╭" + h_bar + "╮").gray().str());
    putLine(2, styled("│ ").gray().str() + prompt_prefix + fitWidth(input_rendered, text_slot) + styled("│").gray().str());
    putLine(3, styled("╰" + h_bar + "╯").gray().str());

    if (st.point_mode) {
        std::string pinfo = preview.valid
            ? ("= " + preview.formatted_value + "  (Point Mode: operador +-*/ o [Enter])")
            : ("Point Mode [" + active_id + "]");
        putLine(4, styled(clipPlain(pinfo, inner_w)).dim().str());
    } else if (preview.active) {
        putLine(4, (TuiState::isFormulaBuffer(st.input_buffer) && preview.valid)
            ? styled(clipPlain("= " + preview.formatted_value, inner_w)).dim().str()
            : "");
    } else {
        SpreadsheetCell cur_cell = st.engine.getCell(active_id);
        putLine(4, (cur_cell.is_formula && !cur_cell.has_error)
            ? styled(clipPlain("= " + cur_cell.displayValue(), inner_w)).dim().str()
            : "");
    }

    putLine(5, "");
    putLine(6, styled("Ejemplos: ").gray().bold().str() +
               styled("5 │ = A1 + 4 │ = A1 + B1 │ [Tab] Point Mode").dim().str());
    putLine(7, styled("Comandos: ").gray().bold().str() +
               styled("SUM / CONCAT / COUNTIF(S) / & │ demo │ clear │ quit").dim().str());
}

inline void renderActivityWindow(TuiState& st) {
    drawWindowFrame(st, st.right_x, TuiState::BOT_Y, st.right_w, TuiState::BOT_H,
                    "Eventos & Propagacion",
                    false, true);

    const size_t inner_w = static_cast<size_t>(st.right_w - 4);
    const auto& logs = st.engine.getLogs();
    const size_t max_lines = TuiState::BOT_H - 3;
    size_t total = logs.size();
    size_t start_idx = (total > max_lines) ? (total - max_lines) : 0;

    for (size_t i = 0; i < max_lines; ++i) {
        TX abs_y = TuiState::BOT_Y + 1 + static_cast<TX>(i);
        if (start_idx + i < total) {
            std::string line = logs[start_idx + i].getValue();
            std::string clipped = clipPlain(line, inner_w);
            std::string styled_line;
            if (line.starts_with("✔")) styled_line = styled(clipped).bright_green().str();
            else if (line.starts_with("⚠") || line.starts_with("✖")) styled_line = styled(clipped).bright_red().str();
            else styled_line = styled(clipped).white().str();
            drawWindowLineWithSelection(st, 4, st.right_x + 2, abs_y, styled_line, inner_w);
        } else {
            drawWindowLineWithSelection(st, 4, st.right_x + 2, abs_y, "", inner_w);
        }
    }

    std::string footer = "Total ops: " + std::to_string(total) + " │ Ciclos: protegidos";
    drawWindowLineWithSelection(st, 4, st.right_x + 2, TuiState::BOT_Y + TuiState::BOT_H - 2, styled(footer).dim().str(), inner_w);
}

inline void renderFileManagerModal(TuiState& st) {
    const TX modal_w = std::min<TX>(60, st.total_width - 8);
    const TX modal_h = 16;
    const TX mx = (st.total_width - modal_w) / 2;
    const TX my = (TuiState::TOTAL_HEIGHT - modal_h) / 2;
    const size_t inner_w = static_cast<size_t>(modal_w - 4);

    drawWindowFrame(st, mx, my, modal_w, modal_h, "Gestor de Hojas & Configuracion", true, false, true);

    auto putRow = [&](TX rel_y, bool selected, const std::string& content) {
        gotoxy(st.ros(), mx + 2, my + rel_y);
        std::string line = (selected ? " ▸ " : "   ") + content;
        if (selected) st.ros() << styled(fitWidth(line, inner_w)).badge_cyan();
        else st.ros() << fitWidth(line, inner_w);
    };

    std::string auto_txt = st.config.autosave
        ? styled("[ON] ").bright_green().bold().str()
        : styled("[OFF]").gray().str();
    putRow(1, st.modal_idx == 0, "[A] Autoguardado al editar : " + (st.modal_idx == 0 ? std::string(st.config.autosave ? "[ON]" : "[OFF]") : auto_txt));
    putRow(2, st.modal_idx == 1, "[S] Guardar hoja actual (" + st.config.current_sheet + TexcelConfig::SHEET_EXT + ")");

    if (st.modal_new_mode) {
        putRow(3, true, "Nueva hoja (Enter): " + st.modal_new_name + "█");
    } else {
        putRow(3, st.modal_idx == 2, "[N] Crear nueva hoja limpia...");
    }

    gotoxy(st.ros(), mx + 2, my + 4);
    st.ros() << fitWidth(styled(makeSectionHeader("Hojas guardadas en output/sheets/", inner_w)).gray().str(), inner_w);

    const TX list_rows = modal_h - 8;
    TX file_cursor = (st.modal_idx >= 3) ? (st.modal_idx - 3) : 0;
    TX scroll_f = (file_cursor >= list_rows) ? (file_cursor - list_rows + 1) : 0;

    if (st.config.sheet_files.size() == 0) {
        gotoxy(st.ros(), mx + 2, my + 6);
        st.ros() << fitWidth(styled("   (No hay hojas guardadas aun. Pulsa [S] o [N])").dim().str(), inner_w);
    } else {
        for (TX i = 0; i < list_rows; ++i) {
            TX idx = scroll_f + i;
            if (idx < static_cast<TX>(st.config.sheet_files.size())) {
                std::string fname = st.config.sheet_files[static_cast<size_t>(idx)].getValue();
                std::string tag = (fname == st.config.current_sheet) ? " ★ (actual)" : "";
                putRow(5 + i, st.modal_idx == 3 + idx, fname + TexcelConfig::SHEET_EXT + tag);
            } else {
                gotoxy(st.ros(), mx + 2, my + 5 + i);
                st.ros() << std::string(inner_w, ' ');
            }
        }
    }

    gotoxy(st.ros(), mx + 2, my + modal_h - 2);
    std::string help = st.modal_new_mode
        ? "Escribe el nombre y pulsa [Enter] │ [Esc] Cancelar"
        : "[↑/↓] Navegar │ [Enter] Seleccionar │ [Supr] Borrar │ [Esc] Cerrar";
    st.ros() << fitWidth(styled(clipPlain(help, inner_w)).dim().str(), inner_w);
}

inline void renderAll(TuiState& st, bool force_clear = false) {
    bool resized = st.updateLayout();
    st.clampScroll();
    std::string active_id = st.activeCellId();
    EvalPreview preview = st.engine.previewCommand(st.input_buffer, active_id);

    std::ostringstream frame_buf;
    if ((resized || force_clear) && st.interactive_alt_screen) {
        frame_buf << "\033[2J\033[1;1H\033[s";
    }
    st.render_os = &frame_buf;

    renderHeaderBar(st, preview);
    renderGridWindow(st, preview);
    renderInspectorWindow(st, preview);
    renderCommandWindow(st, preview);
    renderActivityWindow(st);
    if (st.modal_open) {
        renderFileManagerModal(st);
    }
    TX end_y = st.interactive_alt_screen
        ? std::min<TX>(TuiState::TOTAL_HEIGHT - 1, std::max<TX>(0, st.term_rows - 1))
        : TuiState::TOTAL_HEIGHT;
    gotoxy(st.ros(), 0, end_y);

    st.render_os = &st.os;
    st.os << "\033[?2026h" << frame_buf.str() << "\033[?2026l" << std::flush;
}

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_TUI_RENDERER_H__
