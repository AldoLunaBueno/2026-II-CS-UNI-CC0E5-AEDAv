/**
 * @file renderer.h
 * @ingroup texcel
 * @brief Renderizador 2D de las cuatro ventanas, barra superior y modales flotantes de Texcel.
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
#include <mutex>
#include <sstream>
#include <string>

namespace texcel_tui {

inline void drawVerticalScrollbar(TuiState& st, TX x, TX y, TX h, TX scroll_pos, TX max_scroll) {
    if (h <= 0) return;
    TX thumb_h = std::min<TX>(2, h);
    TX thumb = (scroll_pos * (h - thumb_h)) / std::max<TX>(1, max_scroll);
    for (TX r = 0; r < h; ++r) {
        gotoxy(st.ros(), x, y + r);
        st.ros() << ((r >= thumb && r < thumb + thumb_h) ? styled("█").bright_cyan().bold() : styled("│").dim());
    }
}

inline void drawHorizontalScrollbar(TuiState& st, TX x0, TX y, TX len, TX thumb_w, TX scroll_pos, TX max_scroll, bool active_border = false) {
    if (len <= 0) return;
    TX eff_thumb_w = std::min(thumb_w, len);
    TX thumb = (scroll_pos * (len - eff_thumb_w)) / std::max<TX>(1, max_scroll);
    gotoxy(st.ros(), x0, y);
    for (TX i = 0; i < len; ++i) {
        if (i >= thumb && i < thumb + eff_thumb_w) st.ros() << styled("▬").bright_cyan().bold();
        else st.ros() << (active_border ? styled("─").bright_cyan().bold() : styled("─").gray());
    }
}

inline void drawWindowLineWithSelection(TuiState& st, TX win_id, TX start_x, TX abs_y,
                                        const std::string& styled_line, size_t inner_w) {
    std::string plain = stripAnsi(styled_line);
    if (abs_y >= 0 && win_id >= 1 && win_id <= 4 && static_cast<size_t>(abs_y) < st.win_lines[win_id].size()) {
        st.win_lines[win_id][static_cast<size_t>(abs_y)] = plain;
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

    std::string padded = fitWidth(plain, inner_w);
    TX c_start = (abs_y == sy0) ? std::max<TX>(0, sx0 - start_x) : 0;
    TX c_end   = (abs_y == sy1) ? std::min<TX>(static_cast<TX>(inner_w) - 1, sx1 - start_x) : static_cast<TX>(inner_w) - 1;

    gotoxy(st.ros(), start_x, abs_y);
    if (c_start > c_end || c_start >= static_cast<TX>(inner_w)) {
        st.ros() << fitWidth(styled_line, inner_w);
        return;
    }

    uint8_t bg_tone = (win_id == TuiState::WIN_INSPECTOR) ? 23 : ((win_id == TuiState::WIN_CONSOLE) ? 22 : 53);

    std::string before = utf8Slice(padded, 0, static_cast<size_t>(c_start));
    std::string after  = utf8Slice(padded, static_cast<size_t>(c_end) + 1, inner_w);

    std::ostringstream mid_ss;
    for (size_t cp = static_cast<size_t>(c_start); cp <= static_cast<size_t>(c_end); ++cp) {
        std::string g = utf8At(padded, cp);
        if (isDividerGlyph(g)) {
            mid_ss << styled(g).gray();
        } else {
            mid_ss << styled(g.empty() ? " " : g).bright_white().bg_color8(bg_tone).bold();
        }
    }
    st.ros() << before << mid_ss.str() << after;
}

/**
 * @brief Dibuja el marco de una ventana con título y limpia sus márgenes internos laterales.
 */
inline void drawWindowFrame(TuiState& st, TX x, TX y, TX w, TX h,
                            const std::string& title,
                            bool active_focus,
                            bool accent_secondary = false,
                            bool clear_interior = false) {
    if (w < 4 || h < 2) return;
    auto styleBorder = [&](const std::string& glyph) {
        if (active_focus) return styled(glyph).bright_cyan().bold().str();
        if (accent_secondary) return styled(glyph).cyan().str();
        return styled(glyph).gray().str();
    };

    size_t inner_w = static_cast<size_t>(w - 2);
    std::string full_title = " " + title + " ";
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
        std::string left_edge  = styleBorder("│") + " ";
        std::string right_edge = " " + styleBorder("│");
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(st.ros(), x, row);
            st.ros() << left_edge;
            gotoxy(st.ros(), x + w - 2, row);
            st.ros() << right_edge;
        }
    }

    gotoxy(st.ros(), x, y + h - 1);
    st.ros() << styleBorder("╰" + repeatGlyph("─", inner_w) + "╯");
}

inline void renderHeaderBar(TuiState& st, const EvalPreview&) {
    gotoxy(st.ros(), 0, 0);
    std::string mode_badge;
    if (st.modal_kind == ModalKind::Sheets) {
        mode_badge = styled(fitWidth(" HOJAS ", 11)).badge_cyan();
    } else if (st.modal_kind == ModalKind::Functions) {
        mode_badge = styled(fitWidth(" FUNCIONES ", 11)).badge_cyan();
    } else if (st.modal_kind == ModalKind::Help) {
        mode_badge = styled(fitWidth(" AYUDA ", 11)).badge_cyan();
    } else if (st.search.active) {
        mode_badge = styled(fitWidth(" BUSCAR ", 11)).badge_yellow();
    } else if (st.point_mode) {
        mode_badge = styled(fitWidth(" APUNTAR ", 11)).badge_yellow();
    } else if (st.mode == SpreadsheetFocusMode::GridNavigation) {
        mode_badge = styled(fitWidth(" HOJA ", 11)).badge_cyan();
    } else {
        mode_badge = styled(fitWidth(" EDITAR ", 11)).badge_green();
    }

    std::string auto_badge = st.config.autosave
        ? styled(fitWidth(" Auto:ON ", 10)).badge_green().str()
        : styled(fitWidth(" Auto:OFF ", 10)).gray().str();

    std::string sheet_txt = st.config.current_sheet + TexcelConfig::SHEET_EXT;
    std::string dirty_dot = (st.dirty_unsaved && !st.config.autosave)
        ? styled(" ●").bright_red().bold().str()
        : "";

    std::ostringstream oss;
    oss << styled(" ◆ Texcel ").badge_blue()
        << " " << mode_badge
        << styled(" │ ").gray() << styled(sheet_txt).bright_yellow().bold() << dirty_dot
        << " " << auto_badge;

    std::string left_part = oss.str();
    if (st.total_width >= 64) {
        std::string right_part = styled("[?]").bright_cyan().str() + " " + styled("[Salir]").gray().str();
        size_t lw = visible_length(left_part);
        size_t rw = visible_length(right_part);
        size_t tw = static_cast<size_t>(st.total_width);
        if (lw + rw + 1 <= tw) {
            st.ros() << left_part << std::string(tw - lw - rw, ' ') << right_part;
            return;
        }
    }
    st.ros() << fitWidth(left_part, static_cast<size_t>(st.total_width));
}

