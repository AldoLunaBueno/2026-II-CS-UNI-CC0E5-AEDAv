/**
 * @file tui.h
 * @ingroup visualization
 * @brief Orquestador de la interfaz multi-ventana en consola (TUI reactiva) para Texcel.
 *
 * Estructura acíclica: `tui.h` -> `helpers/{tui_state.h, tui_renderer.h, tui_input.h}` -> `engine.h`.
 */

#ifndef __TEXCEL_TUI_H__
#define __TEXCEL_TUI_H__

#include "helpers/tui_state.h"
#include "helpers/tui_renderer.h"
#include "helpers/tui_input.h"
#include "../utilities/gotoxy.h"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace texcel_sig {
inline volatile sig_atomic_t resized_flag = 0;
inline void on_sigwinch(int) { resized_flag = 1; }
} // namespace texcel_sig

/**
 * @class SpreadsheetTUI
 * @brief Orquestador de la interfaz multi-ventana de Texcel sobre `texcel_tui::TuiState`.
 */
class SpreadsheetTUI {
    texcel_tui::TuiState m_st;

public:
    explicit SpreadsheetTUI(SpreadsheetEngine& engine, std::ostream& os = std::cout)
        : m_st(engine, os) {}

    void initScreen() {
        m_st.updateLayout();
        gotoxy_origin(m_st.os, texcel_tui::TuiState::TOTAL_HEIGHT);
    }

    void renderAll(bool force_clear = false) {
        texcel_tui::renderAll(m_st, force_clear);
    }

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
            m_st.mode          = st.mode;
            m_st.cursor_col    = st.col;
            m_st.cursor_row    = st.row;
            m_st.input_buffer  = st.typing;
            m_st.status_banner = st.banner;
            renderAll();
            if (step_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(step_delay_ms));
            if (st.commit) {
                texcel_tui::handleCommandSubmit(m_st, st.typing);
                m_st.input_buffer.clear();
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

        m_st.interactive_alt_screen = true;
        struct TermGuard {
            std::ostream& os; termios orig; struct sigaction prev_sa; bool& alt_flag;
            ~TermGuard() {
                alt_flag = false;
                sigaction(SIGWINCH, &prev_sa, nullptr);
                os << "\033[?2004l\033[?7h\033[?1006l\033[?1003l\033[?1002l\033[?1000l\033[?25h\033[?1049l" << std::flush;
                tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            }
        } guard{m_st.os, orig_term, old_sa, m_st.interactive_alt_screen};

        m_st.config.loadConfig();
        if (m_st.config.autosave && std::filesystem::exists(m_st.config.sheetPath(m_st.config.current_sheet))) {
            m_st.loadSheet(m_st.config.current_sheet);
        }

        m_st.os << "\033[?1049h\033[?7l\033[?2004h\033[1;1H\033[s\033[?25l\033[?1003l\033[?1000h\033[?1002h\033[?1006h" << std::flush;
        m_st.updateLayout();
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
                if (m_st.has_cell_sel || m_st.has_text_sel) {
                    std::string txt = m_st.extractCurrentSelectionText();
                    if (!txt.empty()) {
                        m_st.copyToClipboard(txt);
                        m_st.status_banner = "✔ Seleccion copiada al portapapeles.";
                    }
                    renderAll();
                    continue;
                }
                break;
            }
            if (ch == 4) break;

            if (ch == 19) { m_st.saveSheet(m_st.config.current_sheet); m_st.refreshSheetList(); renderAll(); continue; }
            if (ch == 7)  { // Ctrl+G: Ir a celda o rango en el Name Box
                m_st.namebox_editing = true;
                m_st.namebox_buffer.clear();
                m_st.mode = SpreadsheetFocusMode::GridNavigation;
                m_st.status_banner = "Ir a celda o rango (ej. ZZ500 o A1:C5) y pulsa [Enter].";
                renderAll();
                continue;
            }
            if (ch == 15) {
                m_st.modal_open = !m_st.modal_open;
                m_st.modal_new_mode = false;
                m_st.modal_new_name.clear();
                if (m_st.modal_open) m_st.refreshSheetList();
                renderAll();
                continue;
            }
            if (ch == '\033') {
                bool need_render = texcel_tui::handleEscapeSequence(m_st, raw_term);
                // Coalescencia de ráfagas SGR Mouse Drag: drenar eventos ya encolados en stdin antes de renderizar
                while (m_st.mouse_dragging || m_st.vscroll_dragging || m_st.hscroll_dragging) {
                    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
                    if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN)) break;
                    char next_ch = 0;
                    if (read(STDIN_FILENO, &next_ch, 1) <= 0) break;
                    if (next_ch == '\033') {
                        need_render |= texcel_tui::handleEscapeSequence(m_st, raw_term);
                    } else {
                        break;
                    }
                }
                if (need_render) renderAll();
                continue;
            }
            if (m_st.modal_open)      { texcel_tui::handleModalKey(m_st, ch);   renderAll(); continue; }
            if (m_st.namebox_editing) { texcel_tui::handleNameBoxKey(m_st, ch); renderAll(); continue; }
            if (ch == '\t')           { texcel_tui::handleTabKey(m_st);         renderAll(); continue; }

            if (m_st.mode == SpreadsheetFocusMode::GridNavigation) running = texcel_tui::handleGridKey(m_st, ch);
            else running = texcel_tui::handleConsoleKey(m_st, ch);

            if (running) renderAll();
        }
    }
};

#endif // __TEXCEL_TUI_H__
