/**
 * @file main.cpp
 * @ingroup demos
 * @brief Punto de entrada principal que ejecuta las demostraciones del repositorio.
 */

#include "demos.h"

using namespace std;

/**
 * @brief Ejecuta secuencialmente todas las demostraciones de estructuras de datos.
 *
 * Compilar con `make` y ejecutar con `./main`.
 * @return Código de salida del programa (`0` si finaliza correctamente).
 */
int main() {
    DemoVector();
    DemoRaceCondition();
    DemoLinkedList();
    DemoHeap();
    DemoBinaryTree();
    DemoHashTable();
    DemoMerkleTree();
    DemoGotoxy();
    DemoBTree();
    return 0;
}