struct GridLegendFlags {
    bool active  = false;
    bool sel     = false;
    bool fill    = false;
    bool target  = false;
    bool ref     = false;
    bool dep     = false;
    bool formula = false;
    bool err     = false;
    bool search  = false;
};

struct GridRenderCtx {
    const EvalPreview& preview;
    std::string active_id;
    std::string preview_target_id;
    const SpreadsheetCell* active_cell_ptr = nullptr;
    TX sel_cmin = 0, sel_rmin = 0, sel_cmax = 0, sel_rmax = 0;
    SpreadsheetEngine::StringVec dependents;
    GridLegendFlags legend{};
};

inline bool isSearchMatchCoord(const TuiState& st, TX col, TX row) {
    if (!st.search.active || st.search.matches.empty()) return false;
    for (const auto& m : st.search.matches) {
        if (m.col == col && m.row == row) return true;
    }
    return false;
}

inline std::string renderGridCell(TuiState& st, GridRenderCtx& ctx, TX col_idx, TX row_idx, TX cell_w) {
    std::string cid = CellCoord(col_idx, row_idx, true).toId();
    const SpreadsheetCell* cell_ptr = st.engine.findCellAt(col_idx, row_idx);

    bool is_cursor      = (cid == ctx.active_id);
    bool is_target_cell = st.point_mode && (cid == ctx.preview_target_id);
    bool in_sel         = (st.hasCellSel() || st.hasFillSel()) &&
                          (col_idx >= ctx.sel_cmin && col_idx <= ctx.sel_cmax &&
                           row_idx >= ctx.sel_rmin && row_idx <= ctx.sel_rmax);
    bool in_cut         = st.cell_clip.active && st.cell_clip.is_cut &&
                          col_idx >= st.cell_clip.src_c0 && col_idx < st.cell_clip.src_c0 + st.cell_clip.cols &&
                          row_idx >= st.cell_clip.src_r0 && row_idx < st.cell_clip.src_r0 + st.cell_clip.rows;

    std::string val_text = cell_ptr ? cell_ptr->displayValue() : "";
    bool has_error       = cell_ptr ? cell_ptr->has_error : false;
    bool is_formula      = cell_ptr ? cell_ptr->is_formula : false;
    bool is_text_disp    = cell_ptr ? cell_ptr->result.is_text : false;

    if ((is_target_cell || (!st.point_mode && is_cursor)) && ctx.preview.active && ctx.preview.valid) {
        val_text = ctx.preview.result.asString();
        is_text_disp = ctx.preview.result.is_text;
        has_error = false;
    }

    bool is_bool_true = false, is_bool_false = false;
    if (!has_error && is_text_disp) {
        if (texcel_detail::equalsIgnoreCase(val_text, "TRUE"))       { val_text = "■"; is_bool_true = true; }
        else if (texcel_detail::equalsIgnoreCase(val_text, "FALSE")) { val_text = "□"; is_bool_false = true; }
    }

    size_t cw = static_cast<size_t>(std::max<TX>(4, cell_w));
    std::string formatted = alignCell(val_text, cw, !has_error && !is_text_disp);
    bool is_fill_tip = st.hasFillSel() && (col_idx == st.sel_b.col && row_idx == st.sel_b.row);
    if (is_fill_tip && formatted.size() >= 2) {
        bool left = (st.sel_b.col < st.sel_a.col);
        bool up   = (st.sel_b.row < st.sel_a.row);
        std::string ramp = up ? (left ? "◤" : "◥") : (left ? "◣" : "◢");
        formatted = left
            ? (ramp + utf8Slice(formatted, 1, cw))
            : (utf8Slice(formatted, 0, cw - 1) + ramp);
    }

    bool is_ref = ctx.preview.active
        ? ctx.preview.referencesCoord(col_idx, row_idx, cid)
        : (ctx.active_cell_ptr && ctx.active_cell_ptr->referencesCoord(col_idx, row_idx, cid));

    if (is_cursor)                                       { ctx.legend.active = true;  return styled(formatted).badge_cyan(); }
    if (st.hasFillSel() && in_sel)                       { ctx.legend.fill = true;    return styled(formatted).badge_green(); }
    if (is_target_cell)                                  { ctx.legend.target = true;  return styled(formatted).badge_green(); }
    if (st.hasCellSel() && in_sel)                       { ctx.legend.sel = true;     return styled(formatted).badge_blue(); }
    if (isSearchMatchCoord(st, col_idx, row_idx))        { ctx.legend.search = true;  return styled(formatted).badge_yellow(); }
    if (is_ref)                                          { ctx.legend.ref = true;     return styled(formatted).badge_yellow(); }
    if (texcel_detail::containsStr(ctx.dependents, cid)) { ctx.legend.dep = true;     return styled(formatted).bright_magenta().bold().underline(); }
    if (in_cut)                                          return styled(formatted).cyan().dim().italic();
    if (has_error)                                       { ctx.legend.err = true;     return styled(formatted).bright_red().bold(); }
    if (is_bool_true || is_formula)                      { ctx.legend.formula = true; return styled(formatted).bright_green().bold(); }
    if (is_bool_false)                                   return styled(formatted).dim();
    if (!val_text.empty())                               return styled(formatted).bright_white();
    return styled(alignCell("·", cw, false)).dim();
}

