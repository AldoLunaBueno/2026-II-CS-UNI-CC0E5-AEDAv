/**
 * @file spreadsheet_tui.h
 * @ingroup visualization
 * @brief Interfaz multi-ventana en consola (estilo Claude Code / Codex TUI) para la hoja de cálculo.
 *
 * Renderiza 4 ventanas independientes sincronizadas en tiempo real mediante `gotoxy`:
 * 1. Ventana Matriz (Navegación interactiva con resaltado de referencias e inputs).
 * 2. Ventana Inspector & AST (`BinaryTree` PostOrder, fórmula `fx` guardada detrás y grafo de dependencias).
 * 3. Ventana Consola de Comandos / Fórmulas (prompt `❯` con evaluación preview en vivo en cada tecla).
 * 4. Ventana Historial & Propagación Reactiva.
 */

#ifndef __SPREADSHEET_TUI_H__
#define __SPREADSHEET_TUI_H__

#include "../containers/spreadsheet.h"
#include "gotoxy.h"
#include "ansi_style.h"
#include "ansi_metrics.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>

/**
 * @enum SpreadsheetFocusMode
 * @brief Ventana activa en la interfaz dual: Navegación en Matriz o Consola de Comandos.
 */
enum class SpreadsheetFocusMode {
    GridNavigation = 0, /**< Foco en la Ventana 1 (Matriz): movimiento por flechas/WASD, Enter para editar. */
    CommandConsole = 1  /**< Foco en la Ventana 3 (Consola REPL): escritura libre `A1 = 5`, `B1 = A1 + 4` con preview. */
};

/**
 * @class SpreadsheetTUI
 * @brief Gestor visual de 4 ventanas independientes en consola usando `gotoxy` y `ansi_style.h`.
 */
class SpreadsheetTUI {
public:
    static constexpr TX TOTAL_WIDTH  = 98;
    static constexpr TX TOTAL_HEIGHT = 28;

    // Geometría de las 4 ventanas independientes
    static constexpr TX LEFT_X  = 0;
    static constexpr TX LEFT_W  = 56;
    static constexpr TX RIGHT_X = 57;
    static constexpr TX RIGHT_W = 41;

    static constexpr TX TOP_Y   = 1;
    static constexpr TX TOP_H   = 16; // filas 1..16
    static constexpr TX BOT_Y   = 17;
    static constexpr TX BOT_H   = 10; // filas 17..26

    static constexpr TX VISIBLE_COLS = 5;  // A..E por vista (con scroll hasta Z)
    static constexpr TX VISIBLE_ROWS = 11; // 1..11 por vista (con scroll hasta 99)
    static constexpr TX CELL_WIDTH   = 9;

private:
    SpreadsheetEngine&   m_engine;
    std::ostream&        m_os;
    SpreadsheetFocusMode m_mode = SpreadsheetFocusMode::GridNavigation;

    TX m_cursor_col = 0; // 0 = A
    TX m_cursor_row = 0; // 0 = fila 1
    TX m_scroll_col = 0;
    TX m_scroll_row = 0;

    std::string m_input_buffer;
    std::string m_status_banner = "Listo. Usa [Tab] para alternar entre Matriz y Consola de Comandos.";
    Vector<VectorAscTraits<std::string>> m_cmd_history;
    TX m_history_idx = -1;

    /** @brief Ajusta o rellena un string (con o sin ANSI) para que ocupe exactamente `width` columnas visibles. */
    static std::string fitWidth(const std::string& s, size_t width) {
        size_t vlen = visible_length(s);
        if (vlen < width) {
            return s + std::string(width - vlen, ' ');
        }
        return s;
    }

    /** @brief Trunca texto plano a `max_len` caracteres ASCII/UTF-8 simples. */
    static std::string clipPlain(const std::string& s, size_t max_len) {
        if (s.size() <= max_len) return s;
        if (max_len <= 2) return s.substr(0, max_len);
        return s.substr(0, max_len - 1) + "~";
    }

    /** @brief Centra o alinea texto plano en una celda de ancho visual `w`. */
    static std::string alignCell(const std::string& text, size_t w, bool right_align) {
        std::string clipped = clipPlain(text, w - 2);
        size_t vlen = visible_length(clipped);
        size_t pad = (w - 2 > vlen) ? (w - 2 - vlen) : 0;
        if (right_align) {
            return " " + std::string(pad, ' ') + clipped + " ";
        }
        size_t left_pad = pad / 2;
        size_t right_pad = pad - left_pad;
        return " " + std::string(left_pad, ' ') + clipped + std::string(right_pad, ' ') + " ";
    }

