/**
 * @file renderer.h
 * @ingroup texcel
 * @brief Funciones puras de renderizado 2D de las 4 ventanas y del modal flotante sobre `TuiState`.
 */

#ifndef __TEXCEL_HELPERS_RENDERER_H__
#define __TEXCEL_HELPERS_RENDERER_H__

#include "state.h"
#include "text.h"
#include "../../utilities/gotoxy.h"
#include "../../utilities/ansi_style.h"
#include "../../utilities/ansi_metrics.h"
#include "../../utilities/clipped_graph_renderer.h"
#include "../../utilities/layouts/vertical_tree_layout.h"
#include "../../utilities/layouts/horizontal_tree_layout.h"
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

    if (!st.hasTextSel() || st.text_win != win_id || !st.isSelectableWindowRow(win_id, abs_y)) {
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
    // Inspector: petróleo cian (23), Consola: verde bosque (22), Eventos: índigo (53)
    uint8_t bg_tone = (win_id == TuiState::WIN_INSPECTOR) ? 23 : ((win_id == TuiState::WIN_CONSOLE) ? 22 : 53);

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

struct GridRenderCtx {
    const EvalPreview& preview;
    std::string active_id;
    std::string preview_target_id;
    TX sel_cmin = 0, sel_rmin = 0, sel_cmax = 0, sel_rmax = 0;
    SpreadsheetEngine::StringVec inputs;
    SpreadsheetEngine::StringVec dependents;
};

inline std::string renderGridCell(TuiState& st, const GridRenderCtx& ctx, TX col_idx, TX row_idx) {
    std::string cid = CellCoord(col_idx, row_idx, true).toId();
    SpreadsheetCell cell = st.engine.getCell(cid);

    bool is_cursor      = (cid == ctx.active_id);
    bool is_target_cell = st.point_mode && (cid == ctx.preview_target_id);
    bool in_sel         = (st.hasCellSel() || st.hasFillSel()) &&
                          (col_idx >= ctx.sel_cmin && col_idx <= ctx.sel_cmax &&
                           row_idx >= ctx.sel_rmin && row_idx <= ctx.sel_rmax);

    std::string val_text = cell.displayValue();
    bool is_text_disp = cell.result.is_text;
    if ((is_target_cell || (!st.point_mode && is_cursor)) && ctx.preview.active && ctx.preview.valid) {
        val_text = ctx.preview.result.asString();
        is_text_disp = ctx.preview.result.is_text;
    }

    bool is_bool_true = false, is_bool_false = false;
    if (!cell.has_error && is_text_disp) {
        std::string upper_val = texcel_detail::toUpper(val_text);
        if (upper_val == "TRUE")       { val_text = "■"; is_bool_true = true; }
        else if (upper_val == "FALSE") { val_text = "□"; is_bool_false = true; }
    }

    std::string formatted = alignCell(val_text, TuiState::CELL_WIDTH, !cell.has_error && !is_text_disp);
    bool is_fill_tip = st.hasFillSel() && (col_idx == st.sel_b.col && row_idx == st.sel_b.row);
    if (is_fill_tip && formatted.size() >= 2) {
        bool left = (st.sel_b.col < st.sel_a.col);
        bool up   = (st.sel_b.row < st.sel_a.row);
        std::string ramp = up ? (left ? "◤" : "◥") : (left ? "◣" : "◢");
        formatted = left ? (ramp + formatted.substr(1)) : (formatted.substr(0, formatted.size() - 1) + ramp);
    }

    if (is_cursor)                                     return styled(formatted).badge_cyan();
    if ((st.hasFillSel() && in_sel) || is_target_cell) return styled(formatted).badge_green();
    if (st.hasCellSel() && in_sel)                     return styled(formatted).badge_blue();
    if (texcel_detail::containsStr(ctx.inputs, cid))   return styled(formatted).badge_yellow();
    if (texcel_detail::containsStr(ctx.dependents, cid)) return styled(formatted).bright_magenta().bold().underline();
    if (cell.has_error)                                return styled(formatted).bright_red().bold();
    if (is_bool_true || cell.is_formula)               return styled(formatted).bright_green().bold();
    if (is_bool_false)                                 return styled(formatted).dim();
    if (!val_text.empty())                             return styled(formatted).bright_white();
    return styled(alignCell("·", TuiState::CELL_WIDTH, false)).dim();
}

inline void renderGridWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = (st.mode == SpreadsheetFocusMode::GridNavigation);
    drawWindowFrame(st, TuiState::LEFT_X, TuiState::TOP_Y, st.left_w, TuiState::TOP_H, "Hoja de Calculo", is_focused);

    std::string active_id = st.cursor.toId();
    std::string preview_target_id = active_id;
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        preview_target_id = preview.target_cell;
        if (!st.point_mode) active_id = preview_target_id;
    }

    GridRenderCtx ctx{
        preview,
        active_id,
        preview_target_id,
        0, 0, 0, 0,
        preview.active ? preview.referenced_cells : st.engine.getCellInputs(active_id),
        st.engine.getCellDependents(active_id)
    };
    const size_t inner_w = static_cast<size_t>(st.left_w - 4);

    bool any_rect_sel = st.hasCellSel() || st.hasFillSel();
    if (any_rect_sel) {
        normalizeCellRange(st.sel_a.col, st.sel_a.row, st.sel_b.col, st.sel_b.row,
                           ctx.sel_cmin, ctx.sel_rmin, ctx.sel_cmax, ctx.sel_rmax);
    }
    bool has_multi_sel = any_rect_sel && (ctx.sel_cmin != ctx.sel_cmax || ctx.sel_rmin != ctx.sel_rmax);

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
            bool col_in_sel = st.hasCellSel() && (col_idx >= ctx.sel_cmin && col_idx <= ctx.sel_cmax);
            if (col_in_sel)                    hdr_ss << styled(hdr).badge_blue();
            else if (col_idx == st.cursor.col) hdr_ss << styled(hdr).bright_cyan().bold();
            else                               hdr_ss << styled(hdr).white().bold();
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
        bool row_in_sel = st.hasCellSel() && (row_idx >= ctx.sel_rmin && row_idx <= ctx.sel_rmax);
        if (row_in_sel)                    row_ss << styled(row_lbl.str()).badge_blue();
        else if (row_idx == st.cursor.row) row_ss << styled(row_lbl.str()).bright_cyan().bold();
        else                               row_ss << styled(row_lbl.str()).gray();

        for (TX c = 0; c < st.visible_cols; ++c) {
            row_ss << styled("│").gray() << renderGridCell(st, ctx, st.scroll_col + c, row_idx);
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::GRID_Y0 + r);
        st.ros() << fitWidth(row_ss.str(), inner_w);
    }

    gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::GRID_Y0 + TuiState::VISIBLE_ROWS);
    st.ros() << std::string(inner_w, ' ');

    gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + TuiState::TOP_H - 2);
    std::ostringstream leg;
    leg << styled(" ■ ").badge_cyan() << styled("Activa ").gray()
        << styled(" ■ ").badge_yellow() << styled("Referencia ").gray()
        << styled(" ■ ").bright_magenta().bold() << styled("Afectada ").gray()
        << styled(" ■ ").bright_green().bold() << styled("Formula").gray();
    st.ros() << fitWidth(leg.str(), inner_w);

    // Mini-scrollbars de la Matriz (vertical y horizontal)
    drawVerticalScrollbar(st, TuiState::LEFT_X + st.left_w - 1, TuiState::GRID_Y0,
                          TuiState::VISIBLE_ROWS, st.scroll_row, CellCoord::MAX_ROWS - TuiState::VISIBLE_ROWS);

    TX h_x0 = TuiState::GRID_X0 + 1;
    TX h_len = (TuiState::LEFT_X + st.left_w - 2) - h_x0 + 1;
    if (h_len >= 10) {
        drawHorizontalScrollbar(st, h_x0, TuiState::TOP_Y + TuiState::TOP_H - 1,
                                h_len, std::min<TX>(9, h_len / 2),
                                st.scroll_col, CellCoord::MAX_COLS - st.visible_cols, is_focused);
    }
}

