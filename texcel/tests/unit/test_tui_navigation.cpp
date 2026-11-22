#include "../../texcel.h"
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    SpreadsheetEngine e;
    std::ostringstream dummy_os;
    texcel_tui::TuiState st(e, dummy_os);
    st.updateLayout();

    // 1. Name Box: salto a celda lejana y seleccion de rango
    assert(st.goToCellOrRange("ZZ500"));
    assert(st.cursor_col == 701 && st.cursor_row == 499);
    assert(!st.has_cell_sel);

    assert(st.goToCellOrRange("AA10:AC20"));
    assert(st.has_cell_sel);
    assert(st.sel_c0 == 26 && st.sel_r0 == 9);
    assert(st.sel_c1 == 28 && st.sel_r1 == 19);

    assert(!st.goToCellOrRange("INVALIDO9999"));

    // 2. Doble clic en Name Box -> regreso a A1
    st.jumpToHome();
    assert(st.cursor_col == 0 && st.cursor_row == 0);
    assert(st.scroll_col == 0 && st.scroll_row == 0);
    assert(!st.has_cell_sel);

    // 3. Scrollbars proporcionales (vertical y horizontal)
    st.scrollVerticalToScreenY(texcel_tui::TuiState::TOP_Y + 3 + texcel_tui::TuiState::VISIBLE_ROWS - 1);
    assert(st.scroll_row == CellCoord::MAX_ROWS - texcel_tui::TuiState::VISIBLE_ROWS);
    st.scrollHorizontalToScreenX(texcel_tui::TuiState::LEFT_X + st.left_w - 2);
    assert(st.scroll_col == CellCoord::MAX_COLS - st.visible_cols);
    st.scrollVerticalToScreenY(texcel_tui::TuiState::TOP_Y + 3);
    st.scrollHorizontalToScreenX(texcel_tui::TuiState::LEFT_X + 2 + texcel_tui::TuiState::ROW_HEADER_W + 1);
    assert(st.scroll_row == 0 && st.scroll_col == 0);

    // 4. Insercion de referencia o rango justo en input_cursor desde Consola
    st.mode = SpreadsheetFocusMode::CommandConsole;
    st.input_buffer = "= SUM() + 5";
    st.input_cursor = 6; // entre '(' y ')'
    st.insertOrReplaceRefAtCursor("A1");
    assert(st.input_buffer == "= SUM(A1) + 5");
    assert(st.input_cursor == 8);
    st.insertOrReplaceRefAtCursor("A1:C3");
    assert(st.input_buffer == "= SUM(A1:C3) + 5");
    assert(st.input_cursor == 11);

    // 5. Retorno automatico a GridNavigation al editar desde la Matriz
    st.mode = SpreadsheetFocusMode::GridNavigation;
    st.input_buffer.clear();
    st.cursor_col = 0; st.cursor_row = 0;
    assert(texcel_tui::handleGridKey(st, '='));
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(st.return_to_grid_on_submit);
    assert(texcel_tui::handleConsoleKey(st, '7'));
    assert(texcel_tui::handleConsoleKey(st, '\n'));
    assert(st.mode == SpreadsheetFocusMode::GridNavigation);
    assert(e.getCell("A1").displayValue() == "7");

    // Con Tab manual permanece en CommandConsole tras Enter (Tab precarga la celda actual; Ctrl+U la limpia)
    texcel_tui::handleTabKey(st);
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(!st.return_to_grid_on_submit);
    assert(texcel_tui::handleConsoleKey(st, 21)); // Ctrl+U
    assert(texcel_tui::handleConsoleKey(st, '9'));
    assert(texcel_tui::handleConsoleKey(st, '\n'));
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(e.getCell("A1").displayValue() == "9");

    // 6. Clic en otra celda desde Modo 2 con valor pendiente lo guarda en la celda anterior
    st.cursor_col = 0; st.cursor_row = 0; // A1
    st.input_buffer = "42";
    st.input_cursor = 2;
    // Simular clic izquierdo en B1 (interior de la columna B, evitando la barra divisoria '│')
    TX b1_x = texcel_tui::TuiState::LEFT_X + 2 + texcel_tui::TuiState::ROW_HEADER_W + (texcel_tui::TuiState::CELL_WIDTH + 1) + 1;
    TX b1_y = texcel_tui::TuiState::TOP_Y + 3;
    std::string sgr_press = "0;" + std::to_string(b1_x + 1) + ";" + std::to_string(b1_y + 1);
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'M'));
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'm'));
    assert(e.getCell("A1").displayValue() == "42");
    assert(st.cursor_col == 1 && st.cursor_row == 0);
    assert(st.mode == SpreadsheetFocusMode::GridNavigation);

    // 7. Clic en otra celda desde Modo 2 editando formula inserta referencia y mantiene Modo 2
    st.cursor_col = 0; st.cursor_row = 0; // A1
    st.mode = SpreadsheetFocusMode::CommandConsole;
    st.input_buffer = "=SUM(";
    st.input_cursor = st.input_buffer.size();
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'M'));
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'm'));
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(st.input_buffer == "=SUM(B1");

    std::cout << "[PASS] test_tui_navigation\n";
    return 0;
}
