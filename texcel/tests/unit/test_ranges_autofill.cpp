#include "../../engine.h"
#include <cassert>
#include <iostream>

int main() {
    SpreadsheetEngine e;

    // 1. Limite ampliado A..ZZ (702 cols) x 1..999 filas
    assert(CellCoord::parse("AA10").valid);
    assert(CellCoord::parse("ZZ999").valid);
    assert(!CellCoord::parse("AAA1").valid);
    assert(!CellCoord::parse("A1000").valid);

    e.setCell("AA10", "5");
    e.setCell("ZZ999", "= AA10 * 3");
    assert(e.getCell("ZZ999").displayValue() == "15");

    // 2. Auto-Fill de formulas relativas y absolutas ($)
    e.setCell("A1", "10");
    e.setCell("A2", "20");
    e.setCell("A3", "30");
    e.setCell("C1", "100");
    e.setCell("B1", "= A1 + $C$1");

    TX filled = e.autoFillRange(1, 0, 1, 2); // B1:B3
    assert(filled == 2);
    assert(e.getCell("B2").raw_expression == "= A2 + $C$1");
    assert(e.getCell("B2").displayValue() == "120");
    assert(e.getCell("B3").raw_expression == "= A3 + $C$1");
    assert(e.getCell("B3").displayValue() == "130");

    // 3. Auto-Fill de serie numerica
    e.setCell("D1", "1");
    e.setCell("D2", "2");
    e.autoFillRange(3, 0, 3, 3); // D1:D4
    assert(e.getCell("D3").displayValue() == "3");
    assert(e.getCell("D4").displayValue() == "4");

    // 4. Borrado de rango rectangular con clearRange
    e.clearRange(1, 0, 1, 2); // B1:B3
    assert(e.getCell("B1").displayValue() == "");
    assert(e.getCell("B2").displayValue() == "");
    assert(e.getCell("B3").displayValue() == "");

    std::cout << "[PASS] test_ranges_autofill\n";
    return 0;
}
