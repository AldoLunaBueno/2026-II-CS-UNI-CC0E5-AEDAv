/**
 * @file tui.h
 * @ingroup visualization
 * @brief Orquestador de la interfaz multi-ventana en consola (TUI reactiva) para Texcel.
 *
 * Delega las utilidades de texto/UTF-8, el renderizado 2D y los despachadores de entrada
 * a `texcel/helpers/tui_text.h`, `texcel/helpers/tui_renderer.h` y `texcel/helpers/tui_input.h`.
 */

#ifndef __TEXCEL_TUI_H__
#define __TEXCEL_TUI_H__

#include "engine.h"
#include "config.h"
#include "clipboard.h"
#include "helpers/tui_text.h"
#include "../utilities/gotoxy.h"
#include "../utilities/ansi_style.h"
#include "../utilities/ansi_metrics.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <csignal>
#include <cerrno>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace texcel_sig {
inline volatile sig_atomic_t resized_flag = 0;
inline void on_sigwinch(int) { resized_flag = 1; }
} // namespace texcel_sig

/**
 * @enum SpreadsheetFocusMode
 * @brief Ventana activa en la interfaz dual: Navegación en Matriz o Consola de Comandos.
 */
enum class SpreadsheetFocusMode {
    GridNavigation = 0, /**< Foco en la Matriz: movimiento por flechas/WASD, Enter para editar. */
    CommandConsole = 1  /**< Foco en la Consola: escritura libre `= A1 + 4`, `5`, `B1 = A1 + 4`. */
};

/**
 * @class SpreadsheetTUI
 * @brief Gestor visual de 4 ventanas independientes en consola usando `gotoxy` y `ansi_style.h`.
 */
class SpreadsheetTUI {
public:
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

private:
    SpreadsheetEngine&   m_engine;
    std::ostream&        m_os;
    std::ostream*        m_render_os = &m_os;
    SpreadsheetFocusMode m_mode = SpreadsheetFocusMode::GridNavigation;

    std::ostream& ros() { return *m_render_os; }

    // Geometría adaptativa al ancho real de la consola
    TX   m_total_width  = DEFAULT_WIDTH;
    TX   m_term_rows    = TOTAL_HEIGHT;
    TX   m_left_w       = 56;
    TX   m_right_x      = 57;
    TX   m_right_w      = 41;
    TX   m_visible_cols = 5;
    bool m_interactive_alt_screen = false;

    TX m_cursor_col = 0;
    TX m_cursor_row = 0;
    TX m_scroll_col = 0;
    TX m_scroll_row = 0;

    // Point Mode
    bool        m_point_mode = false;
    TX          m_target_col = 0;
    TX          m_target_row = 0;
    std::string m_point_base;

    // Selección con ratón (SGR Mouse Tracking) confinada por ventana
    bool m_mouse_dragging = false;
    TX   m_drag_win       = 0;
    bool m_has_cell_sel   = false;
    TX   m_sel_c0 = 0, m_sel_r0 = 0, m_sel_c1 = 0, m_sel_r1 = 0;
    bool m_has_text_sel   = false;
    TX   m_text_win       = 0;
    TX   m_tx0 = 0, m_ty0 = 0, m_tx1 = 0, m_ty1 = 0;
    std::string m_win_lines[5][TOTAL_HEIGHT];

    // Consola e historial
    std::string m_input_buffer;
    size_t      m_input_cursor = 0;
    std::string m_status_banner = "Usa [Tab] para alternar entre Matriz y Consola.";
    Vector<VectorAscTraits<std::string>> m_cmd_history;
    TX m_history_idx = -1;

