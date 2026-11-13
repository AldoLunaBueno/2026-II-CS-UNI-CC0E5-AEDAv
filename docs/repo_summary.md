# 🧭 Resumen Rápido del Repositorio (Repository Summary)

> **Propósito:** Proporcionar a cualquier agente de IA o desarrollador un contexto inmediato, conciso y de alto valor sobre la arquitectura, organización y contenido de cada archivo en el repositorio.

---

## 🏛️ Visión General del Proyecto

Biblioteca moderna en **C++23/26** de estructuras de datos avanzadas desarrollada para el curso **CC0E5 (AEDAv) - Universidad Nacional de Ingeniería (UNI)**.
- **Arquitectura basada en Mixins y Traits:** Composición flexible de capacidades (`Callable`, `Printable`, `Applicable`, `Reversible`, `Drawable`, etc.) sobre contenedores genéricos.
- **Seguridad Concurrente:** Contenedores protegidos mediante `std::recursive_mutex` con composición thread-safe y operaciones no bloqueantes entre vistas.
- **Vistas No Propietarias $O(1)$:** Desacoplamiento de iteración mediante `BaseView` y filtrado en tiempo de compilación con `FilterViewMixin`.
- **Iteradores CRTP y Recorrido de Euler:** Iteradores hacia adelante, reversos y un iterador de recorrido de Euler basado en máquina de estados con fases por bitmask (`Pre`, `In`, `Post`).
- **Motor de Renderizado Gráfico Terminal:** Trazado de estructuras (árboles y listas) sobre consola con caracteres UTF-8/ANSI (`Canvas2D`, layouts desacoplados y renderizadores).
- **Estructuras Criptográficas y Asociativas:** Árbol de Merkle con pruebas de autenticación (RFC 6962) y HashTable asociativa sobre árboles AVL.

---

## 📁 Índice y Resumen de Archivos

### 1. Núcleo Base y Tipos (`/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`types.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/types.h) | Definiciones de tipos fundamentales del sistema y portabilidad de arquitectura. | `TX` (`int`), `TXX` (`long long`), `Ref` (`long long`, identificador de referencia), `CW` (soporte unicode según arquitectura 16/32/64 bits). |
| [`foreach.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/foreach.h) | Algoritmos funcionales genéricos sobre rangos de iteradores y contenedores. | `::call(begin, end, func, args...)` (soporta lambdas `void` y predicados con retorno), `ApplyFunction`, `FirstThat`. |

---

### 2. Contenedores y Jerarquía Base (`containers/`)

#### 🧱 Infraestructura Base (`containers/core/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`container_core.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/core/container_core.h) | Abstracciones base para iterabilidad, mutabilidad y concurrencia. | `IterableCore<Traits>` (interfaz `begin`, `end`, `empty`, `mutex`), `ContainerCore<Traits>` (`m_mutex`, `copy_from`, `move_from`, `push_back`, `rebuild`). |
| [`general_container.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/core/general_container.h) | Ensamblador de contenedores genéricos y sistema de vistas $O(1)$. | `GeneralContainer<Traits, Mixins...>`, `BaseView<ViewTraits, Mixins...>`, conceptos `IsIterableMixin`, `IsViewMixin`, `FilterViewMixin`, traits `AscendingTraits`/`DescendingTraits`. |
| [`general_node.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/core/general_node.h) | Estructura base de almacenamiento de nodo y serialización elemental. | `GeneralNode<T>` (almacena par `m_value` y `m_ref`), `operator<<` y `operator>>` formato `(valor,ref)`. |
| [`general_iterator.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/core/general_iterator.h) | Plantilla base de iterador basada en CRTP (Curiously Recurring Template Pattern). | `GeneralIterator<Derived, T>`, operadores `operator*`, `operator==`, `operator!=`, punteros `m_ptr` y `m_pStart`. |
| [`mixins.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/core/mixins.h) | Conjunto de mixins modulares de consulta, modificación y visualización. | **Consulta:** `Callable` (`.call`), `Searchable` (`.FirstThat`), `Printable` (`operator<<`).<br/>**Contenedor:** `Applicable` (`.ApplyFunction` con `.rebuild`), `Reversible` (`rbegin`, `rend`, `.reversed`, `.rcall`), `Readable` (`operator>>`), `Drawable` (`.draw(layout, renderer)`). |

#### 📦 Contenedores Secuenciales y Asociativos (`containers/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`vector.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/vector.h) | Vector dinámico contiguo thread-safe redimensionable con soporte de vistas reversibles. | `BaseVector`, iteradores `VectorForwardIterator` y `VectorBackwardIterator`, traits `VectorAscTraits` y `VectorDescTraits`. |
| [`hash_table.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/hash_table.h) | Diccionario asociativo clave-valor implementado sobre `BaseAVLTree`. | `BaseHashTable`, `HashEntry<Key, Value>`, `operator[]`, `contains`, serialización con soporte de strings con espacios vía `std::quoted`. |

