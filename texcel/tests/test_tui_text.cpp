#include "../helpers/tui_text.h"
#include "../../utilities/ansi_style.h"
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

    // 2. alignCell con caracteres UTF-8 multibyte (· = 2 bytes, 1 columna visual)
    std::string dot_cell = alignCell("·", 9, false);
    assert(visible_length(dot_cell) == 9);
    std::string num_cell = alignCell("42", 9, true);
    assert(visible_length(num_cell) == 9);

    // 3. Normalizacion de rangos
    TX cmin, rmin, cmax, rmax;
    normalizeCellRange(4, 5, 1, 2, cmin, rmin, cmax, rmax);
    assert(cmin == 1 && rmin == 2 && cmax == 4 && rmax == 5);

    TX sx0, sy0, sx1, sy1;
    normalizeTextRange(10, 8, 3, 4, sx0, sy0, sx1, sy1);
    assert(sx0 == 3 && sy0 == 4 && sx1 == 10 && sy1 == 8);

    std::cout << "[PASS] test_tui_text\n";
    return 0;
}
