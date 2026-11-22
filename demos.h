/**
 * @file demos.h
 * @ingroup demos
 * @brief Declaraciones de las funciones de demostración de estructuras de datos y utilidades.
 */

#ifndef __DEMOS_H__
#define __DEMOS_H__
#include "types.h"
#include "containers/trees/heap.h"

/** @brief Demostración de operaciones y renderizado de `Vector`. */
void DemoVector();
/** @brief Demostración de contenedores personalizados y serialización. */
void DemoCustomContainer();
/** @brief Demostración de condición de carrera en operaciones concurrentes. */
void DemoRaceCondition();
/** @brief Demostración de listas enlazadas (simple, doble, circular) y su renderizado. */
void DemoLinkedList();
/** @brief Demostración de montículos (`Heap` / `MinHeap` / `MaxHeap`) y `rebuild()`. */
void DemoHeap();
/** @brief Demostración de árboles binarios, recorridos de Euler y `AVLTree`. */
void DemoBinaryTree();
/** @brief Demostración de tablas hash (`HashTable`) con encadenamiento. */
void DemoHashTable();
/** @brief Demostración de árboles de Merkle (`MerkleTree`) y pruebas de auditoría. */
void DemoMerkleTree();
/** @brief Demostración de posicionamiento directo en consola con `gotoxy`. */
void DemoGotoxy();
/** @brief Demostración de árbol B (`BTree` del profesor). */
void DemoBTree();
/** @brief Demostración animada de hoja de cálculo multi-ventana (`HashTable` + `BinaryTree` AST + `gotoxy`). */
void DemoSpreadsheet();
/** @brief Inicia la interfaz TUI interactiva de 4 ventanas para la hoja de cálculo. */
void RunInteractiveSpreadsheet();

#endif // __DEMOS_H__