#### 🔗 Listas Enlazadas (`containers/lists/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`linked_list.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/lists/linked_list.h) | Lista simplemente enlazada con inserción ordenada o por posición. | `BaseLinkedList`, `LinkedListNode<T>`, iterador `LinkedListForwardIterator`, métodos `insert`, `find`, `remove`. |
| [`double_linked_list.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/lists/double_linked_list.h) | Lista doblemente enlazada con soporte bidireccional y vistas invertidas. | `BaseDoubleLinkedList`, `LinkedListBackwardIterator`, sobrescritura de `linkNode`, mixin `Reversible`. |
| [`circular_linked_list.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/lists/circular_linked_list.h) | Lista enlazada circular simple (`m_pTail->m_pNext = m_pRoot`). | `BaseCircularLinkedList`, mixin `Drawable` para renderizado ASCII/UTF-8. |
| [`circular_double_linked_list.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/lists/circular_double_linked_list.h) | Lista doblemente enlazada circular con enlace bidireccional cerrado en extremos. | `BaseCircularDoubleLinkedList`, combina por herencia virtual `BaseCircularLinkedList` y `BaseDoubleLinkedList`. |

#### 🌳 Árboles y Montículos (`containers/trees/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`binary_tree.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/trees/binary_tree.h) | Árbol binario de búsqueda (BST) con máquina de estados para recorridos de Euler en $O(1)$ memoria. | `BaseBinaryTree`, `BinaryTreeNode<T>`, iterador `BinaryTreeIterator<T, Order, Dir>`, fases bitmask `Phase::Pre/In/Post`, órdenes puros e híbridos (`PreInOrder`, etc.), `insert`, `remove`, `rebalance` virtual. |
| [`avl_tree.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/trees/avl_tree.h) | Árbol auto-balanceado AVL con algoritmo de rotación unificada. | `BaseAVLTree`, `rotate_up` (unifica rotaciones simples y dobles LL/RR/LR/RL), cálculo de factor de balance `getbf`. |
| [`heap.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/trees/heap.h) | Montículo binario (Min/Max Heap) implementado sobre `Vector`. | `BaseHeap`, `push`, `pop`, `top`, `heapify_up`, `heapify_down`, soporte de invariante `rebuild()` tras modificaciones in-place con `ApplyFunction`. |
| [`merkle_tree.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/containers/trees/merkle_tree.h) | Árbol de Merkle criptográfico con pruebas de inclusión y separación de dominios (RFC 6962). | `BaseMerkleTree`, `DefaultMerkleHasher` (prefijos `leaf:` y `node:`), `MerkleProofStep`, `get_proof(index)`, `verify_proof`. |

---