    // Gestor de hojas y configuración
    TexcelConfig m_config;
    std::string& m_sheets_dir    = m_config.sheets_dir;
    std::string& m_current_sheet = m_config.current_sheet;
    bool&        m_autosave      = m_config.autosave;
    Vector<VectorAscTraits<std::string>>& m_sheet_files = m_config.sheet_files;
    bool        m_modal_open     = false;
    bool        m_modal_new_mode = false;
    std::string m_modal_new_name;
    TX          m_modal_idx      = 0;

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
        if (!m_point_mode) return;
        m_input_buffer = m_point_base + CellCoord(m_cursor_col, m_cursor_row, true).toId();
    }

    void copyToClipboardOsc52(const std::string& text) {
        TexcelClipboard::copy(m_os, text);
    }

    std::string extractCurrentSelectionText() {
        if (m_has_cell_sel) {
            TX cmin, rmin, cmax, rmax;
            texcel_tui::normalizeCellRange(m_sel_c0, m_sel_r0, m_sel_c1, m_sel_r1, cmin, rmin, cmax, rmax);
            std::ostringstream tsv;
            for (TX r = rmin; r <= rmax; ++r) {
                if (r > rmin) tsv << "\n";
                for (TX c = cmin; c <= cmax; ++c) {
                    if (c > cmin) tsv << "\t";
                    tsv << m_engine.getCell(CellCoord(c, r, true).toId()).displayValue();
                }
            }
            return tsv.str();
        }
        if (m_has_text_sel && m_text_win >= 2 && m_text_win <= 4) {
            TX wx0, wy0, wx1, wy1;
            if (windowInnerBounds(m_text_win, wx0, wy0, wx1, wy1)) {
                TX sx0, sy0, sx1, sy1;
                texcel_tui::normalizeTextRange(m_tx0, m_ty0, m_tx1, m_ty1, sx0, sy0, sx1, sy1);
                std::ostringstream copied;
                for (TX y = sy0; y <= sy1 && y < TOTAL_HEIGHT; ++y) {
                    if (y > sy0) copied << "\n";
                    const std::string& line = m_win_lines[m_text_win][y];
                    TX c0 = (y == sy0) ? std::max<TX>(0, sx0 - wx0) : 0;
                    TX c1 = (y == sy1) ? std::max<TX>(0, sx1 - wx0) : static_cast<TX>(line.size());
                    if (c0 <= c1 && c0 < static_cast<TX>(line.size())) {
                        copied << line.substr(static_cast<size_t>(c0), static_cast<size_t>(c1 - c0 + 1));
                    }
                }
                return copied.str();
            }
        }
        return m_engine.getCell(CellCoord(m_cursor_col, m_cursor_row, true).toId()).displayValue();
    }

    bool windowInnerBounds(TX win_id, TX& x0, TX& y0, TX& x1, TX& y1) const {
        if (win_id == 2) { x0 = m_right_x + 2; y0 = TOP_Y + 1; x1 = m_right_x + m_right_w - 3; y1 = TOP_Y + TOP_H - 2; return true; }
        if (win_id == 3) { x0 = LEFT_X + 2;    y0 = BOT_Y + 1; x1 = LEFT_X + m_left_w - 3;     y1 = BOT_Y + BOT_H - 2; return true; }
        if (win_id == 4) { x0 = m_right_x + 2; y0 = BOT_Y + 1; x1 = m_right_x + m_right_w - 3; y1 = BOT_Y + BOT_H - 2; return true; }
        return false;
    }

    bool screenToGridCell(TX mx, TX my, TX& out_col, TX& out_row, bool clamp_to_grid) const {
        TX grid_x0 = LEFT_X + 7;
        TX grid_x1 = grid_x0 + m_visible_cols * (CELL_WIDTH + 1) - 1;
        TX grid_y0 = TOP_Y + 3;
        TX grid_y1 = TOP_Y + 3 + VISIBLE_ROWS - 1;

        if (!clamp_to_grid) {
            if (mx < grid_x0 || mx > grid_x1 || my < grid_y0 || my > grid_y1) return false;
        } else {
            mx = std::clamp(mx, grid_x0, grid_x1);
            my = std::clamp(my, grid_y0, grid_y1);
        }

        TX rel_c = (mx - grid_x0) / (CELL_WIDTH + 1);
        TX rel_r = my - grid_y0;
        out_col = std::clamp(m_scroll_col + rel_c, static_cast<TX>(0), static_cast<TX>(25));
        out_row = std::clamp(m_scroll_row + rel_r, static_cast<TX>(0), static_cast<TX>(98));
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
        bool changed = (new_total_w != m_total_width) || (rows != m_term_rows);

        m_term_rows    = rows;
        m_total_width  = new_total_w;
        m_right_w      = std::clamp((m_total_width * 38) / 100, static_cast<TX>(26), static_cast<TX>(68));
        m_left_w       = m_total_width - m_right_w - 1;
        m_right_x      = m_left_w + 1;
        TX avail_grid  = std::max(static_cast<TX>(14), m_left_w - 9);
        m_visible_cols = std::clamp(avail_grid / (CELL_WIDTH + 1), static_cast<TX>(1), static_cast<TX>(26));
        return changed;
    }

    void ensureCursorVisible() {
        if (m_cursor_col < m_scroll_col) m_scroll_col = m_cursor_col;
        if (m_cursor_col >= m_scroll_col + m_visible_cols) m_scroll_col = m_cursor_col - m_visible_cols + 1;
        if (m_cursor_row < m_scroll_row) m_scroll_row = m_cursor_row;
        if (m_cursor_row >= m_scroll_row + VISIBLE_ROWS) m_scroll_row = m_cursor_row - VISIBLE_ROWS + 1;
    }

    void clampScroll() {
        m_scroll_col = std::clamp<TX>(m_scroll_col, 0, std::max<TX>(0, 26 - m_visible_cols));
        m_scroll_row = std::clamp<TX>(m_scroll_row, 0, std::max<TX>(0, 99 - VISIBLE_ROWS));
    }

    // Persistencia de hojas delegada en TexcelConfig
    std::filesystem::path sheetPath(const std::string& name) const { return m_config.sheetPath(name); }
    void ensureSheetsDir() const { m_config.ensureSheetsDir(); }
    void loadConfig() { m_config.loadConfig(); }
    void saveConfig() const { m_config.saveConfig(); }

    void refreshSheetList() {
        m_config.refreshSheetList();
        TX max_idx = 2 + static_cast<TX>(m_sheet_files.size());
        m_modal_idx = std::clamp<TX>(m_modal_idx, 0, max_idx);
    }

    void saveSheet(const std::string& name, bool silent = false) {
        ensureSheetsDir();
        std::filesystem::path p = sheetPath(name);
        std::ofstream ofs(p, std::ios::trunc);
        if (ofs) {
            ofs << m_engine.rawTable() << std::endl;
            m_current_sheet = name;
            saveConfig();
            if (!silent) {
                m_engine.addLog("💾 Guardada hoja: " + name + TexcelConfig::SHEET_EXT);
                m_status_banner = "Hoja '" + name + "' guardada en " + p.string();
            }
        } else if (!silent) {
            m_engine.addLog("✖ Error al guardar " + name + TexcelConfig::SHEET_EXT);
            m_status_banner = "No se pudo guardar '" + name + "'.";
        }
    }

    bool loadSheet(const std::string& name) {
        std::ifstream ifs(sheetPath(name));
        if (!ifs) {
            m_engine.addLog("✖ No existe " + name + TexcelConfig::SHEET_EXT);
            m_status_banner = "Archivo '" + name + TexcelConfig::SHEET_EXT + "' no encontrado.";
            return false;
        }
        m_engine.clear();
        ifs >> m_engine.rawTable();
        m_engine.recalculateAll();
        m_current_sheet = name;
        saveConfig();
        m_engine.addLog("📂 Cargada hoja: " + name + TexcelConfig::SHEET_EXT);
        m_status_banner = "Hoja '" + name + "' cargada correctamente.";
        return true;
    }

    void maybeAutoSave() {
        if (m_autosave) saveSheet(m_current_sheet, true);
    }

    // Declaraciones implementadas en helpers/tui_renderer.h
    void drawWindowLineWithSelection(TX win_id, TX start_x, TX abs_y, const std::string& styled_line, size_t inner_w);
    void drawWindowFrame(TX x, TX y, TX w, TX h, const std::string& title, bool active_focus,
                         bool accent_secondary = false, bool clear_interior = false);
    void renderHeaderBar(const EvalPreview& preview);
    void renderGridWindow(const EvalPreview& preview);
    void renderInspectorWindow(const EvalPreview& preview);
    void renderCommandWindow(const EvalPreview& preview);
    void renderActivityWindow();
    void renderFileManagerModal();

    // Declaraciones implementadas en helpers/tui_input.h
    std::string formatCellForInput(const std::string& cid);
    void loadActiveCellIntoBuffer();
    void deleteSelectedModalSheet();
    bool handleCommandSubmit(const std::string& raw_cmd);
    bool handleSgrMouse(const std::string& payload, char mc);
    bool handleEscapeSequence(termios& raw_term);
    void handleModalKey(char ch);
    void handleTabKey();
    bool handlePointModeKey(char ch);
    bool handleGridKey(char ch);
    bool handleConsoleKey(char ch);