inline void renderGridWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = !st.isModalOpen() && (st.mode == SpreadsheetFocusMode::GridNavigation) && !st.search.active;
    drawWindowFrame(st, TuiState::LEFT_X, TuiState::TOP_Y, st.left_w, st.top_h, "Hoja", is_focused);

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
        st.engine.findCell(active_id),
        0, 0, 0, 0,
        st.engine.getCellDependents(active_id)
    };
    const size_t inner_w = static_cast<size_t>(std::max<TX>(4, st.left_w - 4));

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
        for (TX c = 0; c < st.visible_cols && st.scroll_col + c < CellCoord::MAX_COLS; ++c) {
            TX col_idx = st.scroll_col + c;
            size_t cw = static_cast<size_t>(st.colWidth(col_idx));
            std::string hdr = alignCell(CellCoord::colToLabel(col_idx), cw, false);
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
        for (TX c = 0; c < st.visible_cols && st.scroll_col + c < CellCoord::MAX_COLS; ++c) {
            TX cw = st.colWidth(st.scroll_col + c);
            sep_line += (c == 0) ? "┼" : "┬";
            sep_line += repeatGlyph("─", static_cast<size_t>(cw));
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + 2);
        st.ros() << fitWidth(styled(sep_line).gray().str(), inner_w);
    }

    for (TX r = 0; r < st.visible_rows; ++r) {
        TX row_idx = st.scroll_row + r;
        std::ostringstream row_ss;

        std::ostringstream row_lbl;
        row_lbl << std::setw(4) << (row_idx + 1) << "    ";
        bool row_in_sel = st.hasCellSel() && (row_idx >= ctx.sel_rmin && row_idx <= ctx.sel_rmax);
        if (row_in_sel)                    row_ss << styled(row_lbl.str()).badge_blue();
        else if (row_idx == st.cursor.row) row_ss << styled(row_lbl.str()).bright_cyan().bold();
        else                               row_ss << styled(row_lbl.str()).gray();

        for (TX c = 0; c < st.visible_cols && st.scroll_col + c < CellCoord::MAX_COLS; ++c) {
            TX col_idx = st.scroll_col + c;
            row_ss << styled("│").gray() << renderGridCell(st, ctx, col_idx, row_idx, st.colWidth(col_idx));
        }
        gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::GRID_Y0 + r);
        st.ros() << fitWidth(row_ss.str(), inner_w);
    }

    // Fila de Quick Stats instantáneas cuando hay selección múltiple (o limpia si no hay selección)
    TX stats_y = TuiState::GRID_Y0 + st.visible_rows;
    if (stats_y < TuiState::TOP_Y + st.top_h - 2) {
        gotoxy(st.ros(), TuiState::LEFT_X + 2, stats_y);
        RangeQuickStats qs = st.computeSelectionStats();
        if (qs.has_cells) {
            std::ostringstream qss;
            qss << "Σ ";
            if (qs.num_count > 0) {
                qss << "Suma: " << formatCellNumber(qs.sum)
                    << " │ Prom: " << formatCellNumber(qs.sum / qs.num_count)
                    << " │ Mín: " << formatCellNumber(qs.min_val)
                    << " │ Máx: " << formatCellNumber(qs.max_val) << " │ ";
            }
            qss << "Conteo: " << qs.non_empty;
            st.ros() << fitWidth(styled(clipPlain(qss.str(), inner_w)).bright_cyan().str(), inner_w);
        } else {
            st.ros() << std::string(inner_w, ' ');
        }
    }

    gotoxy(st.ros(), TuiState::LEFT_X + 2, TuiState::TOP_Y + st.top_h - 2);
    std::ostringstream leg;
    if (ctx.legend.active)  leg << styled(" ■ ").badge_cyan() << styled("Activa ").gray();
    if (ctx.legend.sel)     leg << styled(" ■ ").badge_blue() << styled("Selección ").gray();
    if (ctx.legend.fill)    leg << styled(" ■ ").badge_green() << styled("Relleno ").gray();
    if (ctx.legend.target)  leg << styled(" ■ ").badge_green() << styled("Destino ").gray();
    if (ctx.legend.search)  leg << styled(" ■ ").badge_yellow() << styled("Coincidencia ").gray();
    if (ctx.legend.ref)     leg << styled(" ■ ").badge_yellow() << styled("Referencia ").gray();
    if (ctx.legend.dep)     leg << styled(" ■ ").bright_magenta().bold() << styled("Afectada ").gray();
    if (ctx.legend.formula) leg << styled(" ■ ").bright_green().bold() << styled("Fórmula ").gray();
    if (ctx.legend.err)     leg << styled(" ■ ").bright_red().bold() << styled("Error ").gray();
    st.ros() << fitWidth(leg.str(), inner_w);

    drawVerticalScrollbar(st, TuiState::LEFT_X + st.left_w - 1, TuiState::GRID_Y0,
                          st.visible_rows, st.scroll_row, CellCoord::MAX_ROWS - st.visible_rows);

    TX h_x0 = TuiState::GRID_X0 + 1;
    TX h_len = (TuiState::LEFT_X + st.left_w - 2) - h_x0 + 1;
    if (h_len >= 10) {
        drawHorizontalScrollbar(st, h_x0, TuiState::TOP_Y + st.top_h - 1,
                                h_len, std::min<TX>(9, h_len / 2),
                                st.scroll_col, CellCoord::MAX_COLS - st.visible_cols, is_focused);
    }
}