### 3. Visualización y Renderizado en Consola (`utilities/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`ansi_style.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/ansi_style.h) | Utilidades tipográficas y coloración ANSI header-only sin dependencias externas. | `Ansi::Style`, `Ansi::Color`, `Ansi::BgColor`, `StyledBuilder`, `colored(...)`, `styled(...)`, cálculo de longitud visible `visible_length(...)`. |
| [`print_colored.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/print_colored.h) | Salida coloreada en consola con detección automática de terminal vs archivo. | `PrintColored(os, value, mods...)`, `toAnsi(Color)`. |
| [`render.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/render.h) | Motor de renderizado gráfico y lienzo 2D UTF-8/ANSI. | `Canvas2D`, `ConsoleGlyphs`, `GraphRenderer`, `SvgGraphRenderer`, `ConsoleGraphRenderer` (estilos `Rounded`, `Sharp`, `DivisionSlash`, `Ascii`). |
| [`layouts.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts.h) | Cabecera unificada que agrupa todos los layouts de proyección 2D. | Incluye todos los headers en `utilities/layouts/`. |

#### 📐 Subsistema de Layouts (`utilities/layouts/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`visual_graph.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/visual_graph.h) | Representación geométrica y topológica agnóstica al contenedor. | `SimpleVector<T>`, `VisualNode`, `VisualEdge`, `VisualGraph`. |
| [`base_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/base_layout.h) | Clase base abstracta para layouts, formateo de nodos y normalización de coordenadas. | `DefaultNodeFormatter`, `BaseLayout<Formatter>`. |
| [`tree_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/tree_layout.h) | Base común para layouts de árboles y roles semánticos de nodos. | `TreeRole`, `TreeLayout<Formatter>`, `LayoutNode`. |
| [`vertical_tree_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/vertical_tree_layout.h) | Proyección vertical clásica de árboles binarios con cálculo de anchos por subárbol. | `VerticalTreeLayout<Formatter>`. |
| [`horizontal_tree_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/horizontal_tree_layout.h) | Proyección horizontal (rotada 90°) de árboles binarios. | `HorizontalTreeLayout<Formatter>`. |
| [`circular_list_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/circular_list_layout.h) | Disposición espacial en bucle para listas circulares simples y dobles. | `CircularListRole`, `CircularListLayout<Formatter>`. |
| [`merkle_tree_layout.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/utilities/layouts/merkle_tree_layout.h) | Layout especializado para árboles de Merkle incluyendo hojas de datos crudos y roles de auditoría. | `MerkleRole`, `MerkleTreeLayout<Formatter>`, `MerkleNode`. |

---

### 4. Puntos de Entrada y Demostraciones (`/`)

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`main.cpp`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/main.cpp) | Punto de entrada principal. Ejecuta la suite completa de demostraciones. | `main()` invocando `DemoVector`, `DemoRaceCondition`, `DemoLinkedList`, `DemoHeap`, `DemoBinaryTree`, `DemoHashTable`, `DemoMerkleTree`. |
| [`demos.h`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/demos.h) | Declaración de prototipos de las demostraciones interactivas. | Funciones de prueba de todas las estructuras y escenarios de concurrencia. |
| [`demos.cpp`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/demos.cpp) | Implementación de casos de uso, pruebas multi-hilo, serialización a `output/` y visualización gráfica. | Pruebas de mutación funcional, estrés multi-hilo (`std::thread`), serialización a disco, renderizado y verificación Merkle. |

---

### 5. Compilación, Automatización y Configuración

| Archivo | Rol / Propósito | Componentes Clave |
| :--- | :--- | :--- |
| [`Makefile`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/Makefile) | Automatización de compilación y ejecución GNU Make. | `g++ -std=c++23` sobre `main.cpp demos.cpp`; targets `all`, `main`, `run`, `pr-comment`, `clean`. |
| [`scripts/gen_pr_comment.sh`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/scripts/gen_pr_comment.sh) | Generador de Markdown formateado para Pull Requests de GitHub (`tmp/pr_comment.md`). | Convierte rutas relativas a URLs absolutas raw/blob de GitHub e inyecta anclas `<a id="..."></a>`. |
| [`.clangd`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/.clangd) | Configuración del servidor de lenguaje Clangd (LSP). | Establece estándares `-std=c++26` y warnings `-Wall`. |
| [`.gitignore`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/.gitignore) | Reglas de exclusión para Git. | Excluye binarios, objetos `.o`, temporales `tmp/`, salidas `output/` y archivos `.txt` generados. |
| [`.vscode/settings.json`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/.vscode/settings.json) | Configuración de entorno de trabajo para Visual Studio Code. | Configura niveles de tabla de contenidos (TOC) para Markdown (`levels 1..3`). |

---

### 6. Documentación y Diagramas (`docs/`)

| Archivo | Rol / Propósito |
| :--- | :--- |
| [`README.md`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/README.md) | Documentación arquitectónica exhaustiva: taxonomía de mixins, máquina de estados Euler, rebalanceo AVL, concurrencia y guías de uso. |
| [`docs/repo_summary.md`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/repo_summary.md) | Este resumen de contexto rápido para agentes y desarrolladores. |
| [`docs/images/avl_step_up.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/avl_step_up.svg) | Diagrama del mecanismo de rebalanceo AVL y rotación unificada `rotate_up`. |
| [`docs/images/euler_bitmask.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/euler_bitmask.svg) | Codificación de fases (`Pre`, `In`, `Post`) mediante máscaras de bits. |
| [`docs/images/euler_contour.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/euler_contour.svg) | Ilustración del recorrido perimetral/contorno del árbol binario. |
| [`docs/images/euler_state_machine.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/euler_state_machine.svg) | Transiciones de estados de la máquina del `BinaryTreeIterator`. |
| [`docs/images/euler_venn.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/euler_venn.svg) | Diagrama de Venn entre recorridos puros e híbridos. |
| [`docs/images/heap_rebuild.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/heap_rebuild.svg) | Restauración de la invariante de montículo con `rebuild()`. |
| [`docs/images/layout_hierarchy.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/layout_hierarchy.svg) | Jerarquía de clases del subsistema de layouts y proyección a `VisualGraph`. |
| [`docs/images/merkle_tree_proof.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/merkle_tree_proof.svg) | Diagrama del árbol de Merkle, separación de dominios y verificación mediante ruta de auditoría. |
| [`docs/images/mixin_filter.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/mixin_filter.svg) | Mecanismo de filtrado por metaprogramación `FilterViewMixin` para `BaseView`. |
| [`docs/images/render_pipeline.svg`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/docs/images/render_pipeline.svg) | Pipeline de visualización: Contenedor $\to$ Layout $\to$ `VisualGraph` $\to$ `Canvas2D` $\to$ Renderer. |