public:
    explicit SpreadsheetTUI(SpreadsheetEngine& engine, std::ostream& os = std::cout)
        : m_engine(engine), m_os(os) {}

    void initScreen() {
        updateLayout();
        gotoxy_origin(m_os, TOTAL_HEIGHT);
    }

    void renderAll(bool force_clear = false);

    /** @brief Ejecuta la demostración animada paso a paso de la secuencia de clase. */
    void runAnimatedDemo(TX step_delay_ms = 220) {
        initScreen();
        struct DemoStep {
            SpreadsheetFocusMode mode; TX col; TX row;
            std::string typing; bool commit; std::string banner;
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
            m_mode = st.mode; m_cursor_col = st.col; m_cursor_row = st.row;
            m_input_buffer = st.typing; m_status_banner = st.banner;
            renderAll();
            if (step_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
            if (st.commit) {
                handleCommandSubmit(st.typing);
                m_input_buffer.clear();
                renderAll();
                if (step_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
            }
        }
    }

    /** @brief Bucle principal interactivo (modo raw `termios`, `SIGWINCH` y ratón SGR). */
    void runInteractive() {
        if (!isatty(STDIN_FILENO)) { runAnimatedDemo(0); return; }

        termios orig_term{};
        tcgetattr(STDIN_FILENO, &orig_term);
        termios raw_term = orig_term;
        raw_term.c_iflag &= static_cast<tcflag_t>(~IXON);
        raw_term.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
        raw_term.c_cc[VMIN] = 1;
        raw_term.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_term);

        struct sigaction sa{}, old_sa{};
        sa.sa_handler = texcel_sig::on_sigwinch;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGWINCH, &sa, &old_sa);
        texcel_sig::resized_flag = 0;

        m_interactive_alt_screen = true;
        struct TermGuard {
            std::ostream& os; termios orig; struct sigaction prev_sa; bool& alt_flag;
            ~TermGuard() {
                alt_flag = false;
                sigaction(SIGWINCH, &prev_sa, nullptr);
                os << "\033[?7h\033[?1006l\033[?1002l\033[?1000l\033[?25h\033[?1049l" << std::flush;
                tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            }
        } guard{m_os, orig_term, old_sa, m_interactive_alt_screen};

        loadConfig();
        if (m_autosave && std::filesystem::exists(sheetPath(m_current_sheet))) {
            loadSheet(m_current_sheet);
        }

        m_os << "\033[?1049h\033[?7l\033[1;1H\033[s\033[?25l\033[?1000h\033[?1002h\033[?1006h" << std::flush;
        updateLayout();
        renderAll(true);

        bool running = true;
        while (running) {
            char ch = 0;
            ssize_t nread = read(STDIN_FILENO, &ch, 1);
            if (nread < 0) {
                if (errno == EINTR && texcel_sig::resized_flag) {
                    texcel_sig::resized_flag = 0;
                    renderAll(true);
                }
                if (errno == EINTR) continue;
                break;
            }
            if (nread == 0) break;

            if (ch == 3) {
                if (m_has_cell_sel || m_has_text_sel) {
                    std::string txt = extractCurrentSelectionText();
                    if (!txt.empty()) {
                        copyToClipboardOsc52(txt);
                        m_status_banner = "✔ Seleccion copiada al portapapeles.";
                    }
                    renderAll();
                    continue;
                }
                break;
            }
            if (ch == 4) break;

            if (ch == 19) { saveSheet(m_current_sheet); refreshSheetList(); renderAll(); continue; }
            if (ch == 15) {
                m_modal_open = !m_modal_open;
                m_modal_new_mode = false;
                m_modal_new_name.clear();
                if (m_modal_open) refreshSheetList();
                renderAll();
                continue;
            }
            if (ch == '\033') {
                if (handleEscapeSequence(raw_term)) renderAll();
                continue;
            }
            if (m_modal_open) { handleModalKey(ch); renderAll(); continue; }
            if (ch == '\t')   { handleTabKey();     renderAll(); continue; }

            if (m_mode == SpreadsheetFocusMode::GridNavigation) running = handleGridKey(ch);
            else running = handleConsoleKey(ch);

            if (running) renderAll();
        }
    }
};

#include "helpers/tui_renderer.h"
#include "helpers/tui_input.h"

#endif // __TEXCEL_TUI_H__