inline VisualGraph buildAstVisualGraph(const std::string& raw_expr, bool horizontal) {
    std::string expr = texcel_detail::trim(raw_expr);
    if (expr.empty()) return {};
    if (expr[0] == '=') expr = texcel_detail::trim(std::string_view(expr).substr(1));
    if (expr.empty() || expr[0] == '"') return {};

    // Solo se requiere el árbol sintáctico; no se recolectan dependencias
    SpreadsheetEngine::AstNode* ast = SpreadsheetEngine::tryParseAst(expr, nullptr, nullptr, nullptr);
    if (!ast) return {};

    struct AstTreeView {
        SpreadsheetEngine::AstNode* m_root;
        SpreadsheetEngine::AstNode* getRoot() const { return m_root; }
    };

    auto fmt = [](const SpreadsheetEngine::AstNode& node, TreeRole role) -> std::string {
        const AstToken& tok = node.getValue();
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

inline std::string formatDependenciesBounded(TuiState& st,
                                             const Vector<VectorAscTraits<std::string>>& points,
                                             const Vector<VectorAscTraits<CellRangeRect>>* ranges,
                                             size_t max_cols) {
    if (points.size() == 0 && (!ranges || ranges->size() == 0)) return "ninguna";
    std::string out;
    bool first = true;

    if (ranges) {
        for (size_t i = 0; i < ranges->size(); ++i) {
            if (!first) out += ", ";
            out += (*ranges)[i].getValue().toId();
            first = false;
            if (visible_length(out) > max_cols + 4) return clipPlain(out, max_cols);
        }
    }
    for (size_t i = 0; i < points.size(); ++i) {
        if (!first) out += ", ";
        const std::string& cid = points[i].getValue();
        const SpreadsheetCell* p = st.engine.findCell(cid);
        std::string dv = p ? p->displayValue() : "";
        out += cid + "(" + dv + ")";
        first = false;
        if (visible_length(out) > max_cols + 4) return clipPlain(out, max_cols);
    }
    return clipPlain(out, max_cols);
}

inline void renderInspectorWindow(TuiState& st, const EvalPreview& preview) {
    std::string inspect_id = st.cursor.toId();
    if (preview.active && CellCoord::parse(preview.target_cell).valid) {
        inspect_id = preview.target_cell;
    }

    drawWindowFrame(st, st.right_x, TuiState::TOP_Y, st.right_w, st.top_h,
                    "Inspector [" + inspect_id + "]",
                    false, true);

    const SpreadsheetCell* cell_ptr = st.engine.findCell(inspect_id);
    bool showing_preview = preview.active && (preview.target_cell == inspect_id);

    std::string raw_fx = showing_preview
        ? preview.expression
        : (cell_ptr ? cell_ptr->raw_expression : "");
    bool val_is_err = showing_preview
        ? !preview.valid
        : (cell_ptr && cell_ptr->has_error);
    std::string val_str = showing_preview
        ? (preview.valid ? preview.result.asString() : preview.error_msg)
        : ((!cell_ptr || cell_ptr->raw_expression.empty()) ? "∅" : cell_ptr->displayValue());
    std::string rpn_str = showing_preview
        ? preview.postorder_rpn
        : (cell_ptr ? cell_ptr->postorder_rpn : "");

    const size_t inner_w = static_cast<size_t>(std::max<TX>(4, st.right_w - 4));
    const size_t val_max = (inner_w > 14) ? (inner_w - 13) : 12;

    auto putLine = [&](TX rel_y, const std::string& content) {
        drawWindowLineWithSelection(st, TuiState::WIN_INSPECTOR, st.right_x + 2, TuiState::TOP_Y + rel_y, content, inner_w);
    };

    bool is_fx  = (cell_ptr && cell_ptr->is_formula) || (showing_preview && !raw_fx.empty() && raw_fx[0] == '=');
    bool is_txt = showing_preview ? preview.result.is_text : (cell_ptr && cell_ptr->result.is_text);
    std::string kind_badge = raw_fx.empty()
        ? styled("Vacía").dim().str()
        : (is_fx
            ? styled("Fórmula").bright_green().bold().str()
            : (is_txt ? styled("Texto").bright_cyan().str() : styled("Número").bright_cyan().str()));
    putLine(1, styled("Tipo       : ").gray().str() + kind_badge);

    std::string fx_display = "∅";
    if (!raw_fx.empty()) {
        std::string prefix = (raw_fx[0] == '=') ? "" : "= ";
        fx_display = (is_txt && !is_fx)
            ? clipPlain(raw_fx, val_max)
            : (prefix + clipPlain(raw_fx, val_max > 2 ? val_max - 2 : val_max));
    }
    putLine(2, styled("Contenido  : ").gray().str() + styled(fx_display).bright_cyan().bold().str());

    std::string styled_val = val_is_err
        ? styled(clipPlain(val_str, val_max)).bright_red().bold().str()
        : styled(clipPlain(val_str, val_max)).bright_white().bold().str();
    putLine(3, styled("Valor      : ").gray().str() + styled_val);

    Vector<VectorAscTraits<std::string>> empty_pts;
    std::string in_fmt = showing_preview
        ? formatDependenciesBounded(st, preview.referenced_cells, &preview.referenced_ranges, val_max)
        : (cell_ptr ? formatDependenciesBounded(st, cell_ptr->inputs, &cell_ptr->range_inputs, val_max) : "ninguna");
    auto deps = st.engine.getCellDependents(inspect_id);
    std::string dep_fmt = formatDependenciesBounded(st, deps, nullptr, val_max);

    putLine(4, styled("Depende de : ").gray().str() + styled(in_fmt).bright_yellow().str());
    putLine(5, styled("Afecta a   : ").gray().str() + styled(dep_fmt).bright_magenta().str());

    std::string ast_hdr_prefix = "─── AST  ";
    std::string v_radio = st.ast_horizontal
        ? styled("□ V").dim().str()
        : styled("■ V").bright_cyan().bold().str();
    std::string h_radio = st.ast_horizontal
        ? styled("■ H").bright_cyan().bold().str()
        : styled("□ H").dim().str();
    size_t hdr_used = 9 + 3 + 2 + 3 + 1;
    std::string ast_hdr_tail = (inner_w > hdr_used) ? repeatGlyph("─", inner_w - hdr_used) : "";
    putLine(6, styled(ast_hdr_prefix).gray().str() +
               v_radio + "  " + h_radio +
               styled(" " + ast_hdr_tail).gray().str());

    std::string rpn_disp = rpn_str.empty() ? "∅" : clipPlain(rpn_str, inner_w > 11 ? inner_w - 11 : 12);
    putLine(7, styled("PostOrder: ").gray().str() + styled(rpn_disp).bright_green().str());

    bool should_show_ast = (!is_txt || is_fx) && !raw_fx.empty();
    if (!should_show_ast) {
        st.ast_graph_w = 0;
        st.ast_graph_h = 0;
        for (TX row = 0; row < st.ast_view_h; ++row) putLine(8 + row, "");
        if (st.ast_view_h >= 2) putLine(9, styled("          (sin AST)").dim().str());
    } else {
        if (!st.ast_cache.valid || st.ast_cache.expr != raw_fx || st.ast_cache.horizontal != st.ast_horizontal) {
            st.ast_cache.graph = buildAstVisualGraph(raw_fx, st.ast_horizontal);
            st.ast_cache.expr = raw_fx;
            st.ast_cache.horizontal = st.ast_horizontal;
            st.ast_cache.valid = true;
        }
        const VisualGraph& vg = st.ast_cache.graph;
        st.ast_graph_w = vg.width;
        st.ast_graph_h = vg.height;
        st.clampAstScroll();

        if (vg.nodes.size() == 0 || vg.width <= 0 || vg.height <= 0) {
            for (TX row = 0; row < st.ast_view_h; ++row) putLine(8 + row, "");
            if (st.ast_view_h >= 2) putLine(9, styled("          (sin AST)").dim().str());
        } else {
            ClippedGraphRenderer ast_renderer(
                st.ros(), static_cast<TX>(inner_w), st.ast_view_h,
                st.ast_scroll_x, st.ast_scroll_y, 0, 0, true, ConsoleLineStyle::Rounded);
            std::vector<std::string> ast_lines = ast_renderer.renderToLines(vg);
            for (TX row = 0; row < st.ast_view_h; ++row) {
                const std::string line = (row < static_cast<TX>(ast_lines.size()))
                    ? ast_lines[static_cast<size_t>(row)]
                    : std::string();
                putLine(8 + row, line);
            }
        }

        if (vg.height > st.ast_view_h) {
            drawVerticalScrollbar(st, st.right_x + st.right_w - 1, TuiState::AST_Y0,
                                  st.ast_view_h, st.ast_scroll_y, vg.height - st.ast_view_h);
        }
        if (vg.width > static_cast<TX>(inner_w)) {
            TX h_len = (st.right_x + st.right_w - 3) - (st.right_x + 2) + 1;
            if (h_len >= 6) {
                drawHorizontalScrollbar(st, st.right_x + 2, TuiState::TOP_Y + st.top_h - 1,
                                        h_len, std::min<TX>(9, std::max<TX>(3, h_len / 3)),
                                        st.ast_scroll_x, vg.width - static_cast<TX>(inner_w));
            }
        }
    }
}

inline std::string renderCommandInput(TuiState& st, const std::string& active_id, bool is_focused, size_t text_slot) {
    const size_t max_in = (text_slot > 1) ? (text_slot - 1) : text_slot;
    if (st.search.active) {
        std::string q = clipPlain(st.search.query, max_in);
        return st.search.query.empty()
            ? (styled("█").bright_yellow().blink().str() + styled("Texto o valor a buscar...").dim().str())
            : (styled(q).bright_white().bold().str() + styled("█").bright_yellow().blink().str());
    }

    st.input_cursor = std::min(st.input_cursor, st.input_buffer.size());
    std::string cursor_block = is_focused ? styled("█").bright_green().blink().str() : "";

    if (st.input_buffer.empty()) {
        if (is_focused) return cursor_block + styled("Valor, =fórmula o [fx]").dim().str();
        std::string cell_preview = st.formatCellForInput(active_id);
        if (!cell_preview.empty()) {
            std::string clipped = clipPlain(cell_preview, max_in);
            return (cell_preview[0] == '=')
                ? styled(clipped).bright_cyan().str()
                : styled(clipped).white().str();
        }
        return styled("Valor o =fórmula").dim().str();
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

/** @brief Construye un *calltip* con el argumento actual resaltado (ej. `COUNTIFS(r1, ▸c1, ...)`). */
inline std::string formatHighlightedCalltip(const TexcelFuncInfo& fn, size_t arg_idx, size_t max_w) {
    std::string_view syn(fn.syntax);
    size_t lp = syn.find('(');
    size_t rp = syn.rfind(')');
    if (lp == std::string_view::npos || rp == std::string_view::npos || rp <= lp) {
        return styled(clipPlain(std::string(fn.syntax), max_w)).bright_yellow().str();
    }
    std::string head(syn.substr(0, lp + 1));
    std::string_view params = syn.substr(lp + 1, rp - lp - 1);
    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos < params.size()) {
        size_t c = params.find(',', pos);
        std::string_view item = (c == std::string_view::npos) ? params.substr(pos) : params.substr(pos, c - pos);
        parts.push_back(texcel_detail::trim(item));
        if (c == std::string_view::npos) break;
        pos = c + 1;
    }

    size_t highlight_i = parts.empty() ? 0 : std::min(arg_idx, parts.size() - 1);
    std::ostringstream oss;
    oss << styled("ƒ " + head).cyan();
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << styled(", ").gray();
        if (i == highlight_i) oss << styled("▸" + parts[i]).bright_yellow().bold();
        else oss << styled(parts[i]).gray();
    }
    oss << styled(") — ").cyan() << styled(fn.summary).dim();
    return fitWidth(oss.str(), max_w);
}

inline std::string renderCommandStatus(TuiState& st, const EvalPreview& preview, const std::string& active_id, size_t inner_w) {
    if (st.search.active) {
        if (st.search.query.empty()) return styled(clipPlain("Escribe para filtrar celdas por valor o fórmula.", inner_w)).dim().str();
        if (st.search.matches.empty()) return styled(clipPlain("⚠ Sin coincidencias para \"" + st.search.query + "\".", inner_w)).bright_yellow().str();
        std::string m_info = "✔ Coincidencia " + std::to_string(st.search.match_idx + 1) + "/" +
                             std::to_string(st.search.matches.size()) + " en " + st.cursor.toId();
        return styled(clipPlain(m_info, inner_w)).bright_green().str();
    }

    size_t call_arg = 0;
    const TexcelFuncInfo* active_fn = (st.mode == SpreadsheetFocusMode::CommandConsole || st.point_mode)
        ? FormulaParser::detectActiveCalltip(st.input_buffer, st.input_cursor, call_arg)
        : nullptr;

    if (st.point_mode) {
        std::string pointed = st.point_has_range ? st.selectedRangeStr(true) : st.cursor.toId();
        if (preview.valid) {
            return styled(clipPlain("= " + preview.result.asString() + "  │  Apuntando [" + pointed + "] (Shift+Flecha rango)", inner_w)).bright_cyan().str();
        }
        if (active_fn) return formatHighlightedCalltip(*active_fn, call_arg, inner_w);
        return styled(clipPlain("Apuntando [" + pointed + "]  │  Shift+Flecha selecciona rango", inner_w)).cyan().str();
    }

    if (preview.active) {
        if (TuiState::isFormulaBuffer(st.input_buffer)) {
            if (preview.valid) {
                std::string res_txt = "= " + preview.result.asString();
                if (active_fn && inner_w > visible_length(res_txt) + 12) {
                    return styled(res_txt + "  │  ").bright_green().bold().str() +
                           formatHighlightedCalltip(*active_fn, call_arg, inner_w - visible_length(res_txt) - 5);
                }
                return styled(clipPlain(res_txt, inner_w)).bright_green().bold().str();
            }
            // Mostrar diagnóstico de error en vivo (y calltip si está dentro de una función incompleta)
            if (active_fn && preview.error_msg.find("Se esperaba") != std::string::npos) {
                return formatHighlightedCalltip(*active_fn, call_arg, inner_w);
            }
            return styled(clipPlain("⚠ " + preview.error_msg, inner_w)).bright_red().str();
        }
        return "";
    }

    const SpreadsheetCell* cur_cell = st.engine.findCell(active_id);
    if (cur_cell && cur_cell->is_formula) {
        return cur_cell->has_error
            ? styled(clipPlain("⚠ Error en celda: " + cur_cell->error_msg, inner_w)).bright_red().str()
            : styled(clipPlain("= " + cur_cell->displayValue(), inner_w)).dim().str();
    }
    return "";
}

inline void renderCommandWindow(TuiState& st, const EvalPreview& preview) {
    bool is_focused = !st.isModalOpen() && (st.mode == SpreadsheetFocusMode::CommandConsole || st.search.active);
    drawWindowFrame(st, TuiState::LEFT_X, st.bot_y, st.left_w, st.bot_h,
                    st.search.active ? "Buscar" : "Fórmula",
                    is_focused);

    const size_t inner_w = static_cast<size_t>(std::max<TX>(6, st.left_w - 4));
    auto putLine = [&](TX rel_y, const std::string& content) {
        if (rel_y < st.bot_h - 1) {
            drawWindowLineWithSelection(st, TuiState::WIN_CONSOLE, TuiState::LEFT_X + 2, st.bot_y + rel_y, content, inner_w);
        }
    };

    std::string active_id = st.activeCellId();
    const size_t box_inner = (inner_w > 2) ? (inner_w - 2) : 10;
    std::string prompt_label = st.search.active ? "Buscar ❯ " : (active_id + " ❯ ");
    const size_t prefix_len = visible_length(prompt_label);
    constexpr size_t fx_btn_cols = 5; // " [fx]"
    const size_t text_slot = (box_inner > prefix_len + fx_btn_cols + 2)
        ? (box_inner - prefix_len - fx_btn_cols - 2)
        : 8;

    std::string prompt_prefix = st.search.active
        ? styled(prompt_label).bright_yellow().bold().str()
        : (is_focused ? styled(prompt_label).bright_green().bold().str() : styled(prompt_label).cyan().str());
    std::string input_rendered = renderCommandInput(st, active_id, is_focused, text_slot);
    std::string fx_pill = is_focused
        ? styled("[fx]").bright_cyan().bold().str()
        : styled("[fx]").cyan().str();

    std::string h_bar = repeatGlyph("─", box_inner);
    putLine(1, styled("╭" + h_bar + "╮").gray().str());
    putLine(2, styled("│ ").gray().str() + prompt_prefix + fitWidth(input_rendered, text_slot) + " " + fx_pill + styled(" │").gray().str());
    putLine(3, styled("╰" + h_bar + "╯").gray().str());
    putLine(4, renderCommandStatus(st, preview, active_id, inner_w));

    std::string banner = clipPlain(st.status_banner, inner_w);
    std::string styled_banner = banner.starts_with("✔")
        ? styled(banner).bright_green().str()
        : (banner.starts_with("⚠") || banner.find("Error") != std::string::npos || banner.find("inválid") != std::string::npos || banner.find("invalid") != std::string::npos)
            ? styled(banner).bright_yellow().str()
            : styled(banner).cyan().str();
    putLine(5, styled_banner);

    for (TX ry = 6; ry < st.bot_h - 3; ++ry) {
        putLine(ry, "");
    }

    TX shortcut_ry = std::max<TX>(6, st.bot_h - 3);
    if (st.chord_x) {
        std::string chord_line =
            styled(" Ctrl+X ❯ ").bright_yellow().bold().str() +
            styled("[F] fx  [H] Hojas  [S] Guardar  [G] Ir  [B] Buscar  [=] AutoAncho  [A] Ayuda").cyan().str();
        putLine(shortcut_ry, fitWidth(chord_line, inner_w));
    } else {
        std::string shortcuts;
        if (st.search.active) {
            shortcuts = "[Enter/↓] Siguiente  [↑] Anterior  [Ctrl+U] Limpiar  [Esc] Cerrar";
        } else if (st.point_mode) {
            shortcuts = "[↑↓←→] Celda  [Shift+Flecha] Rango  [+-*/,] Operador  [Enter] Aplicar  [Esc] Volver";
        } else if (st.mode == SpreadsheetFocusMode::CommandConsole) {
            shortcuts = "[Enter] Aplicar  [Tab] Apuntar  [Ctrl+F] fx  [Ctrl+U] Limpiar  [Esc] Cancelar";
        } else {
            shortcuts = "[Ctrl+X] Comandos  [F2] Editar  [Ctrl+B] Buscar  [Ctrl+Z/Y] Undo  [Alt+←/→] Col";
        }
        putLine(shortcut_ry, styled(clipPlain(shortcuts, inner_w)).dim().str());
    }
    for (TX ry = shortcut_ry + 1; ry < st.bot_h - 1; ++ry) {
        putLine(ry, "");
    }
}

inline void renderActivityWindow(TuiState& st) {
    drawWindowFrame(st, st.right_x, st.bot_y, st.right_w, st.bot_h,
                    "Actividad",
                    false, true);

    const size_t inner_w = static_cast<size_t>(std::max<TX>(4, st.right_w - 4));
    const auto& logs = st.engine.getLogs();
    const size_t max_lines = static_cast<size_t>(std::max<TX>(1, st.bot_h - 3));
    size_t total = logs.size();
    size_t start_idx = (total > max_lines) ? (total - max_lines) : 0;

    for (size_t i = 0; i < max_lines; ++i) {
        TX abs_y = st.bot_y + 1 + static_cast<TX>(i);
        if (start_idx + i < total) {
            std::string line = logs[start_idx + i].getValue();
            std::string clipped = clipPlain(line, inner_w);
            std::string styled_line;
            if (line.starts_with("✔")) styled_line = styled(clipped).bright_green().str();
            else if (line.starts_with("⚠") || line.starts_with("✖")) styled_line = styled(clipped).bright_red().str();
            else if (line.starts_with("↶") || line.starts_with("↷")) styled_line = styled(clipped).bright_cyan().str();
            else styled_line = styled(clipped).white().str();
            drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, abs_y, styled_line, inner_w);
        } else {
            drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, abs_y, "", inner_w);
        }
    }

    std::string footer = std::to_string(total) + (total == 1 ? " evento" : " eventos");
    if (!st.undo_mgr.undo_stack.empty()) {
        footer += " │ Undo:" + std::to_string(st.undo_mgr.undo_stack.size());
    }
    drawWindowLineWithSelection(st, TuiState::WIN_ACTIVITY, st.right_x + 2, st.bot_y + st.bot_h - 2,
                                styled(clipPlain(footer, inner_w)).dim().str(), inner_w);
}