    static bool VecContains(const Vector<VectorAscTraits<std::string>>& v, const std::string& key) {
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i].getValue() == key) return true;
        }
        return false;
    }

    /** @brief Dibuja el marco de una ventana independiente con título y color según tenga foco. */
    void drawWindowFrame(TX x, TX y, TX w, TX h,
                         const std::string& title,
                         bool active_focus,
                         bool accent_secondary = false) {
        auto styleBorder = [&](const std::string& glyph) {
            if (active_focus) {
                return styled(glyph).bright_cyan().bold().str();
            }
            if (accent_secondary) {
                return styled(glyph).cyan().str();
            }
            return styled(glyph).gray().str();
        };

        std::string badge = active_focus ? " ● ACTIVA " : " ○ ";
        std::string full_title = " " + title + badge;
        size_t title_vlen = visible_length(full_title);
        size_t inner_w = (w >= 2) ? static_cast<size_t>(w - 2) : 0;

        // Borde superior
        gotoxy(m_os, x, y);
        m_os << styleBorder("╭─");
        std::string styled_title = active_focus
            ? styled(full_title).pure_black().bg_bright_cyan().bold().str()
            : (accent_secondary
                ? styled(full_title).bright_white().bold().str()
                : styled(full_title).gray().str());
        m_os << styled_title;

        size_t used = 1 + title_vlen;
        for (size_t i = used; i < inner_w; ++i) m_os << styleBorder("─");
        m_os << styleBorder("╮");

        // Laterales y limpieza interior
        std::string blank_inner(inner_w, ' ');
        for (TX row = y + 1; row < y + h - 1; ++row) {
            gotoxy(m_os, x, row);
            m_os << styleBorder("│") << blank_inner << styleBorder("│");
        }

        // Borde inferior
        gotoxy(m_os, x, y + h - 1);
        m_os << styleBorder("╰");
        for (size_t i = 0; i < inner_w; ++i) m_os << styleBorder("─");
        m_os << styleBorder("╯");
    }

    void ensureCursorVisible() {
        if (m_cursor_col < m_scroll_col) m_scroll_col = m_cursor_col;
        if (m_cursor_col >= m_scroll_col + VISIBLE_COLS) m_scroll_col = m_cursor_col - VISIBLE_COLS + 1;
        if (m_cursor_row < m_scroll_row) m_scroll_row = m_cursor_row;
        if (m_cursor_row >= m_scroll_row + VISIBLE_ROWS) m_scroll_row = m_cursor_row - VISIBLE_ROWS + 1;
    }

    /** @brief Barra superior global estilo Claude Code / Codex CLI. */
    void renderHeaderBar(const EvalPreview& preview) {
        gotoxy(m_os, 0, 0);
        std::string mode_badge = (m_mode == SpreadsheetFocusMode::GridNavigation)
            ? styled(" MODO 1: NAVEGACION MATRIZ ").pure_black().bg_bright_cyan().bold().str()
            : styled(" MODO 2: CONSOLA COMANDOS ").pure_black().bg_bright_green().bold().str();

        std::string focus_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
        if (preview.active && !preview.target_cell.empty()) {
            focus_id = preview.target_cell;
        }

        std::ostringstream oss;
        oss << styled(" ◆ AEDA Excel Codex TUI ").bright_white().bg_blue().bold()
            << " " << mode_badge
            << styled(" │ Celda: ").gray() << styled(focus_id).bright_yellow().bold()
            << styled(" │ [Tab] Cambiar Ventana │ [=] Formula │ [q] Salir").gray();

        m_os << fitWidth(oss.str(), TOTAL_WIDTH);
    }

    /** @brief Ventana 1 (Arriba-Izquierda): Matriz interactiva de celdas con resaltado reactivo. */
    void renderGridWindow(const EvalPreview& preview) {
        bool is_focused = (m_mode == SpreadsheetFocusMode::GridNavigation);
        drawWindowFrame(LEFT_X, TOP_Y, LEFT_W, TOP_H, "Ventana 1: Hoja de Calculo", is_focused);

        std::string active_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
        if (preview.active && CellCoord::parse(preview.target_cell).valid) {
            active_id = preview.target_cell;
        }

        auto inputs = preview.active ? preview.referenced_cells : m_engine.getCellInputs(active_id);
        auto dependents = m_engine.getCellDependents(active_id);

        // Encabezado de columnas (A, B, C, D, E...)
        gotoxy(m_os, LEFT_X + 2, TOP_Y + 1);
        m_os << styled("Fila ").dim();
        for (TX c = 0; c < VISIBLE_COLS; ++c) {
            TX col_idx = m_scroll_col + c;
            std::string col_label = CellCoord::colToLabel(col_idx);
            bool is_active_col = (col_idx == m_cursor_col);
            std::string hdr = alignCell(col_label, CELL_WIDTH, false);
            m_os << styled("│").gray();
            if (is_active_col) {
                m_os << styled(hdr).bright_cyan().bold();
            } else {
                m_os << styled(hdr).white().bold();
            }
        }

        // Separador horizontal
        gotoxy(m_os, LEFT_X + 2, TOP_Y + 2);
        m_os << styled("─────┼─────────────────────────────────────────────────").gray();

        // Filas de la matriz
        for (TX r = 0; r < VISIBLE_ROWS; ++r) {
            TX row_idx = m_scroll_row + r;
            gotoxy(m_os, LEFT_X + 2, TOP_Y + 3 + r);

            std::ostringstream row_lbl;
            row_lbl << std::setw(3) << (row_idx + 1) << "  ";
            if (row_idx == m_cursor_row) {
                m_os << styled(row_lbl.str()).bright_cyan().bold();
            } else {
                m_os << styled(row_lbl.str()).gray();
            }

            for (TX c = 0; c < VISIBLE_COLS; ++c) {
                TX col_idx = m_scroll_col + c;
                std::string cid = CellCoord(col_idx, row_idx, true).toId();
                SpreadsheetCell cell = m_engine.getCell(cid);

                bool is_cursor = (cid == active_id);
                bool is_input_ref = VecContains(inputs, cid);
                bool is_dependent = VecContains(dependents, cid);

                std::string val_text = cell.displayValue();
                if (is_cursor && preview.active && preview.valid) {
                    val_text = preview.formatted_value;
                }
                std::string formatted = alignCell(val_text, CELL_WIDTH, !cell.has_error);

                m_os << styled("│").gray();
                if (is_cursor) {
                    if (is_focused) {
                        m_os << styled(formatted).pure_black().bg_bright_cyan().bold();
                    } else {
                        m_os << styled(formatted).pure_black().bg_bright_green().bold();
                    }
                } else if (is_input_ref) {
                    // Celda referenciada por la fórmula actual
                    m_os << styled(formatted).pure_black().bg_bright_yellow().bold();
                } else if (is_dependent) {
                    // Celda que depende de la celda actual
                    m_os << styled(formatted).bright_magenta().bold().underline();
                } else if (cell.has_error) {
                    m_os << styled(formatted).bright_red().bold();
                } else if (cell.is_formula) {
                    m_os << styled(formatted).bright_green().bold();
                } else if (!val_text.empty()) {
                    m_os << styled(formatted).bright_white();
                } else {
                    m_os << styled(alignCell("·", CELL_WIDTH, false)).dim();
                }
            }
        }

        // Leyenda semántica al pie de la Ventana 1
        gotoxy(m_os, LEFT_X + 2, TOP_Y + TOP_H - 2);
        std::ostringstream leg;
        leg << styled(" ■ ").pure_black().bg_bright_cyan() << styled("Activa ").gray()
            << styled(" ■ ").pure_black().bg_bright_yellow() << styled("Input(fx) ").gray()
            << styled(" ■ ").bright_magenta().bold() << styled("Dependiente ").gray()
            << styled(" ■ ").bright_green().bold() << styled("Formula").gray();
        m_os << fitWidth(leg.str(), LEFT_W - 4);
    }

    /** @brief Ventana 2 (Arriba-Derecha): Inspector de Celda, Fórmula guardada detrás y Árbol AST (`BinaryTree`). */
    void renderInspectorWindow(const EvalPreview& preview) {
        std::string inspect_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
        if (preview.active && CellCoord::parse(preview.target_cell).valid) {
            inspect_id = preview.target_cell;
        }

        drawWindowFrame(RIGHT_X, TOP_Y, RIGHT_W, TOP_H,
                        "Ventana 2: Inspector & AST [" + inspect_id + "]",
                        false, true);

        SpreadsheetCell cell = m_engine.getCell(inspect_id);
        bool showing_preview = preview.active && (preview.target_cell == inspect_id);

        std::string raw_fx = showing_preview ? preview.expression : cell.raw_expression;
        std::string val_str = showing_preview
            ? (preview.valid ? preview.formatted_value : preview.error_msg)
            : (cell.raw_expression.empty() ? "(vacia)" : cell.displayValue());
        std::string rpn_str = showing_preview ? preview.postorder_rpn : cell.postorder_rpn;
        std::string root_lbl = showing_preview ? preview.ast_root : cell.ast_root_label;
        std::string left_lbl = showing_preview ? preview.ast_left : cell.ast_left_label;
        std::string right_lbl = showing_preview ? preview.ast_right : cell.ast_right_label;

        const size_t inner_w = RIGHT_W - 4;

        auto putLine = [&](TX rel_y, const std::string& content) {
            gotoxy(m_os, RIGHT_X + 2, TOP_Y + rel_y);
            m_os << fitWidth(content, inner_w);
        };

        // 1. Coordenada y tipo
        std::string kind_badge = raw_fx.empty()
            ? styled("Celda vacia").dim().str()
            : ((cell.is_formula || showing_preview)
                ? styled("Formula / Expresion").bright_green().bold().str()
                : styled("Valor Literal").bright_cyan().str());
        putLine(1, styled("Celda      : ").gray().str() + styled(inspect_id).bright_yellow().bold().str() + " (" + kind_badge + ")");

        // 2. Fórmula guardada detrás (requisito clave del profesor)
        std::string fx_display = raw_fx.empty() ? "∅" : ("= " + clipPlain(raw_fx, 22));
        putLine(2, styled("Detras (fx): ").gray().str() + styled(fx_display).bright_cyan().bold().str());

        // 3. Resultado calculado mostrado en pantalla
        putLine(3, styled("Resultado  : ").gray().str() + styled(clipPlain(val_str, 24)).bright_white().bold().str());

        // 4. Dependencias (Inputs y Dependientes)
        auto inputs = showing_preview ? preview.referenced_cells : m_engine.getCellInputs(inspect_id);
        auto deps   = m_engine.getCellDependents(inspect_id);

        std::ostringstream in_ss;
        if (inputs.size() == 0) in_ss << "ninguna";
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i > 0) in_ss << ", ";
            std::string ref_id = inputs[i].getValue();
            in_ss << ref_id << "(" << m_engine.getCell(ref_id).displayValue() << ")";
        }
        putLine(4, styled("Lee de     : ").gray().str() + styled(clipPlain(in_ss.str(), 24)).bright_yellow().str());

        std::ostringstream dep_ss;
        if (deps.size() == 0) dep_ss << "ninguna";
        for (size_t i = 0; i < deps.size(); ++i) {
            if (i > 0) dep_ss << ", ";
            std::string dep_id = deps[i].getValue();
            dep_ss << dep_id << "(" << m_engine.getCell(dep_id).displayValue() << ")";
        }
        putLine(5, styled("Afecta a   : ").gray().str() + styled(clipPlain(dep_ss.str(), 24)).bright_magenta().str());

        // Separador AST
        putLine(6, styled("─── BinaryTree AST (Euler PostOrder) ─").gray().str());

        // 5. Recorrido PostOrder RPN del BinaryTreeIterator
        std::string rpn_disp = rpn_str.empty() ? "∅" : clipPlain(rpn_str, 26);
        putLine(7, styled("PostOrder: ").gray().str() + styled(rpn_disp).bright_green().str());

        // 6. Visualización gráfica del árbol binario de la fórmula
        if (root_lbl.empty()) {
            putLine(9,  styled("       (sin arbol de expresion)").dim().str());
        } else if (left_lbl.empty() && right_lbl.empty()) {
            putLine(9,  "             " + styled("[" + clipPlain(root_lbl, 12) + "]").bright_cyan().bold().str());
            putLine(10, styled("          (Nodo hoja unico)").dim().str());
        } else {
            putLine(8,  "               " + styled("(" + clipPlain(root_lbl, 6) + ")").bright_yellow().bold().str());
            putLine(9,  styled("              ╭──┴──╮").gray().str());
            std::string l_box = "[" + clipPlain(left_lbl, 12) + "]";
            std::string r_box = "[" + clipPlain(right_lbl, 12) + "]";
            putLine(10, "        " + styled(fitWidth(l_box, 13)).bright_cyan().str() + " " + styled(r_box).bright_green().str());
        }

        // Pie del inspector: contenedor subyacente
        putLine(13, styled("Contenedores: HashTable<AVL> + BinaryTree").dim().str());
    }

    /** @brief Ventana 3 (Abajo-Izquierda): Consola interactiva de comandos y editor de fórmulas estilo Claude Code. */
    void renderCommandWindow(const EvalPreview& preview) {
        bool is_focused = (m_mode == SpreadsheetFocusMode::CommandConsole);
        drawWindowFrame(LEFT_X, BOT_Y, LEFT_W, BOT_H,
                        "Ventana 3: Consola & Editor (Claude Codex)",
                        is_focused);

        const size_t inner_w = LEFT_W - 4;
        auto putLine = [&](TX rel_y, const std::string& content) {
            gotoxy(m_os, LEFT_X + 2, BOT_Y + rel_y);
            m_os << fitWidth(content, inner_w);
        };

        std::string active_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();

        // Caja del prompt interactivo
        std::string prompt_prefix = is_focused
            ? styled("❯ ").bright_green().bold().str()
            : styled("❯ ").gray().str();

        std::string cursor_block = is_focused ? styled("█").bright_green().blink().str() : "";
        std::string shown_input = m_input_buffer.empty()
            ? (is_focused
                ? styled("Ej: " + active_id + " = 5  |  B1 = A1 + 4").dim().str()
                : styled("[Tab] o [Enter] para escribir formula").dim().str())
            : styled(clipPlain(m_input_buffer, 45)).bright_white().bold().str();

        putLine(1, styled("╭──────────────────────────────────────────────────╮").gray().str());
        if (m_input_buffer.empty()) {
            putLine(2, styled("│ ").gray().str() + prompt_prefix + fitWidth(cursor_block + shown_input, 47) + styled("│").gray().str());
        } else {
            putLine(2, styled("│ ").gray().str() + prompt_prefix + fitWidth(shown_input + cursor_block, 47) + styled("│").gray().str());
        }
        putLine(3, styled("╰──────────────────────────────────────────────────╯").gray().str());

        // Línea de Preview Reactivo en Vivo (evalúa antes de presionar Enter)
        if (preview.active) {
            if (preview.valid) {
                std::string pmsg = "⚡ Preview en vivo: " + preview.target_cell + " = " + preview.formatted_value + " (AST OK)";
                putLine(4, styled(clipPlain(pmsg, inner_w)).bright_green().bold().str());
            } else {
                std::string emsg = "⏳ Preview [" + preview.target_cell + "]: " + preview.error_msg;
                putLine(4, styled(clipPlain(emsg, inner_w)).bright_yellow().str());
            }
        } else {
            putLine(4, styled(clipPlain("ℹ " + m_status_banner, inner_w)).cyan().str());
        }

        // Guía rápida de comandos en la parte inferior de la ventana 3
        putLine(6, styled("Comandos: ").gray().bold().str() +
                   styled("A1 = 5").bright_white().str() + styled(" │ ").gray().str() +
                   styled("B1 = A1 + 4").bright_white().str() + styled(" │ ").gray().str() +
                   styled("D4 = A1 + B1").bright_white().str());
        putLine(7, styled("Extras  : ").gray().bold().str() +
                   styled("SUM(A1:D4)").bright_cyan().str() + styled(" │ ").gray().str() +
                   styled("demo").bright_yellow().str() + styled(" │ ").gray().str() +
                   styled("clear").bright_yellow().str() + styled(" │ ").gray().str() +
                   styled("save / load").bright_yellow().str());
    }

    /** @brief Ventana 4 (Abajo-Derecha): Historial de eventos y propagación reactiva en tiempo real. */
    void renderActivityWindow() {
        drawWindowFrame(RIGHT_X, BOT_Y, RIGHT_W, BOT_H,
                        "Ventana 4: Eventos & Propagacion",
                        false, true);

        const size_t inner_w = RIGHT_W - 4;
        const auto& logs = m_engine.getLogs();
        const size_t max_lines = BOT_H - 3; // 7 líneas visibles
        size_t total = logs.size();
        size_t start_idx = (total > max_lines) ? (total - max_lines) : 0;

        for (size_t i = 0; i < max_lines; ++i) {
            gotoxy(m_os, RIGHT_X + 2, BOT_Y + 1 + static_cast<TX>(i));
            if (start_idx + i < total) {
                std::string line = logs[start_idx + i].getValue();
                std::string clipped = clipPlain(line, inner_w);
                if (line.starts_with("✔")) {
                    m_os << fitWidth(styled(clipped).bright_green().str(), inner_w);
                } else if (line.starts_with("⚠") || line.starts_with("✖")) {
                    m_os << fitWidth(styled(clipped).bright_red().str(), inner_w);
                } else {
                    m_os << fitWidth(styled(clipped).white().str(), inner_w);
                }
            } else {
                m_os << std::string(inner_w, ' ');
            }
        }

        gotoxy(m_os, RIGHT_X + 2, BOT_Y + BOT_H - 2);
        std::string footer = "Total ops: " + std::to_string(total) + " │ Ciclos: protegidos";
        m_os << fitWidth(styled(footer).dim().str(), inner_w);
    }

    /** @brief Ejecuta un comando especial o fórmula desde la Ventana 3. */
    bool handleCommandSubmit(const std::string& raw_cmd) {
        std::string cmd;
        size_t a = 0, b = raw_cmd.size();
        while (a < b && std::isspace(static_cast<unsigned char>(raw_cmd[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(raw_cmd[b - 1]))) --b;
        cmd = raw_cmd.substr(a, b - a);

        if (cmd.empty()) return true;
        m_cmd_history.push_back(cmd);
        m_history_idx = static_cast<TX>(m_cmd_history.size());

        std::string upper = cmd;
        for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper == "QUIT" || upper == "EXIT" || upper == "Q") {
            return false;
        }
        if (upper == "CLEAR") {
            m_engine.clear();
            m_status_banner = "Hoja de calculo limpiada por completo.";
            return true;
        }
        if (upper == "DEMO") {
            m_engine.clear();
            m_engine.loadProfessorDemo();
            m_cursor_col = 3; // D
            m_cursor_row = 3; // 4 -> D4
            ensureCursorVisible();
            m_status_banner = "Cargado ejemplo de clase: A1=5, B1=A1+4, D4=A1+B1";
            return true;
        }
        if (upper == "SAVE") {
            std::ofstream ofs("output/spreadsheet_state.txt", std::ios::trunc);
            ofs << m_engine.rawTable() << std::endl;
            m_engine.addLog("💾 Guardado en output/spreadsheet_state.txt");
            m_status_banner = "Serializado con HashTable Printable en output/spreadsheet_state.txt";
            return true;
        }
        if (upper == "LOAD") {
            std::ifstream ifs("output/spreadsheet_state.txt");
            if (ifs) {
                ifs >> m_engine.rawTable();
                m_engine.recalculateAll();
                m_engine.addLog("📂 Cargado desde output/spreadsheet_state.txt");
                m_status_banner = "Restaurado con HashTable Readable desde output/spreadsheet_state.txt";
            } else {
                m_engine.addLog("✖ No existe output/spreadsheet_state.txt");
                m_status_banner = "Archivo output/spreadsheet_state.txt no encontrado.";
            }
            return true;
        }

        std::string current_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
        std::string assigned_id;
        bool ok = m_engine.executeCommand(cmd, current_id, &assigned_id);
        CellCoord dest = CellCoord::parse(assigned_id);
        if (dest.valid) {
            m_cursor_col = dest.col;
            m_cursor_row = dest.row;
            ensureCursorVisible();
        }
        if (ok) {
            m_status_banner = "Actualizada " + assigned_id + " = " + m_engine.getCell(assigned_id).displayValue();
        } else {
            m_status_banner = "Error al evaluar " + assigned_id + ": verifica sintaxis o ciclos.";
        }
        return true;
    }

public:
    explicit SpreadsheetTUI(SpreadsheetEngine& engine, std::ostream& os = std::cout)
        : m_engine(engine), m_os(os) {}

    /** @brief Reserva el espacio vertical en consola con `gotoxy_origin`. */
    void initScreen() {
        gotoxy_origin(m_os, TOTAL_HEIGHT);
    }

    /** @brief Redibuja las 4 ventanas independientes de forma atómica y sincronizada. */
    void renderAll() {
        ensureCursorVisible();
        std::string active_id = CellCoord(m_cursor_col, m_cursor_row, true).toId();
        EvalPreview preview = m_engine.previewCommand(m_input_buffer, active_id);

        renderHeaderBar(preview);
        renderGridWindow(preview);
        renderInspectorWindow(preview);
        renderCommandWindow(preview);
        renderActivityWindow();

        gotoxy(m_os, 0, TOTAL_HEIGHT);
        m_os << std::flush;
    }

    /**
     * @brief Ejecuta una demostración animada paso a paso que actualiza las 4 ventanas en tiempo real.
     *
     * Reproduce exactamente la secuencia explicada por el profesor Ernesto Cuadros-Vargas:
     * 1. `A1 = 5`
     * 2. `B1 = A1 + 4` (muestra `9`, conserva `A1 + 4` detrás)
     * 3. `D4 = A1 + B1` (muestra `14`, conserva `A1 + B1` detrás)
     * 4. Cambio reactivo `A1 = 8` -> `B1` pasa a `12` y `D4` pasa a `20` automáticamente.
     */
    void runAnimatedDemo(TX step_delay_ms = 220) {
        initScreen();

        struct DemoStep {
            SpreadsheetFocusMode mode;
            TX col;
            TX row;
            std::string typing;
            bool commit;
            std::string banner;
        };

        const DemoStep steps[] = {
            {SpreadsheetFocusMode::CommandConsole, 0, 0, "A1 = 5",       true,  "Paso 1: Ingresando valor directo A1 = 5"},
            {SpreadsheetFocusMode::CommandConsole, 1, 0, "B1 = A1 + 4",  false, "Paso 2: Tipeando formula dependiente B1 = A1 + 4 (Preview en vivo)"},
            {SpreadsheetFocusMode::CommandConsole, 1, 0, "B1 = A1 + 4",  true,  "Paso 2: B1 muestra 9 y guarda detras fx: A1 + 4"},
            {SpreadsheetFocusMode::CommandConsole, 3, 3, "D4 = A1 + B1", false, "Paso 3: Encadenando operacion D4 = A1 + B1 (Preview en vivo)"},
            {SpreadsheetFocusMode::CommandConsole, 3, 3, "D4 = A1 + B1", true,  "Paso 3: D4 muestra 14 y el Inspector dibuja su BinaryTree AST"},
            {SpreadsheetFocusMode::GridNavigation, 0, 0, "",             false, "Modo Navegacion: Inspeccionando A1 y sus dependientes (B1, D4)"},
            {SpreadsheetFocusMode::CommandConsole, 0, 0, "A1 = 8",       true,  "Paso 4: Actualizando A1 = 8 -> Propagacion automatica a B1(12) y D4(20)"},
            {SpreadsheetFocusMode::GridNavigation, 3, 3, "",             false, "Demo completada: D4 = 20 (fx: A1 + B1)."}
        };

        renderAll();
        for (const auto& st : steps) {
            m_mode = st.mode;
            m_cursor_col = st.col;
            m_cursor_row = st.row;
            m_input_buffer = st.typing;
            m_status_banner = st.banner;
            renderAll();
            if (step_delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
            }
            if (st.commit) {
                handleCommandSubmit(st.typing);
                m_input_buffer.clear();
                renderAll();
                if (step_delay_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
                }
            }
        }
    }

    /**
     * @brief Inicia el bucle interactivo en tiempo real (lectura tecla a tecla con `termios`).
     */
    void runInteractive() {
        if (!isatty(STDIN_FILENO)) {
            runAnimatedDemo(0);
            return;
        }

        // Configurar terminal en modo raw sin eco para capturar flechas, Tab, Backspace y tipeo en vivo
        termios orig_term{};
        tcgetattr(STDIN_FILENO, &orig_term);
        termios raw_term = orig_term;
        raw_term.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw_term.c_cc[VMIN] = 1;
        raw_term.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

        // Ocultar cursor nativo durante el renderizado TUI
        m_os << "\033[?25l" << std::flush;

        initScreen();
        renderAll();

        bool running = true;
        while (running) {
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) <= 0) break;

            // Secuencias de escape (Flechas, Supr, Esc)
            if (ch == '\033') {
                char seq[3] = {0, 0, 0};
                // Lectura no bloqueante corta para distinguir tecla ESC sola de secuencia ANSI
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

                if (n1 == 0) {
                    // Tecla ESC sola: limpia buffer y pasa a Modo Navegación
                    m_input_buffer.clear();
                    m_mode = SpreadsheetFocusMode::GridNavigation;
                    m_status_banner = "Modo Navegacion activo (usa flechas para moverte).";
                } else if (seq[0] == '[') {
                    if (m_mode == SpreadsheetFocusMode::GridNavigation) {
                        if (seq[1] == 'A' && m_cursor_row > 0) --m_cursor_row;       // Up
                        else if (seq[1] == 'B' && m_cursor_row < 98) ++m_cursor_row; // Down
                        else if (seq[1] == 'C' && m_cursor_col < 25) ++m_cursor_col; // Right
                        else if (seq[1] == 'D' && m_cursor_col > 0) --m_cursor_col;  // Left
                        else if (seq[1] == '3' && seq[2] == '~') {                   // Delete
                            std::string cid = CellCoord(m_cursor_col, m_cursor_row, true).toId();
                            m_engine.setCell(cid, "");
                        }
                    } else {
                        // En modo Consola de Comandos: Flechas Arriba/Abajo recorren historial
                        if (seq[1] == 'A' && m_cmd_history.size() > 0) {
                            if (m_history_idx > 0) --m_history_idx;
                            m_input_buffer = m_cmd_history[static_cast<size_t>(m_history_idx)].getValue();
                        } else if (seq[1] == 'B') {
                            if (m_history_idx + 1 < static_cast<TX>(m_cmd_history.size())) {
                                ++m_history_idx;
                                m_input_buffer = m_cmd_history[static_cast<size_t>(m_history_idx)].getValue();
                            } else {
                                m_history_idx = static_cast<TX>(m_cmd_history.size());
                                m_input_buffer.clear();
                            }
                        }
                    }
                }
                renderAll();
                continue;
            }

            // Tabulador: alterna foco entre Ventana 1 (Matriz) y Ventana 3 (Consola de Comandos)
            if (ch == '\t') {
                if (m_mode == SpreadsheetFocusMode::GridNavigation) {
                    m_mode = SpreadsheetFocusMode::CommandConsole;
                    m_status_banner = "Foco en Ventana 3 (Consola). Escribe ej: A1 = 5 o B1 = A1 + 4";
                } else {
                    m_mode = SpreadsheetFocusMode::GridNavigation;
                    m_status_banner = "Foco en Ventana 1 (Matriz). Muevete con flechas o pulsa Enter.";
                }
                renderAll();
                continue;
            }

            // Ctrl+C o Ctrl+D para salir limpiamente
            if (ch == 3 || ch == 4) {
                running = false;
                break;
            }

            if (m_mode == SpreadsheetFocusMode::GridNavigation) {
                std::string cid = CellCoord(m_cursor_col, m_cursor_row, true).toId();
                if (ch == 'q' || ch == 'Q') {
                    running = false;
                    break;
                } else if (ch == '\n' || ch == '\r' || ch == '=') {
                    m_mode = SpreadsheetFocusMode::CommandConsole;
                    SpreadsheetCell cur = m_engine.getCell(cid);
                    if (!cur.raw_expression.empty() && ch != '=') {
                        m_input_buffer = cid + " = " + cur.raw_expression;
                    } else {
                        m_input_buffer = cid + " = ";
                    }
                    m_status_banner = "Editando celda " + cid + " en vivo...";
                } else if (ch == ':') {
                    m_mode = SpreadsheetFocusMode::CommandConsole;
                    m_input_buffer.clear();
                    m_status_banner = "Modo comando directo activado.";
                } else if (ch == 127 || ch == '\b') {
                    m_engine.setCell(cid, "");
                    m_status_banner = "Celda " + cid + " borrada.";
                } else if (ch == 'w' || ch == 'k') {
                    if (m_cursor_row > 0) --m_cursor_row;
                } else if (ch == 's' || ch == 'j') {
                    if (m_cursor_row < 98) ++m_cursor_row;
                } else if (ch == 'a' || ch == 'h') {
                    if (m_cursor_col > 0) --m_cursor_col;
                } else if (ch == 'd' || ch == 'l') {
                    if (m_cursor_col < 25) ++m_cursor_col;
                } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                    // Empezar a escribir un número en la celda activa abre el editor automáticamente
                    m_mode = SpreadsheetFocusMode::CommandConsole;
                    m_input_buffer = cid + " = " + std::string(1, ch);
                }
            } else {
                // Modo 2: Ventana de Consola de Comandos activa
                if (ch == '\n' || ch == '\r') {
                    if (!handleCommandSubmit(m_input_buffer)) {
                        running = false;
                        break;
                    }
                    m_input_buffer.clear();
                } else if (ch == 127 || ch == '\b') {
                    if (!m_input_buffer.empty()) m_input_buffer.pop_back();
                } else if (std::isprint(static_cast<unsigned char>(ch))) {
                    m_input_buffer.push_back(ch);
                    // Si el usuario escribe una coordenada destino como "D4 = ...", sincronizar la vista
                    size_t eq = m_input_buffer.find('=');
                    if (eq != std::string::npos) {
                        CellCoord target = CellCoord::parse(m_input_buffer.substr(0, eq));
                        if (target.valid) {
                            m_cursor_col = target.col;
                            m_cursor_row = target.row;
                        }
                    }
                }
            }

            renderAll();
        }

        // Restaurar cursor y estado de terminal
        m_os << "\033[?25h";
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
        gotoxy(m_os, 0, TOTAL_HEIGHT);
        m_os << "\n";
    }
};

#endif // __SPREADSHEET_TUI_H__