---

### 7. Datos y Salidas de Prueba (`Students.txt`, `output/*.txt`)

| Ubicación | Rol / Contenido |
| :--- | :--- |
| [`Students.txt`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/Students.txt) | Registro de estudiantes y códigos universitarios del curso AEDAv (UNI). |
| [`linkedlist.txt`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/linkedlist.txt), [`vector.txt`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/vector.txt), [`vector_str.txt`](file:///home/aldolunab/repos/2026-II-CS-UNI-CC0E5-AEDAv/vector_str.txt) | Archivos de prueba base en la raíz. |
| `output/*.txt` | Archivos generados y leídos durante la ejecución de `demos.cpp` (`vector.txt`, `vector_str.txt`, `linked_list.txt`, `double_linked_list.txt`, `circular_linked_list.txt`, `circular_double_linked_list.txt`, `binarytree_test.txt`, `avltree_test.txt`, `hashtable_test.txt`). |

---

## ⚡ Guía Rápida para Agentes

1. **Compilar y Ejecutar:** Ejecutar `make` compila `main.cpp` + `demos.cpp` con `-std=c++23`, corre el ejecutable y actualiza `tmp/pr_comment.md`.
2. **Para agregar un nuevo contenedor:** Heredar de `GeneralContainer<Traits, Mixins...>`, implementar `begin()`, `end()`, `push_back()`, `clear()`, y opcionalmente `rebuild()`.
3. **Para añadir un nuevo recorrido a árboles:** Extender `BinaryTreeIterator` configurando las combinaciones de `Order` y `Phase`.
4. **Para visualización de estructuras:** Componer el mixin `Drawable` junto a un layout de `utilities/layouts.h` y `ConsoleGraphRenderer`.