/** @brief Proyecta el AST de una expresión hacia un `VisualGraph` coloreado para el Inspector. */
inline VisualGraph buildAstVisualGraph(const std::string& raw_expr, bool horizontal) {
    std::string expr = texcel_detail::trim(raw_expr);
    if (expr.empty()) return {};
    if (expr[0] == '=') expr = texcel_detail::trim(expr.substr(1));
    if (expr.empty() || expr[0] == '"') return {};

    SpreadsheetEngine::StringVec refs;
    SpreadsheetEngine::AstNode* ast = SpreadsheetEngine::tryParseAst(expr, refs);
    if (!ast) return {};

    struct AstTreeView {
        SpreadsheetEngine::AstNode* m_root;
        SpreadsheetEngine::AstNode* getRoot() const { return m_root; }
    };

    auto fmt = [](const AstToken& tok, TreeRole role) -> std::string {
        if (role == TreeRole::Root) return styled("[" + tok.text + "]").bright_yellow().bold().str();
        if (role == TreeRole::Leaf) return styled(tok.text).bright_cyan().str();
        return styled(tok.text).bright_green().bold().str();
    };

    VisualGraph vg = horizontal
        ? HorizontalTreeLayout<decltype(fmt)>(fmt, true)(AstTreeView{ast})
        : VerticalTreeLayout<decltype(fmt)>(fmt, 3, false)(AstTreeView{ast});
    texcel_detail::freeAst(ast);
    return vg;
}

