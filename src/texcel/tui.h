/**
 * @file tui.h
 * @ingroup texcel
 * @brief Orquestador de la interfaz multi-ventana en consola (TUI reactiva) para Texcel.
 *
 * Estructura acíclica: `tui.h` -> `helpers/{state.h, renderer.h, input.h}` -> `engine.h`.
 */

#ifndef __TEXCEL_TUI_H__
#define __TEXCEL_TUI_H__

#include "helpers/state.h"
#include "helpers/renderer.h"
#include "helpers/input.h"
#include "../utilities/gotoxy.h"
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <poll.h>
#include <termios.h>
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

    texcel_tui::TuiState& state() { return m_st; }

    void initScreen() {
        m_st.updateLayout();
        gotoxy_origin(m_st.os, texcel_tui::TuiState::TOTAL_HEIGHT);
    }

    void renderAll(bool force_clear = false) {
        texcel_tui::renderAll(m_st, force_clear);
    }

    /** @brief Bucle principal interactivo (modo raw `termios`, `SIGWINCH` y ratón SGR). */
    void runInteractive() {
        if (!isatty(STDIN_FILENO)) return;

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
            TermGuard(std::ostream& o, termios t, struct sigaction sa_prev, bool& flag)
                : os(o), orig(t), prev_sa(sa_prev), alt_flag(flag) {
                texcel_tui::enterInteractiveTerminal(os);
            }
            ~TermGuard() {
                alt_flag = false;
                sigaction(SIGWINCH, &prev_sa, nullptr);
                texcel_tui::exitInteractiveTerminal(os);
                tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            }
        } guard{m_st.os, orig_term, old_sa, m_st.interactive_alt_screen};

        m_st.config.loadConfig();
        if (m_st.config.autosave && std::filesystem::exists(m_st.config.sheetPath(m_st.config.current_sheet))) {
            m_st.loadSheet(m_st.config.current_sheet);
        }

        m_st.updateLayout();
        renderAll(true);

        bool running = true;
        while (running && !m_st.should_exit) {
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
                if (m_st.hasCellSel() || m_st.hasTextSel()) {
                    m_st.copySelectionToClipboard();
                    renderAll();
                    continue;
                }
                break;
            }
            if (ch == 4) break;

            if (ch == 24) { // Ctrl+X: Prefijo de comandos
                if (!m_st.isModalOpen()) {
                    m_st.chord_x = !m_st.chord_x;
                    renderAll();
                }
                continue;
            }
            if (m_st.chord_x) {
                if (ch == Ansi::ESC) {
                    m_st.chord_x = false;
                    renderAll();
                    continue;
                }
                texcel_tui::handleChordXKey(m_st, ch);
                renderAll();
                continue;
            }
            if (ch == 19) { m_st.saveSheet(m_st.config.current_sheet); m_st.refreshSheetList(); renderAll(); continue; }
            if (ch == 6) { // Ctrl+F: Catálogo interactivo de funciones (fx)
                if (m_st.modal_kind == texcel_tui::ModalKind::Functions) m_st.closeModal();
                else m_st.openModal(texcel_tui::ModalKind::Functions);
                renderAll();
                continue;
            }
            if (ch == 7)  { // Ctrl+G: Ir a celda o rango en el Name Box
                m_st.closeModal();
                m_st.beginNameBoxEdit();
                renderAll();
                continue;
            }
            if (ch == 15) { // Ctrl+O: Gestor de hojas
                if (m_st.modal_kind == texcel_tui::ModalKind::Sheets) m_st.closeModal();
                else m_st.openModal(texcel_tui::ModalKind::Sheets);
                renderAll();
                continue;
            }
            if (ch == Ansi::ESC) {
                bool need_render = texcel_tui::handleEscapeSequence(m_st, raw_term);
                // Coalescencia de ráfagas SGR Mouse Drag: drenar eventos ya encolados en stdin antes de renderizar
                while (m_st.isAnyDragging()) {
                    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
                    if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN)) break;
                    char next_ch = 0;
                    if (read(STDIN_FILENO, &next_ch, 1) <= 0) break;
                    if (next_ch == Ansi::ESC) {
                        need_render |= texcel_tui::handleEscapeSequence(m_st, raw_term);
                    } else {
                        break;
                    }
                }
                if (need_render) renderAll();
                continue;
            }
            if (m_st.isModalOpen())   { texcel_tui::handleModalKey(m_st, ch);   renderAll(); continue; }
            if (m_st.namebox_editing) { texcel_tui::handleNameBoxKey(m_st, ch); renderAll(); continue; }
            if (ch == '\t')           { texcel_tui::handleTabKey(m_st);         renderAll(); continue; }

            if (m_st.mode == SpreadsheetFocusMode::GridNavigation) running = texcel_tui::handleGridKey(m_st, ch);
            else running = texcel_tui::handleConsoleKey(m_st, ch);

            if (running) renderAll();
        }
    }
};

#endif // __TEXCEL_TUI_H__