inline void renderFileManagerModal(TuiState& st) {
    ModalRect mr = st.modalRect();
    const size_t inner_w = static_cast<size_t>(mr.inner_w);

    drawWindowFrame(st, mr.x, mr.y, mr.w, mr.h, "Hojas", true, false, true);

    auto putRow = [&](TX rel_y, bool selected, const std::string& content) {
        gotoxy(st.ros(), mr.x + 2, mr.y + rel_y);
        std::string line = (selected ? " ▸ " : "   ") + content;
        if (selected) st.ros() << styled(fitWidth(line, inner_w)).badge_cyan();
        else st.ros() << fitWidth(line, inner_w);
    };

    std::string auto_txt = st.config.autosave
        ? styled("[ON] ").bright_green().bold().str()
        : styled("[OFF]").gray().str();
    putRow(1, st.modal_idx == TuiState::MODAL_AUTOSAVE,
           "[A] Autoguardado : " + (st.modal_idx == TuiState::MODAL_AUTOSAVE ? std::string(st.config.autosave ? "[ON]" : "[OFF]") : auto_txt));
    putRow(2, st.modal_idx == TuiState::MODAL_SAVE,
           "[S] Guardar (" + st.config.current_sheet + TexcelConfig::SHEET_EXT + (st.dirty_unsaved ? " ●" : "") + ")");

    if (st.modal_new_mode) {
        putRow(3, true, "Nueva hoja (Enter): " + st.modal_new_name + "█");
    } else {
        putRow(3, st.modal_idx == TuiState::MODAL_NEW, "[N] Nueva hoja...");
    }

    gotoxy(st.ros(), mr.x + 2, mr.y + 4);
    st.ros() << fitWidth(styled(makeSectionHeader("Guardadas", inner_w)).gray().str(), inner_w);

    const TX list_rows = mr.h - 8;
    TX file_cursor = (st.modal_idx >= TuiState::MODAL_FIRST_FILE) ? (st.modal_idx - TuiState::MODAL_FIRST_FILE) : 0;
    TX scroll_f = (file_cursor >= list_rows) ? (file_cursor - list_rows + 1) : 0;

    if (st.config.sheet_files.size() == 0) {
        gotoxy(st.ros(), mr.x + 2, mr.y + 6);
        st.ros() << fitWidth(styled("   (Sin hojas guardadas. Pulsa [S] o [N])").dim().str(), inner_w);
    } else {
        for (TX i = 0; i < list_rows; ++i) {
            TX idx = scroll_f + i;
            if (idx < static_cast<TX>(st.config.sheet_files.size())) {
                std::string fname = st.config.sheet_files[static_cast<size_t>(idx)].getValue();
                std::string tag = (fname == st.config.current_sheet) ? " ★" : "";
                putRow(5 + i, st.modal_idx == TuiState::MODAL_FIRST_FILE + idx, fname + TexcelConfig::SHEET_EXT + tag);
            } else {
                gotoxy(st.ros(), mr.x + 2, mr.y + 5 + i);
                st.ros() << std::string(inner_w, ' ');
            }
        }
    }

    gotoxy(st.ros(), mr.x + 2, mr.y + mr.h - 2);
    std::string help = st.modal_new_mode
        ? "Nombre y [Enter] │ [Esc] Cancelar"
        : "[↑/↓] Mover │ [Enter] OK │ [Supr] Borrar │ [Esc] Cerrar";
    st.ros() << fitWidth(styled(clipPlain(help, inner_w)).dim().str(), inner_w);
}

