#include "../../../src/texcel/texcel.h"
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
    assert(st.status_banner.find("C3:D3") != std::string::npos);
    texcel_tui::handleGridEscapeKey(st, "", 'B');    // Flecha abajo limpia seleccion y banner de sombreado
    assert(!st.hasCellSel() && st.status_banner.empty());
    texcel_tui::handleGridEscapeKey(st, "", 'A');    // Volver a C3
    texcel_tui::handleGridEscapeKey(st, "1;2", 'C'); // Re-sombrear C3:D3
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

    // 10. Escritura directa en celda sin Enter previo + boton [V ↻] de AST + modal [fx] y [?]
    st.mode = SpreadsheetFocusMode::GridNavigation;
    st.setInput();
    st.cursor = {0, 0, true};
    assert(texcel_tui::handleGridKey(st, 'q')); // 'q' ahora escribe directamente en la celda
    assert(st.mode == SpreadsheetFocusMode::CommandConsole && st.input_buffer == "q");
    assert(texcel_tui::handleConsoleKey(st, 'u'));
    assert(texcel_tui::handleConsoleKey(st, 'e'));
    assert(texcel_tui::handleConsoleKey(st, '\n'));
    assert(st.mode == SpreadsheetFocusMode::GridNavigation && e.getCell("A1").displayValue() == "que");

    // Clic en radio buttons ■ V / □ H del Inspector orienta el AST
    assert(!st.ast_horizontal);
    TX ast_h_x = st.right_x + 2 + 15;
    TX ast_v_x = st.right_x + 2 + 10;
    TX ast_btn_y = texcel_tui::TuiState::TOP_Y + 6;
    std::string ast_h_click = "0;" + std::to_string(ast_h_x + 1) + ";" + std::to_string(ast_btn_y + 1);
    std::string ast_v_click = "0;" + std::to_string(ast_v_x + 1) + ";" + std::to_string(ast_btn_y + 1);
    assert(texcel_tui::handleSgrMouse(st, ast_h_click, 'M'));
    assert(st.ast_horizontal);
    assert(texcel_tui::handleSgrMouse(st, ast_v_click, 'M'));
    assert(!st.ast_horizontal);

    // Chord Ctrl+X + A abre Ayuda, Ctrl+X + F abre Funciones
    st.chord_x = true;
    texcel_tui::handleChordXKey(st, 'a');
    assert(!st.chord_x && st.modal_kind == texcel_tui::ModalKind::Help);
    st.closeModal();
    st.chord_x = true;
    texcel_tui::handleChordXKey(st, 'f');
    assert(!st.chord_x && st.modal_kind == texcel_tui::ModalKind::Functions);
    st.closeModal();

    // Clic en [?] de la barra superior abre Ayuda
    TX help_btn_x = st.total_width - 10;
    std::string help_click = "0;" + std::to_string(help_btn_x + 1) + ";1";
    assert(texcel_tui::handleSgrMouse(st, help_click, 'M'));
    assert(st.modal_kind == texcel_tui::ModalKind::Help);
    st.closeModal();

    // Clic en [fx] abre Funciones
    TX fx_x = texcel_tui::TuiState::LEFT_X + st.left_w - 5;
    TX fx_y = texcel_tui::TuiState::BOT_Y + 2;
    std::string fx_click = "0;" + std::to_string(fx_x + 1) + ";" + std::to_string(fx_y + 1);
    assert(texcel_tui::handleSgrMouse(st, fx_click, 'M'));
    assert(st.modal_kind == texcel_tui::ModalKind::Functions);

    // Rueda del ratón hacia abajo selecciona AVG (índice 1)
    texcel_tui::ModalRect mr = st.modalRect();
    std::string wheel_down = "65;" + std::to_string(mr.x + 5) + ";" + std::to_string(mr.y + 5);
    assert(texcel_tui::handleSgrMouse(st, wheel_down, 'M'));
    assert(st.fx_idx == 1);

    // Tab cambia a Ayuda y vuelve a Funciones
    texcel_tui::handleModalKey(st, '\t');
    assert(st.modal_kind == texcel_tui::ModalKind::Help);
    texcel_tui::handleModalKey(st, '\t');
    assert(st.modal_kind == texcel_tui::ModalKind::Functions);

    // Enter inserta "= AVG(" desde Hoja y pasa a modo Edición
    texcel_tui::handleModalKey(st, '\n');
    assert(!st.isModalOpen());
    assert(st.mode == SpreadsheetFocusMode::CommandConsole);
    assert(st.input_buffer == "= AVG(");

    // Inserción en medio de una fórmula ya abierta (clic 1 selecciona, clic 2 inserta)
    st.setInput("= A1 + ");
    st.openModal(texcel_tui::ModalKind::Functions);
    st.fx_idx = 0; st.fx_scroll = 0;
    std::string row2_click = "0;" + std::to_string(mr.x + 6) + ";" + std::to_string(mr.y + 3 + 2 + 1); // MIN (idx 2)
    assert(texcel_tui::handleSgrMouse(st, row2_click, 'M')); // 1er clic: selecciona MIN
    assert(st.isModalOpen() && st.fx_idx == 2);
    assert(texcel_tui::handleSgrMouse(st, row2_click, 'M')); // 2do clic: inserta MIN(
    assert(!st.isModalOpen() && st.input_buffer == "= A1 + MIN(");

    // 11. Leyenda dinámica + smoke test de renderAll (4 ventanas + 3 modales)
    SpreadsheetEngine clean_e;
    std::ostringstream render_out;
    texcel_tui::TuiState st2(clean_e, render_out);
    st2.updateLayout();
    texcel_tui::renderAll(st2);
    std::string plain_empty = texcel_tui::stripAnsi(render_out.str());
    assert(plain_empty.find("Activa") != std::string::npos);
    assert(plain_empty.find("Referencia") == std::string::npos);
    assert(plain_empty.find("Afectada") == std::string::npos);

    clean_e.setCell("A1", "5");
    clean_e.setCell("B1", "= A1 + 4");
    st2.cursor = {1, 0, true}; // B1 activa -> A1 en Referencia
    render_out.str("");
    texcel_tui::renderAll(st2);
    std::string plain_ref = texcel_tui::stripAnsi(render_out.str());
    assert(plain_ref.find("Activa") != std::string::npos);
    assert(plain_ref.find("Referencia") != std::string::npos);

    for (auto mk : {texcel_tui::ModalKind::Sheets, texcel_tui::ModalKind::Functions, texcel_tui::ModalKind::Help}) {
        render_out.str("");
        st2.openModal(mk);
        texcel_tui::renderAll(st2);
        assert(!render_out.str().empty());
    }
    st2.closeModal();

    std::cout << "[PASS] test_tui_navigation\n";
    return 0;
}