inline void renderInspectorWindow(TuiState& st, const EvalPreview& preview) {
    std::string inspect_id = st.cursor.toId();
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
        ? (preview.valid ? preview.result.asString() : preview.error_msg)
        : (cell.raw_expression.empty() ? "(vacia)" : cell.displayValue());
    std::string rpn_str   = showing_preview ? preview.postorder_rpn : cell.postorder_rpn;

    const size_t inner_w = static_cast<size_t>(st.right_w - 4);
    const size_t val_max = (inner_w > 14) ? (inner_w - 13) : 20;

    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(st, TuiState::WIN_INSPECTOR, st.right_x + 2, TuiState::TOP_Y + rel_y, content, inner_w);
    };

    bool is_fx = cell.is_formula || (showing_preview && !preview.result.is_text && !raw_fx.empty() && raw_fx[0] == '=');
    bool is_txt = showing_preview ? preview.result.is_text : cell.result.is_text;
    std::string kind_badge = raw_fx.empty()
        ? styled("Celda vacia").dim().str()
        : (is_fx
            ? styled("Formula / Expresion").bright_green().bold().str()
            : (is_txt ? styled("Texto").bright_cyan().str() : styled("Valor Literal").bright_cyan().str()));
    putLine(1, styled("Celda      : ").gray().str() + styled(inspect_id).bright_yellow().bold().str() + " (" + kind_badge + ")");

    std::string fx_display = "∅";
    if (!raw_fx.empty()) {
        std::string prefix = (raw_fx[0] == '=') ? "" : "= ";
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
        ? buildAstVisualGraph(raw_fx, st.ast_horizontal)
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
        drawVerticalScrollbar(st, st.right_x + st.right_w - 1, TuiState::AST_Y0,
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

inline std::string renderCommandInput(TuiState& st, const std::string& active_id, bool is_focused, size_t text_slot) {
    const size_t max_in = (text_slot > 1) ? (text_slot - 1) : text_slot;
    st.input_cursor = std::min(st.input_cursor, st.input_buffer.size());
    std::string cursor_block = is_focused ? styled("█").bright_green().blink().str() : "";

    if (st.input_buffer.empty()) {
        if (is_focused) return cursor_block + styled("Ej: = A1 + 4  o  5  o  Ctrl+U limpiar").dim().str();
        std::string cell_preview = st.formatCellForInput(active_id);
        if (!cell_preview.empty()) return styled(clipPlain(cell_preview, max_in)).dim().str();
        return styled("[Enter] o [=] para editar " + active_id).dim().str();
    }
    if (!is_focused) {
        return styled(clipPlain(st.input_buffer, max_in)).bright_white().bold().str();
    }

    size_t view_start = (st.input_cursor > max_in) ? (st.input_cursor - max_in) : 0;
    std::string vis = st.input_buffer.substr(view_start, max_in);
    size_t rel_cur = st.input_cursor - view_start;
    if (rel_cur >= vis.size()) {
        return styled(vis).bright_white().bold().str() + cursor_block;
    }
    return styled(vis.substr(0, rel_cur)).bright_white().bold().str() +
           styled(std::string(1, vis[rel_cur])).badge_green().str() +
           styled(vis.substr(rel_cur + 1)).bright_white().bold().str();
}

inline std::string renderCommandStatus(TuiState& st, const EvalPreview& preview, const std::string& active_id, size_t inner_w) {
    if (st.point_mode) {
        std::string pinfo = preview.valid
            ? ("= " + preview.result.asString() + "  (Point Mode: operador +-*/ o [Enter])")
            : ("Point Mode [" + active_id + "]");
        return styled(clipPlain(pinfo, inner_w)).dim().str();
    }
    if (preview.active) {
        return (TuiState::isFormulaBuffer(st.input_buffer) && preview.valid)
            ? styled(clipPlain("= " + preview.result.asString(), inner_w)).dim().str()
            : "";
    }
    SpreadsheetCell cur_cell = st.engine.getCell(active_id);
    return (cur_cell.is_formula && !cur_cell.has_error)
        ? styled(clipPlain("= " + cur_cell.displayValue(), inner_w)).dim().str()
        : "";
}

inline void renderCommandWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = (st.mode == SpreadsheetFocusMode::CommandConsole);
    drawWindowFrame(st, TuiState::LEFT_X, TuiState::BOT_Y, st.left_w, TuiState::BOT_H,
                    "Consola & Editor de Formulas",
                    is_focused);

    const size_t inner_w = static_cast<size_t>(st.left_w - 4);
    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(st, TuiState::WIN_CONSOLE, TuiState::LEFT_X + 2, TuiState::BOT_Y + rel_y, content, inner_w);
    };

    std::string active_id = st.activeCellId();
    const size_t box_inner = (inner_w > 2) ? (inner_w - 2) : 20;
    std::string prompt_label = active_id + " ❯ ";
    const size_t prefix_len = visible_length(prompt_label);
    const size_t text_slot = (box_inner > prefix_len + 1) ? (box_inner - prefix_len - 1) : 12;

    std::string prompt_prefix = is_focused
        ? styled(prompt_label).bright_green().bold().str()
        : styled(prompt_label).cyan().str();
    std::string input_rendered = renderCommandInput(st, active_id, is_focused, text_slot);

    std::string h_bar = repeatGlyph("─", box_inner);
    putLine(1, styled("╭" + h_bar + "╮").gray().str());
    putLine(2, styled("│ ").gray().str() + prompt_prefix + fitWidth(input_rendered, text_slot) + styled("│").gray().str());
    putLine(3, styled("╰" + h_bar + "╯").gray().str());
    putLine(4, renderCommandStatus(st, preview, active_id, inner_w));
    putLine(5, "");
    putLine(6, styled("Ejemplos: ").gray().bold().str() +
               styled("5 │ = A1 + 4 │ = A1 + B1 │ [Tab] Point Mode").dim().str());
    putLine(7, styled("Comandos: ").gray().bold().str() +
               styled("SUM / CONCAT / COUNTIF(S) / & │ clear │ save │ load │ quit").dim().str());
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
            drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, abs_y, styled_line, inner_w);
        } else {
            drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, abs_y, "", inner_w);
        }
    }

    std::string footer = "Total ops: " + std::to_string(total) + " │ Ciclos: protegidos";
    drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, TuiState::BOT_Y + TuiState::BOT_H - 2, styled(footer).dim().str(), inner_w);
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
    putRow(1, st.modal_idx == TuiState::MODAL_AUTOSAVE,
           "[A] Autoguardado al editar : " + (st.modal_idx == TuiState::MODAL_AUTOSAVE ? std::string(st.config.autosave ? "[ON]" : "[OFF]") : auto_txt));
    putRow(2, st.modal_idx == TuiState::MODAL_SAVE,
           "[S] Guardar hoja actual (" + st.config.current_sheet + TexcelConfig::SHEET_EXT + ")");

    if (st.modal_new_mode) {
        putRow(3, true, "Nueva hoja (Enter): " + st.modal_new_name + "█");
    } else {
        putRow(3, st.modal_idx == TuiState::MODAL_NEW, "[N] Crear nueva hoja limpia...");
    }

    gotoxy(st.ros(), mx + 2, my + 4);
    st.ros() << fitWidth(styled(makeSectionHeader("Hojas guardadas en output/sheets/", inner_w)).gray().str(), inner_w);

    const TX list_rows = modal_h - 8;
    TX file_cursor = (st.modal_idx >= TuiState::MODAL_FIRST_FILE) ? (st.modal_idx - TuiState::MODAL_FIRST_FILE) : 0;
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
                putRow(5 + i, st.modal_idx == TuiState::MODAL_FIRST_FILE + idx, fname + TexcelConfig::SHEET_EXT + tag);
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
        Ansi::clearAndSaveOrigin(frame_buf);
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
    Ansi::flushSynchronizedFrame(st.os, frame_buf.str());
}

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_TUI_RENDERER_H__
