/**
 * @file loading_anim.h
 * @ingroup texcel
 * @brief Animación de carga estilo Claude Code en la ventana Fórmula (`TexcelLoadingSpinner`).
 */

#ifndef __TEXCEL_HELPERS_LOADING_ANIM_H__
#define __TEXCEL_HELPERS_LOADING_ANIM_H__

#include "text.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace texcel_tui {

/**
 * @class TexcelLoadingSpinner
 * @brief Spinner RAII con estrella rotante y efecto *shimmer* RGB dentro de la ventana Fórmula.
 *
 * Espera un breve umbral de gracia (~80 ms) antes de dibujar el primer frame, evitando parpadeos
 * en operaciones instantáneas. Si la operación continúa, anima a ~15 FPS únicamente la fila de
 * estado de la Ventana 3 sin redibujar el resto de la TUI.
 */
template <typename StateT>
class TexcelLoadingSpinner {
    StateT&                 m_st;
    std::mutex              m_mtx;
    std::condition_variable m_cv;
    std::string             m_label;
    bool                    m_stop = false;
    bool                    m_drew = false;
    std::thread             m_worker;

    static std::string renderShimmerText(const std::string& utf8_text, size_t frame_idx) {
        std::vector<std::string_view> cps;
        for (size_t i = 0; i < utf8_text.size();) {
            size_t len = utf8CharLen(static_cast<unsigned char>(utf8_text[i]));
            if (i + len > utf8_text.size()) len = utf8_text.size() - i;
            cps.emplace_back(utf8_text.data() + i, len);
            i += len;
        }
        if (cps.empty()) return "";

        const size_t n = cps.size();
        const size_t cycle = n + 10;
        const int wave_pos = static_cast<int>(frame_idx % cycle) - 3;

        std::string out;
        out.reserve(utf8_text.size() * 20);
        for (size_t i = 0; i < n; ++i) {
            int dist = static_cast<int>(i) - wave_pos;
            if (dist < 0) dist = -dist;
            if (dist == 0)      out += "\033[38;2;255;255;255m";
            else if (dist == 1) out += "\033[38;2;253;224;170m";
            else if (dist == 2) out += "\033[38;2;226;190;145m";
            else                out += "\033[38;2;165;168;178m";
            out.append(cps[i]);
        }
        out += "\033[0m";
        return out;
    }

    void runLoop(std::chrono::steady_clock::time_point start_tp) {
        using namespace std::chrono_literals;
        static constexpr const char* GLYPHS[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        constexpr size_t N_GLYPHS = sizeof(GLYPHS) / sizeof(GLYPHS[0]);

        {
            std::unique_lock<std::mutex> lk(m_mtx);
            if (m_cv.wait_for(lk, 80ms, [&] { return m_stop; })) return;
        }

        const TX x = StateT::LEFT_X + 2;
        const TX y = StateT::BOT_Y + 5;
        size_t frame = 0;

        while (true) {
            std::string label_copy;
            {
                std::lock_guard<std::mutex> lk(m_mtx);
                if (m_stop) break;
                label_copy = m_label;
            }

            if (m_st.left_w >= 20) {
                const size_t inner_w = static_cast<size_t>(m_st.left_w - 4);
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_tp).count();

                std::string line = "\033[38;2;245;158;11m\033[1m";
                line += GLYPHS[frame % N_GLYPHS];
                line += "\033[0m ";
                line += renderShimmerText(label_copy, frame);
                if (elapsed_ms >= 200) {
                    char tbuf[24];
                    std::snprintf(tbuf, sizeof(tbuf), " (%.1fs)", static_cast<double>(elapsed_ms) / 1000.0);
                    line += "\033[2m";
                    line += tbuf;
                    line += "\033[0m";
                }

                std::ostringstream oss;
                gotoxy(oss, x, y);
                oss << fitWidth(line, inner_w);
                m_st.os << oss.str() << std::flush;
                m_drew = true;
            }

            ++frame;
            std::unique_lock<std::mutex> lk(m_mtx);
            if (m_cv.wait_for(lk, 65ms, [&] { return m_stop; })) break;
        }
    }

public:
    TexcelLoadingSpinner(StateT& st, std::string initial_label)
        : m_st(st), m_label(std::move(initial_label)) {
        if (!m_st.interactive_alt_screen) return;
        auto start_tp = std::chrono::steady_clock::now();
        m_worker = std::thread([this, start_tp] { runLoop(start_tp); });
    }

    void setLabel(std::string next_label) {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_label = std::move(next_label);
    }

    ~TexcelLoadingSpinner() {
        if (!m_worker.joinable()) return;
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_stop = true;
        }
        m_cv.notify_all();
        m_worker.join();
        if (m_drew && m_st.left_w >= 20) {
            const size_t inner_w = static_cast<size_t>(m_st.left_w - 4);
            std::ostringstream oss;
            gotoxy(oss, StateT::LEFT_X + 2, StateT::BOT_Y + 5);
            oss << fitWidth("", inner_w);
            m_st.os << oss.str() << std::flush;
        }
    }
};

} // namespace texcel_tui

#endif // __TEXCEL_HELPERS_LOADING_ANIM_H__
