#include "../../../texcel/texcel.h"
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
    assert(st.cursor.col == 701 && st.cursor.row == 499);
    assert(!st.hasCellSel());

    assert(st.goToCellOrRange("AA10:AC20"));
    assert(st.hasCellSel());
    assert(st.sel_a.col == 26 && st.sel_a.row == 9);
    assert(st.sel_b.col == 28 && st.sel_b.row == 19);

    assert(!st.goToCellOrRange("INVALIDO9999"));

    // 2. Doble clic en Name Box -> regreso a A1
    st.jumpToHome();
    assert(st.cursor.col == 0 && st.cursor.row == 0);
    assert(st.scroll_col == 0 && st.scroll_row == 0);
    assert(!st.hasCellSel());

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
    st.cursor = {0, 0, true};
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
    st.cursor = {0, 0, true}; // A1
    st.input_buffer = "42";
    st.input_cursor = 2;
    // Simular clic izquierdo en B1 (interior de la columna B, evitando la barra divisoria '│')
    TX b1_x = texcel_tui::TuiState::LEFT_X + 2 + texcel_tui::TuiState::ROW_HEADER_W + (texcel_tui::TuiState::CELL_WIDTH + 1) + 1;
    TX b1_y = texcel_tui::TuiState::TOP_Y + 3;
    std::string sgr_press = "0;" + std::to_string(b1_x + 1) + ";" + std::to_string(b1_y + 1);
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'M'));
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'm'));
    assert(e.getCell("A1").displayValue() == "42");
    assert(st.cursor.col == 1 && st.cursor.row == 0);
    assert(st.mode == SpreadsheetFocusMode::GridNavigation);

    // 7. Clic en otra celda desde Modo 2 editando formula inserta referencia y mantiene Modo 2
    st.cursor = {0, 0, true}; // A1
    st.mode = SpreadsheetFocusMode::CommandConsole;
    st.input_buffer = "=SUM(";
    st.input_cursor = st.input_buffer.size();
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'M'));
    assert(texcel_tui::handleSgrMouse(st, sgr_press, 'm'));
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(st.input_buffer == "=SUM(B1");

    // 8. Exclusividad de SelectionKind y edicion via Name Box (beginNameBoxEdit / handleNameBoxKey)
    st.sel_kind = texcel_tui::SelectionKind::Fill;
    assert(st.hasFillSel() && !st.hasCellSel() && !st.hasTextSel());
    st.clearSelection();
    assert(!st.hasFillSel() && !st.hasCellSel() && !st.hasTextSel());

    st.beginNameBoxEdit();
    assert(st.namebox_editing && st.mode == SpreadsheetFocusMode::GridNavigation);
    for (char c : std::string("c3")) texcel_tui::handleNameBoxKey(st, c);
    texcel_tui::handleNameBoxKey(st, '\n');
    assert(!st.namebox_editing && st.cursor.col == 2 && st.cursor.row == 2);

    // 9. Secuencias CSI puras (Shift+Flechas, Supr, historial ↑/↓) y Bracketed Paste
    texcel_tui::handleGridEscapeKey(st, "1;2", 'C'); // Shift+Right desde C3 -> C3:D3
    assert(st.hasCellSel() && st.selectedRangeStr() == "C3:D3");
    e.setCell("C3", "11"); e.setCell("D3", "22");
    texcel_tui::handleGridEscapeKey(st, "3", '~');   // Supr borra el rango seleccionado
    assert(!st.hasCellSel() && e.getCell("C3").displayValue() == "" && e.getCell("D3").displayValue() == "");

    st.mode = SpreadsheetFocusMode::CommandConsole;
    st.setInput("= ");
    texcel_tui::applyBracketedPaste(st, "A1 + 5\nignorado");
    assert(st.input_buffer == "= A1 + 5");
    texcel_tui::handleConsoleEscapeKey(st, "", 'H'); // Home
    assert(st.input_cursor == 0);
    texcel_tui::handleConsoleEscapeKey(st, "", 'F'); // End
    assert(st.input_cursor == st.input_buffer.size());

    // 10. Smoke test de renderAll (4 ventanas + AST + modal) sobre ostringstream
    dummy_os.str("");
    st.modal_open = true;
    texcel_tui::renderAll(st);
    assert(!dummy_os.str().empty());
    st.modal_open = false;

    std::cout << "[PASS] test_tui_navigation\n";
    return 0;
}