inline void renderFxHelpModal(TuiState& st) {
    ModalRect mr = st.modalRect();
    const size_t inner_w = static_cast<size_t>(mr.inner_w);
    bool is_fx = (st.modal_kind == ModalKind::Functions);

    drawWindowFrame(st, mr.x, mr.y, mr.w, mr.h, is_fx ? "Funciones [fx]" : "Ayuda [?]", true, false, true);

    gotoxy(st.ros(), mr.x + 2, mr.y + 1);
    std::string tab_fx   = is_fx ? styled(" Funciones (fx) ").badge_cyan().str() : styled(" Funciones (fx) ").gray().str();
    std::string tab_help = is_fx ? styled(" Ayuda (?) ").gray().str()            : styled(" Ayuda (?) ").badge_cyan().str();
    std::string right_hint;
    if (is_fx && !st.modal_filter.empty()) {
        right_hint = "  " + styled("Filtro: ").gray().str() + styled(st.modal_filter + "█").bright_yellow().bold().str();
    } else if (is_fx) {
        right_hint = styled("  [Escribe para filtrar · Tab Ayuda]").dim().str();
    } else {
        right_hint = styled("  [Tab] cambiar").dim().str();
    }
    st.ros() << fitWidth(tab_fx + "  " + tab_help + right_hint, inner_w);

    gotoxy(st.ros(), mr.x + 2, mr.y + 2);
    st.ros() << styled(repeatGlyph("─", inner_w)).gray();

    if (is_fx) {
        st.clampFxScroll();
        auto filtered = st.filteredFunctionIndices();
        TX fcnt = static_cast<TX>(filtered.size());

        for (TX i = 0; i < TuiState::FX_LIST_ROWS; ++i) {
            TX idx = st.fx_scroll + i;
            gotoxy(st.ros(), mr.x + 2, mr.y + 3 + i);
            if (idx < fcnt) {
                const FunctionSpec& fn = FUNCTION_CATALOG[filtered[static_cast<size_t>(idx)]];
                bool selected = (idx == st.fx_idx);
                std::string name_col = fitWidth(fn.name, 10);
                std::string row_txt = (selected ? " ▸ " : "   ") + name_col + " " + fn.summary;
                if (selected) st.ros() << styled(fitWidth(row_txt, inner_w)).badge_cyan();
                else st.ros() << styled(fitWidth(row_txt, inner_w)).white();
            } else if (fcnt == 0 && i == 1) {
                st.ros() << fitWidth(styled("   Sin funciones que coincidan con \"" + st.modal_filter + "\".").dim().str(), inner_w);
            } else {
                st.ros() << std::string(inner_w, ' ');
            }
        }
        if (fcnt > TuiState::FX_LIST_ROWS) {
            drawVerticalScrollbar(st, mr.x + mr.w - 1, mr.y + 3, TuiState::FX_LIST_ROWS,
                                  st.fx_scroll, fcnt - TuiState::FX_LIST_ROWS);
        }

        gotoxy(st.ros(), mr.x + 2, mr.y + 9);
        st.ros() << styled(repeatGlyph("─", inner_w)).gray();
        if (fcnt > 0) {
            const FunctionSpec& cur = FUNCTION_CATALOG[filtered[static_cast<size_t>(st.fx_idx)]];
            gotoxy(st.ros(), mr.x + 2, mr.y + 10);
            st.ros() << fitWidth(styled(clipPlain(cur.syntax, inner_w)).bright_yellow().bold().str(), inner_w);
            gotoxy(st.ros(), mr.x + 2, mr.y + 11);
            st.ros() << fitWidth(styled(clipPlain(cur.detail, inner_w)).white().str(), inner_w);
            gotoxy(st.ros(), mr.x + 2, mr.y + 12);
            st.ros() << fitWidth(styled("Ej: ").gray().str() + styled(clipPlain(cur.example, inner_w > 4 ? inner_w - 4 : inner_w)).bright_cyan().str(), inner_w);
        }

        gotoxy(st.ros(), mr.x + 2, mr.y + mr.h - 2);
        st.ros() << fitWidth(styled(clipPlain("[↑/↓] Mover │ [A-Z] Filtrar │ [Enter/Clic] Insertar │ [Esc] Cerrar", inner_w)).dim().str(), inner_w);
    } else {
        st.clampHelpScroll();
        for (TX i = 0; i < TuiState::HELP_LIST_ROWS; ++i) {
            TX idx = st.help_scroll + i;
            gotoxy(st.ros(), mr.x + 2, mr.y + 3 + i);
            if (idx < HELP_CATALOG_SIZE) {
                const HelpEntry& e = HELP_CATALOG[idx];
                if (e.is_section) {
                    st.ros() << fitWidth(styled(makeSectionHeader(e.key, inner_w)).bright_cyan().bold().str(), inner_w);
                } else {
                    std::string k = styled(fitWidth(e.key, 15)).bright_yellow().bold().str();
                    std::string d = styled(clipPlain(e.desc, inner_w > 17 ? inner_w - 17 : inner_w)).white().str();
                    st.ros() << fitWidth(" " + k + " " + d, inner_w);
                }
            } else {
                st.ros() << std::string(inner_w, ' ');
            }
        }
        if (HELP_CATALOG_SIZE > TuiState::HELP_LIST_ROWS) {
            drawVerticalScrollbar(st, mr.x + mr.w - 1, mr.y + 3, TuiState::HELP_LIST_ROWS,
                                  st.help_scroll, HELP_CATALOG_SIZE - TuiState::HELP_LIST_ROWS);
        }

        gotoxy(st.ros(), mr.x + 2, mr.y + mr.h - 2);
        st.ros() << fitWidth(styled(clipPlain("[↑/↓] Desplazar │ [Tab] Funciones │ [Esc] Cerrar", inner_w)).dim().str(), inner_w);
    }
}

