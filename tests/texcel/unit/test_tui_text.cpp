#include "../../../src/texcel/helpers/text.h"
#include "../../../src/utilities/ansi_style.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace texcel_tui;

    // 1. stripAnsi y fitWidth con relleno y recorte ANSI seguro
    std::string colored_txt = styled("Hola Mundo").bright_cyan().bold().str();
    assert(stripAnsi(colored_txt) == "Hola Mundo");
    assert(visible_length(fitWidth(colored_txt, 15)) == 15);
    assert(visible_length(fitWidth(colored_txt, 4)) == 4);
    assert(stripAnsi(fitWidth(colored_txt, 4)) == "Hola");

    // 2. alignCell y clipPlain con caracteres UTF-8 multibyte de ancho 1 (·, ■, □, ↑, ↓, ✔)
    std::string dot_cell = alignCell("·", 9, false);
    assert(visible_length(dot_cell) == 9);
    assert(visible_length(alignCell("■", 9, false)) == 9);
    assert(visible_length(alignCell("□", 9, false)) == 9);
    std::string num_cell = alignCell("42", 9, true);
    assert(visible_length(num_cell) == 9);
    std::string modal_help = "[↑/↓] Mover │ [Enter] OK │ [Supr] Borrar │ [Esc] Cerrar";
    assert(clipPlain(modal_help, 56) == modal_help);
    assert(visible_length(clipPlain("✔ A1 = 8 ➔ 8", 8)) == 8);

    // 3. Emojis y caracteres Wide de 2 columnas visuales (📂, 💾, 🗑) + secuencias OSC 52
    std::string log_emoji = "📂 Cargada hoja: default.txl";
    assert(Ansi::codepointWidth(0x1F4C2u) == 2); // 📂
    assert(Ansi::codepointWidth(0x1F4BEu) == 2); // 💾
    assert(Ansi::codepointWidth(0x1F5D1u) == 2); // 🗑
    assert(Ansi::codepointWidth(0x2714u) == 1);  // ✔
    assert(visible_length("📂") == 2);
    assert(visible_length(fitWidth(log_emoji, 20)) == 20);
    // Recorte sobre columna impar en medio de un emoji de ancho 2 rellena con espacio sin desbordar
    assert(visible_length(fitWidth("📂📂", 3)) == 3);

    std::string with_osc = "\033]52;c;SG9sYQ==\aHola";
    assert(stripAnsi(with_osc) == "Hola");
    assert(visible_length(with_osc) == 4);

    // 4. Normalización de rangos
    TX cmin, rmin, cmax, rmax;
    normalizeCellRange(4, 5, 1, 2, cmin, rmin, cmax, rmax);
    assert(cmin == 1 && rmin == 2 && cmax == 4 && rmax == 5);

    TX sx0, sy0, sx1, sy1;
    normalizeTextRange(10, 8, 3, 4, sx0, sy0, sx1, sy1);
    assert(sx0 == 3 && sy0 == 4 && sx1 == 10 && sy1 == 8);

    std::cout << "[PASS] test_tui_text\n";
    return 0;
}
