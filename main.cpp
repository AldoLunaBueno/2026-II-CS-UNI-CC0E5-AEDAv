/**
 * @file main.cpp
 * @ingroup demos
 * @brief Punto de entrada principal que ejecuta las demostraciones del repositorio.
 */

#include "demos.h"
#include <string_view>

using namespace std;

/**
 * @brief Ejecuta secuencialmente todas las demostraciones de estructuras de datos,
 *        o abre la interfaz interactiva de hoja de cálculo si se pasa `--excel`.
 *
 * Compilar con `make` y ejecutar con `./main` (demos) o `make excel` / `./main --excel` (TUI interactiva).
 * @return Código de salida del programa (`0` si finaliza correctamente).
 */
int main(int argc, char* argv[]) {
    for (TX i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--excel" || arg == "-e" || arg == "excel") {
            RunInteractiveSpreadsheet();
            return 0;
        }
    }

    DemoVector();
    DemoRaceCondition();
    DemoLinkedList();
    DemoHeap();
    DemoBinaryTree();
    DemoHashTable();
    DemoMerkleTree();
    DemoGotoxy();
    DemoBTree();
    DemoSpreadsheet();
    return 0;
}