inline void renderAll(TuiState& st, bool force_clear = false) {
    bool resized = st.updateLayout();
    if (st.was_modal_open) {
        force_clear = true;
        st.was_modal_open = false;
    }
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

    // Limpia la columna separadora central entre los paneles izquierdo y derecho
    if (!st.isModalOpen()) {
        for (TX y = TuiState::TOP_Y; y < st.bot_y + st.bot_h; ++y) {
            gotoxy(st.ros(), st.left_w, y);
            st.ros() << ' ';
        }
        if (st.bot_y + st.bot_h < st.total_height) {
            gotoxy(st.ros(), 0, st.bot_y + st.bot_h);
            st.ros() << std::string(static_cast<size_t>(st.total_width), ' ');
        }
    }

    if (st.modal_kind == ModalKind::Sheets) {
        renderFileManagerModal(st);
    } else if (st.isFxHelpModal()) {
        renderFxHelpModal(st);
    }

    TX end_y = st.interactive_alt_screen
        ? std::min<TX>(st.total_height - 1, std::max<TX>(0, st.term_rows - 1))
        : st.total_height;
    gotoxy(st.ros(), 0, end_y);

    st.render_os = &st.os;
    std::lock_guard<std::mutex> out_lk(st.output_mtx);
    Ansi::flushSynchronizedFrame(st.os, frame_buf.str());
}

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_RENDERER_H__
