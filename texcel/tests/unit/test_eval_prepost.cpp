#include "../../engine.h"
#include <cassert>
#include <iostream>

int main() {
    SpreadsheetEngine e;

    // Aritmetica basica, precedencia y parentesis con PrePostOrder in-place
    e.setCell("A1", "5");
    e.setCell("B1", "= A1 + 4");
    e.setCell("C1", "= (A1 + B1) * 2");
    assert(e.getCell("B1").displayValue() == "9");
    assert(e.getCell("C1").displayValue() == "28");

    // Funciones agregadas sobre rangos
    e.setCell("A2", "10");
    e.setCell("A3", "15");
    e.setCell("D1", "= SUM(A1:A3)");
    e.setCell("D2", "= AVG(A1:A3)");
    e.setCell("D3", "= MIN(A1:A3)");
    e.setCell("D4", "= MAX(A1:A3)");
    assert(e.getCell("D1").displayValue() == "30");
    assert(e.getCell("D2").displayValue() == "10");
    assert(e.getCell("D3").displayValue() == "5");
    assert(e.getCell("D4").displayValue() == "15");

    // Texto, concatenacion (&, CONCAT) y COUNTIF / COUNTIFS
    e.setCell("E1", "Hola");
    e.setCell("E2", "= E1 & \" \" & A1");
    e.setCell("E3", "= CONCAT(E1, \"!\", B1)");
    assert(e.getCell("E2").displayValue() == "Hola 5");
    assert(e.getCell("E3").displayValue() == "Hola!9");

    e.setCell("F1", "= COUNTIF(A1:A3, \">5\")");
    e.setCell("F2", "= COUNTIFS(A1:A3, \">=10\", A1:A3, \"<=15\")");
    assert(e.getCell("F1").displayValue() == "2");
    assert(e.getCell("F2").displayValue() == "2");

    // Validacion de tipos propagada en Phase::Pre (#VALUE!), #DIV/0! y #CYCLE!
    e.setCell("G1", "= E1 + 5");
    assert(e.getCell("G1").displayValue() == "#VALUE!");

    e.setCell("G2", "= A1 / 0");
    assert(e.getCell("G2").displayValue() == "#DIV/0!");

    e.setCell("H1", "= H2 + 1");
    e.setCell("H2", "= H1 + 1");
    assert(e.getCell("H2").displayValue() == "#CYCLE!");

    // MIN/MAX escalar/variadico, IF, comparaciones (=, <>, <, >) y TRUE en COUNTIFS
    e.setCell("J1", "= MIN(20, MAX(0, 25))");
    e.setCell("J2", "= MIN(20, MAX(0, -5))");
    e.setCell("J3", "= MAX(1, 0, 7, 3)");
    assert(e.getCell("J1").displayValue() == "20");
    assert(e.getCell("J2").displayValue() == "0");
    assert(e.getCell("J3").displayValue() == "7");

    e.setCell("K1", "= IF(A1 = 5, 100, 0)");
    e.setCell("K2", "= IF(A1 <> 5, 100, \"Vacio\")");
    e.setCell("K3", "= IF(0 = 1, \"No\", \"\")");
    assert(e.getCell("K1").displayValue() == "100");
    assert(e.getCell("K2").displayValue() == "Vacio");
    assert(e.getCell("K3").displayValue() == "");

    // Patron de calificacion con 2 filas de referencia (CORTE, TIPO) y TRUE
    e.setCell("P1", "PC1"); e.setCell("Q1", "PC1"); e.setCell("R1", "PC1");
    e.setCell("P2", "A");   e.setCell("Q2", "T");   e.setCell("R2", "CT");
    e.setCell("P5", "True"); e.setCell("Q5", "True"); e.setCell("R5", "True");
    e.setCell("E4", "PC1");
    e.setCell("E5", "=IF(COUNTIFS($P$1:$R$1, E$4, $P$2:$R$2, \"<>\")=0, \"\", MIN(20, MAX(0, 4 * COUNTIFS($P5:$R5, TRUE, $P$1:$R$1, E$4, $P$2:$R$2, \"A\") / MAX(1, COUNTIFS($P$1:$R$1, E$4, $P$2:$R$2, \"A\")) + 16 * COUNTIFS($P5:$R5, TRUE, $P$1:$R$1, E$4, $P$2:$R$2, \"T\") / MAX(1, COUNTIFS($P$1:$R$1, E$4, $P$2:$R$2, \"T\")) - 3 * COUNTIFS($P5:$R5, TRUE, $P$1:$R$1, E$4, $P$2:$R$2, \"CT\"))))");
    assert(e.getCell("E5").displayValue() == "17");

    std::cout << "[PASS] test_eval_prepost\n";
    return 0;
}
