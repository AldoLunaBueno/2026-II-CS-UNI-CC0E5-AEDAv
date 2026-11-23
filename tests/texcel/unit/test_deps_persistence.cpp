#include "../../../texcel/engine.h"
#include <cassert>
#include <iostream>
#include <sstream>

int main() {
    SpreadsheetEngine e;

    // 1. Propagacion incremental en cascada via LinkedList
    e.setCell("A1", "5");
    e.setCell("B1", "= A1 + 4");
    e.setCell("D4", "= A1 + B1");
    assert(e.getCell("B1").displayValue() == "9");
    assert(e.getCell("D4").displayValue() == "14");

    e.setCell("A1", "8");
    assert(e.getCell("B1").displayValue() == "12");
    assert(e.getCell("D4").displayValue() == "20");

    // Verificar lista de dependientes de A1 (B1 y D4)
    auto deps_a1 = e.getCellDependents("A1");
    assert(deps_a1.size() == 2);

    // 2. Desuscripcion al sobrescribir una formula
    e.setCell("B1", "100");
    e.setCell("A1", "10");
    assert(e.getCell("B1").displayValue() == "100");
    assert(e.getCell("D4").displayValue() == "110");
    auto deps_a1_after = e.getCellDependents("A1");
    assert(deps_a1_after.size() == 1);
    assert(deps_a1_after[0].getValue() == "D4");

    // 3. Persistencia con HashTable Printable / Readable
    std::stringstream ss;
    ss << e.rawTable();

    SpreadsheetEngine restored;
    ss >> restored.rawTable();
    restored.recalculateAll();
    assert(restored.getCell("A1").displayValue() == "10");
    assert(restored.getCell("B1").displayValue() == "100");
    assert(restored.getCell("D4").displayValue() == "110");

    std::cout << "[PASS] test_deps_persistence\n";
    return 0;
}
