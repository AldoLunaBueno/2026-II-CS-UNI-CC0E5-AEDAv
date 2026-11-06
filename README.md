<!-- omit in toc -->
# 2026-II-CS-UNI-CC0E5-AEDAv

- [🏛️ I. Arquitectura del Repositorio y Vistas de Subsistemas](#️-i-arquitectura-del-repositorio-y-vistas-de-subsistemas)
  - [1. Vista General del Sistema](#1-vista-general-del-sistema)
  - [2. Jerarquía de Contenedores, Vistas y Sistema de Mixins](#2-jerarquía-de-contenedores-vistas-y-sistema-de-mixins)
  - [3. Sistema de Iteradores, Nodos y Recorridos](#3-sistema-de-iteradores-nodos-y-recorridos)
  - [4. Sistema de Traits, Políticas de Orden y Desacoplamiento de Vistas](#4-sistema-de-traits-políticas-de-orden-y-desacoplamiento-de-vistas)
  - [5. Patrón Template Method en Árboles (BinaryTree / AVLTree), Invariante Rebuild y Renderizado](#5-patrón-template-method-en-árboles-binarytree--avltree-invariante-rebuild-y-renderizado)
- [⚙️ II. Detalle de Aspectos Específicos](#️-ii-detalle-de-aspectos-específicos)
  - [1. Máquina de Estados del Recorrido de Euler por Fases](#1-máquina-de-estados-del-recorrido-de-euler-por-fases)
  - [2. Desacoplamiento Estructural: Separación de Contenedor y Recorrido mediante Vistas (BaseView)](#2-desacoplamiento-estructural-separación-de-contenedor-y-recorrido-mediante-vistas-baseview)
  - [3. Mecanismo de Rebalanceo AVL por Rotación Unificada (rotate\_up)](#3-mecanismo-de-rebalanceo-avl-por-rotación-unificada-rotate_up)
  - [4. Invariante de Reconstrucción de Contenedores (rebuild)](#4-invariante-de-reconstrucción-de-contenedores-rebuild)
  - [5. Subsistema de Renderizado ASCII en Consola (sketch y ConsoleGraphRenderer)](#5-subsistema-de-renderizado-ascii-en-consola-sketch-y-consolegraphrenderer)
  - [6. Guía de Uso y Demostración Práctica de la API](#6-guía-de-uso-y-demostración-práctica-de-la-api)



---

## 🏛️ I. Arquitectura del Repositorio y Vistas de Subsistemas

### 1. Vista General del Sistema

```mermaid
flowchart LR
    TRAITS["<b>1. TRAITS</b><br/>Orden (Asc/Desc) +<br/>Estructura (Node, Iter) +<br/>Vistas (ViewTraits)"]
    MIXINS["<b>2. MIXINS & VISTAS</b><br/>Consulta (Callable, Printable,...)<br/>Contenedor (Applicable, Reversible,...)<br/>BaseView + FilterViewMixin"]
    CONT["<b>3. CONTENEDORES</b><br/>IterableCore y ContainerCore<br/>Vector, Heap, BinaryTree, AVL y Listas"]
    ITER["<b>4. ITERADORES</b><br/>CRTP GeneralIterator<br/>(Forward / Backward / Euler)"]
    NODOS["<b>5. NODOS</b><br/>GeneralNode, LinkedListNode,<br/>BinaryTreeNode (m_height)"]
    LIBRES["<b>6. LIBRES & UTILS</b><br/>::call, operator&lt;&lt;,&gt;&gt;,<br/>ConsoleGraphRenderer"]

    TRAITS ==>|"Inyecta tipos, Compare y View"| CONT
    MIXINS ==>|"Ensambla capacidades y vistas"| CONT
    CONT ==>|"begin() / end() / rbegin() / traverse()"| ITER
    ITER ==>|"Apuntan y recorren"| NODOS
    LIBRES -.->|"Operan vía iteradores / sketch()"| CONT

    classDef c1 fill:#E8EAF6,stroke:#3949AB,color:#111,stroke-width:2px;
    classDef c2 fill:#FCE4EC,stroke:#C2185B,color:#111,stroke-width:2px;
    classDef c3 fill:#E0F2F1,stroke:#00897B,color:#111,stroke-width:2px;
    classDef c4 fill:#FFF3E0,stroke:#EF6C00,color:#111,stroke-width:2px;
    classDef c5 fill:#E8F5E9,stroke:#2E7D32,color:#111,stroke-width:2px;
    classDef c6 fill:#ECEFF1,stroke:#546E7A,color:#111,stroke-width:2px;

    class CONT c1
    class MIXINS c2
    class ITER c3
    class NODOS c4
    class TRAITS c5
    class LIBRES c6
```

### 2. Jerarquía de Contenedores, Vistas y Sistema de Mixins

```mermaid
flowchart TB
    IC["<b>IterableCore#lt;Traits#gt;</b><br/>Base de iteración y sincronización:<br/>virtual mutex(), begin(), end(), empty()"]

    CC["<b>ContainerCore#lt;Traits#gt;</b><br/>Base de mutabilidad y ciclo de vida:<br/>m_mutex recursivo, copy_from(), move_from(),<br/>clear(), push_back(), virtual rebuild()"]

    IC --> CC

    subgraph MIX_Q["Mixins de Consulta / Vistas (Heredan de IterableCore)"]
        direction TB
        CAL["<b>Callable#lt;Traits#gt;</b><br/><i>.call(func, args...)</i>"]
        SEA["<b>Searchable#lt;Traits#gt;</b><br/><i>.FirstThat(...)</i>"]
        PRI["<b>Printable#lt;Traits#gt;</b><br/><i>friend operator#lt;#lt;</i>"]
        CAL --> SEA
    end

    subgraph MIX_C["Mixins de Contenedor (Heredan de ContainerCore)"]
        direction TB
        APP["<b>Applicable#lt;Traits#gt;</b><br/><i>.ApplyFunction(...) ➔ .rebuild()</i>"]
        REV["<b>Reversible#lt;Traits#gt;</b><br/><i>rbegin(), rend(), .reversed(), .rcall(...)</i>"]
        REA["<b>Readable#lt;Traits#gt;</b><br/><i>friend operator#gt;#gt;</i>"]
        DRW["<b>Drawable#lt;Traits#gt;</b><br/><i>.sketch(), .draw(...)</i>"]
    end

    IC -. "virtual base" .-> MIX_Q
    CC -. "virtual base" .-> MIX_C
    CAL -. "virtual base" .-> APP

    subgraph VIEWS["Sistema de Vistas Ligeras O(1)"]
        direction TB
        BV["<b>BaseView#lt;ViewTraits, Mixins...#gt;</b><br/>Iteración sobre rango [begin, end) no propietario<br/><i>Filtra mixins con concept FilterViewMixin</i>"]
    end

    IC --> BV
    MIX_Q -. "heredado vía FilterViewMixin" .-> BV

    GC["<b>GeneralContainer#lt;BaseTraits, Mixins...#gt;</b><br/>Virtual inheritance (Concept IsContainerCore)<br/>Define ContainerTraits::View = BaseView"]

    CC --> GC
    MIX_Q -. "herencia múltiple" .-> GC
    MIX_C -. "herencia múltiple" .-> GC

    subgraph CONT_STRUCT["Contenedores Concretos"]
        direction TB
        V["<b>Vector#lt;Traits#gt;</b><br/><i>BaseVector</i>"]
        
        H["<b>Heap#lt;Traits#gt;</b><br/><i>BaseHeap (override rebuild)</i>"]
        MH["<b>MinHeap / MaxHeap</b><br/><i>(Heap.h)</i>"]
        
        LL["<b>LinkedList#lt;Traits#gt;</b><br/><i>BaseLinkedList (virtual linkNode)</i>"]
        LC["<b>CircularLinkedList#lt;Traits#gt;</b>"]
        LDE["<b>DoubleLinkedList#lt;Traits#gt;</b>"]
        LDEC["<b>CircularDoubleLinkedList#lt;Traits#gt;</b>"]
        
        BT["<b>BinaryTree#lt;Traits#gt;</b><br/><i>BaseBinaryTree<br/>.traverse#lt;Order, Dir#gt;() ➔ EulerView<br/>(insert, remove, connect, virtual rebalance)</i>"]
        AVL["<b>AVLTree#lt;Traits#gt;</b><br/><i>BaseAVLTree<br/>(override rebalance, rotate_up, connect)</i>"]

        GC --> V
        GC --> H
        GC --> LL
        GC --> BT

        H --> MH
        H -. "m_data" .-> V

        LL --> LC
        LL --> LDE
        LC --> LDEC
        LDE --> LDEC

        BT --> AVL
    end

    REV -. ".reversed()" .-> BV
    BT -. ".traverse#lt;Ord, Dir#gt;()" .-> BV

    classDef core fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef mix_q fill:#FCE4EC,stroke:#C2185B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef mix_c fill:#FFF3E0,stroke:#E65100,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef view fill:#E1F5FE,stroke:#0288D1,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef vec fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef lista fill:#FFF8E1,stroke:#FF8F00,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef arbol fill:#E8F5E9,stroke:#2E7D32,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class IC,CC,GC core
    class CAL,SEA,PRI mix_q
    class APP,REV,REA,DRW mix_c
    class BV view
    class V,H,MH vec
    class LL,LC,LDE,LDEC lista
    class BT,AVL arbol

    style MIX_Q fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style MIX_C fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style VIEWS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style CONT_STRUCT fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

### 3. Sistema de Iteradores, Nodos y Recorridos

```mermaid
flowchart TB
    subgraph ALGO["Funciones de Recorrido (foreach.h)"]
        direction TB
        CALL["::call(begin, end, func, args...)<br/><i>Aplica func o busca con predicado</i>"]
        APP_FN["::ApplyFunction(begin, end, func, args...)"]
        FIRST["::FirstThat(begin, end, pred, args...)"]
        
        APP_FN --> CALL
        FIRST --> CALL
    end

    subgraph ENUMS["Configuración de Recorrido de Euler (detail::)"]
        direction TB
        PH["<b>Phase</b><br/>Pre (0b100), In (0b010), Post (0b001)"]
        ORD["<b>Order</b><br/>Puros: PreOrder, InOrder, PostOrder<br/>Híbridos: PreIn, PrePost, InPost, PreInPost"]
        DIR["<b>Direction</b><br/>Asc (0) | Desc (1)"]
        PH -->|"suma de fases (bits)"| ORD
    end

    subgraph ITER["Jerarquía de Iteradores (GeneralIterator.h)"]
        direction TB
        GI["<b>GeneralIterator#lt;Derived, T#gt;</b><br/>CRTP: m_ptr, m_pStart, operator*, ==, !="]
        
        VFI["VectorForwardIterator#lt;T#gt;<br/><i>operator++ (++m_ptr)</i>"]
        VBI["VectorBackwardIterator#lt;T#gt;<br/><i>operator++ (--m_ptr)</i>"]
        
        LLFI["LinkedListForwardIterator#lt;T#gt;<br/><i>Avanza por m_pNext (soporta ciclos)</i>"]
        LLBI["LinkedListBackwardIterator#lt;T#gt;<br/><i>Retrocede por m_pPrev (soporta ciclos)</i>"]
        
        BTI["BinaryTreeIterator#lt;T, Order, Dir#gt;<br/><i>Máquina de estados del recorrido de Euler<br/>step_until_phase() (14 recorridos sobre demanda)</i>"]

        GI --> VFI
        GI --> VBI
        GI --> LLFI
        GI --> LLBI
        GI --> BTI
    end

    subgraph NODOS["Jerarquía de Nodos (GeneralNode.h)"]
        direction TB
        GN["<b>GeneralNode#lt;T#gt;</b><br/>m_value (T), m_ref (Ref)<br/>getValue(), getRef(), value(), ref(), stream I/O"]
        
        LLN["<b>LinkedListNode#lt;T#gt;</b><br/>m_pNext, m_pPrev<br/><i>(Singly, Circular y Doubly)</i>"]
        
        BTN["<b>BinaryTreeNode#lt;T#gt;</b><br/>m_pChild[2] (izq/der), m_parent, m_height<br/><i>(BinaryTree y AVLTree)</i>"]

        GN --> LLN
        GN --> BTN
    end

    CALL ==>|"opera mediante"| GI
    ORD -. "parametriza Order" .-> BTI
    DIR -. "parametriza Dir" .-> BTI
    PH -. "controla m_phase" .-> BTI
    VFI -. "itera" .-> GN
    VBI -. "itera" .-> GN
    LLFI -. "itera" .-> LLN
    LLBI -. "itera" .-> LLN
    BTI -. "itera" .-> BTN

    classDef base fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef vec fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef lista fill:#FFF3E0,stroke:#EF6C00,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef arbol fill:#E8F5E9,stroke:#2E7D32,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef libre fill:#ECEFF1,stroke:#546E7A,stroke-dasharray:5 4,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class GI,GN base
    class VFI,VBI vec
    class LLFI,LLBI,LLN lista
    class BTI,BTN,PH,ORD,DIR arbol
    class CALL,APP_FN,FIRST libre

    style ALGO fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style ENUMS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style ITER fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style NODOS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

### 4. Sistema de Traits, Políticas de Orden y Desacoplamiento de Vistas

```mermaid
flowchart TB
    subgraph ORDEN["Políticas de Comparación (GeneralContainer.h)"]
        direction TB
        DT["<b>DefaultTraits#lt;T, Compare#gt;</b><br/>value_type = T, Compare = _Compare"]
        ASC["<b>AscendingTraits#lt;T#gt;</b><br/>Compare = std::less#lt;T#gt;"]
        DESC["<b>DescendingTraits#lt;T#gt;</b><br/>Compare = std::greater#lt;T#gt;"]
        DT --> ASC
        DT --> DESC
    end

    subgraph STRUCT["Políticas de Estructura (Node + Iteradores)"]
        direction TB
        VT["<b>VectorTraits#lt;T#gt;</b><br/>Node: GeneralNode<br/>Iter: VectorForward/Backward"]
        HT["<b>HeapTraits#lt;T#gt;</b><br/>Node: GeneralNode<br/>Iter: VectorForward"]
        LLT["<b>LinkedListTraits#lt;T#gt;</b><br/>Node: LinkedListNode<br/>Iter: LinkedListForward"]
        DLLT["<b>DoubleLinkedListTraits#lt;T#gt;</b><br/>Agrega: LinkedListBackward"]
        BTT["<b>BinaryTreeTraits#lt;T#gt;</b><br/>Node: BinaryTreeNode<br/>ForwardIter: InOrder Asc<br/>BackwardIter: InOrder Desc"]
        VWT["<b>ViewTraits#lt;NodeT, Iter#gt;</b><br/>value_type = NodeT::value_type<br/>Node = NodeT, Forward/Backward = Iter"]

        LLT --> DLLT
    end

    subgraph COMBINED["Traits Compuestos (Orden + Estructura)"]
        direction TB
        V_TR["VectorAscTraits / VectorDescTraits"]
        H_TR["HeapAscTraits / HeapDescTraits"]
        LL_TR["LinkedListAscTraits / LinkedListDescTraits"]
        DLL_TR["DoubleLinkedListAscTraits / DoubleLinkedListDescTraits"]
        BT_TR["BinaryTreeAscTraits#lt;T#gt;<br/>BinaryTreeDescTraits#lt;T#gt;"]
        CTR["<b>ContainerTraits#lt;BaseTraits, Mixins...#gt;</b><br/>template View = BaseView#lt;ViewTraits, Mixins...#gt;"]
    end

    subgraph USERS["Contenedores y Vistas Parametrizadas"]
        direction TB
        U_V["Vector#lt;VectorAscTraits#lt;T#gt;#gt;"]
        U_H["Heap#lt;HeapAscTraits#lt;T#gt;#gt;"]
        U_LL["LinkedList / CircularLinkedList"]
        U_DLL["DoubleLinkedList / CircularDoubleLinkedList"]
        U_BT["BinaryTree#lt;BinaryTreeAscTraits#lt;T#gt;#gt;"]
        U_AVL["AVLTree#lt;BinaryTreeAscTraits#lt;T#gt;#gt;"]
        U_VIEW["EulerView / ReverseView<br/><i>BaseView en O(1)</i>"]
    end

    ASC --> COMBINED
    DESC --> COMBINED
    VT --> V_TR
    HT --> H_TR
    LLT --> LL_TR
    DLLT --> DLL_TR
    BTT --> BT_TR

    V_TR ==> U_V
    H_TR ==> U_H
    LL_TR ==> U_LL
    DLL_TR ==> U_DLL
    BT_TR ==> U_BT
    BT_TR ==> U_AVL
    CTR -. "genera" .-> U_VIEW
    VWT -. "alimenta" .-> CTR

    classDef orden fill:#F3E5F5,stroke:#8E24AA,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef struct fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef comb fill:#FFF9C4,stroke:#FBC02D,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef user fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class DT,ASC,DESC orden
    class VT,HT,LLT,DLLT,BTT,VWT struct
    class V_TR,H_TR,LL_TR,DLL_TR,BT_TR,CTR comb
    class U_V,U_H,U_LL,U_DLL,U_BT,U_AVL,U_VIEW user

    style ORDEN fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style STRUCT fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style COMBINED fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style USERS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

### 5. Patrón Template Method en Árboles (BinaryTree / AVLTree), Invariante Rebuild y Renderizado

```mermaid
flowchart LR
    subgraph BST["BaseBinaryTree (binarytree.h)"]
        direction TB
        INS["<b>insert(value, ref)</b><br/>1. lock_guard(m_mutex)<br/>2. p = internalInsert(...)<br/>3. rebalance(p)"]
        REM["<b>remove(node)</b><br/>1. lock_guard(m_mutex)<br/>2. p = internalRemove(node)<br/>3. rebalance(p)"]
        I_INS["<b>internalInsert(...)</b><br/>Inserta hoja BST y<br/>retorna m_parent"]
        I_REM["<b>internalRemove(node)</b><br/>Reemplaza con sucesor (2 hijos),<br/>reconecta (0/1 hijo) y retorna m_parent"]
        CONN_B["<b>connect(parent, child, d)</b><br/>Enlaza m_pChild[d] / m_pRoot<br/>y actualiza child->m_parent"]
        REB_B["<b>virtual rebalance(node)</b><br/><i>No-op {} en BST simple</i>"]
        TRAV["<b>traverse#lt;Order, Dir#gt;()</b><br/>Construye EulerView en O(1)"]
        SK["<b>sketch()</b><br/>Genera VisualGraph por niveles"]

        INS --> I_INS
        INS --> REB_B
        REM --> I_REM
        REM --> REB_B
        I_REM --> CONN_B
    end

    subgraph AVL_SUB["BaseAVLTree (avltree.h)"]
        direction TB
        REB_A["<b>rebalance(node) override</b><br/>Sube por m_parent actualizando<br/>m_height y evaluando getbf(z), getbf(y)"]
        ROT["<b>rotate_up(node)</b><br/>Rotación unificada (simple = 1x y,<br/>doble = 2x x) usando dp y dg"]
        CONN_A["<b>connect(parent, child, d)</b><br/>1. Parent::connect(...)<br/>2. parent->m_height = calcHeight(parent)"]

        REB_A --> ROT
        ROT --> CONN_A
    end

    subgraph CORE_REBUILD["Invariante Rebuild (ContainerCore.h / Heap.h)"]
        direction TB
        APP_HOOK["<b>Applicable::ApplyFunction(...)</b><br/>1. this->call(...)<br/>2. this->rebuild()"]
        REB_CORE["<b>ContainerCore::rebuild()</b><br/><i>virtual no-op por defecto</i>"]
        REB_HEAP["<b>BaseHeap::rebuild() override</b><br/>Aplica HeapifyUp para restaurar<br/>el invariante del montículo"]
        APP_HOOK --> REB_CORE
        REB_CORE -. "override" .-> REB_HEAP
    end

    subgraph REND["Subsistema de Renderizado (render.h)"]
        direction TB
        VG["<b>VisualGraph</b><br/>Nodos con coords (x,y), aristas izq/der"]
        GR["<b>ConsoleGraphRenderer</b><br/>Lienzo ASCII con ramas diagonales (/ y \)"]
        VG --> GR
    end

    REB_B -. "override en AVL" .-> REB_A
    CONN_A ==>|"reutiliza enlace"| CONN_B
    SK ==>|"alimenta a"| VG

    classDef bst fill:#E8F5E9,stroke:#2E7D32,color:#1A1A1A,stroke-width:1.5px;
    classDef avl fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px;
    classDef rnd fill:#ECEFF1,stroke:#546E7A,color:#1A1A1A,stroke-width:1.5px;
    classDef reb fill:#FFF3E0,stroke:#EF6C00,color:#1A1A1A,stroke-width:1.5px;

    class INS,REM,I_INS,I_REM,CONN_B,REB_B,TRAV,SK bst
    class REB_A,ROT,CONN_A avl
    class VG,GR rnd
    class APP_HOOK,REB_CORE,REB_HEAP reb

    style BST fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style AVL_SUB fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style REND fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style CORE_REBUILD fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

---

## ⚙️ II. Detalle de Aspectos Específicos

### 1. Máquina de Estados del Recorrido de Euler por Fases

#### A. ¿Cómo avanza el Recorrido de Euler por el contorno de un árbol?

El iterador [`BinaryTreeIterator<T, Order, Dir>`](containers/binarytree.h) modela el recorrido como una caminata continua por el **contorno exterior** del árbol. Al rodear el perímetro de cualquier subárbol, el iterador toca cada nodo exactamente en **3 momentos (fases)**:
- **🟢 Fase `0` (`FirstPhase` = `Phase::Pre` = `0b100`):** al bajar por el costado izquierdo del nodo (antes de visitar su subárbol izquierdo `I`).
- **🟡 Fase `1` (`Phase::In` = `0b010`):** al pasar por debajo del nodo (después de volver de `I` y antes de bajar a su subárbol derecho `D`).
- **🔴 Fase `2` (`LastPhase` = `Phase::Post` = `0b001`):** al subir por el costado derecho del nodo (después de volver de `D`, listo para regresar a su padre `P`).

La caminata completa genera la traza maestra del recorrido de Euler (`PreInPostOrder`), y tanto los recorridos **puros** (como `InOrder`) como los **híbridos** (como `PrePostOrder`) son **subsecuencias** filtradas de esta secuencia completa:

![Contorno del Recorrido de Euler](docs/euler_contour.svg)

---

#### B. Máquina de Estados del Nodo Actual (`step_until_phase`)

En cada paso del bucle `do-while` de `step_until_phase()`, el nodo actual (`m_ptr`) evalúa su fase `(0)`, `(1)` o `(2)` y decide si **salta a un nodo vecino** (`I`, `D`, `P ➔ I`, `P ➔ D`, o `fin`) o si **avanza de fase dentro del mismo nodo**:

```mermaid
flowchart TB
    FIN(["🏁 <b>fin (m_ptr = nullptr)</b><br/><i>Termina el recorrido y resetea a fase 0</i>"])

    subgraph PD["Padre desde rama Der (P ➔ D)<br/><i>m_ptr == parent->m_pChild[1-Dir]</i>"]
        PD_2(("<b>(2)</b><br/>LastPhase"))
    end

    subgraph PI["Padre desde rama Izq (P ➔ I)<br/><i>m_ptr == parent->m_pChild[Dir]</i>"]
        PI_1(("<b>(1)</b><br/>Phase::In"))
    end

    subgraph ACTUAL["📍 Nodo Actual (m_ptr)"]
        direction LR
        F0(("<b>(0)</b><br/>FirstPhase<br/><i>Pre (0b100)</i>"))
        F1(("<b>(1)</b><br/>Phase::In<br/><i>In (0b010)</i>"))
        F2(("<b>(2)</b><br/>LastPhase<br/><i>Post (0b001)</i>"))

        F0 ==>|"no (sin hijo I)"| F1
        F1 ==>|"no (sin hijo D)"| F2
    end

    subgraph HI["Hijo Izquierdo (I)<br/><i>m_pChild[Dir]</i>"]
        I_0(("<b>(0)</b><br/>FirstPhase"))
    end

    subgraph HD["Hijo Derecho (D)<br/><i>m_pChild[1-Dir]</i>"]
        D_0(("<b>(0)</b><br/>FirstPhase"))
    end

    F0 ==>|"sí (existe I)"| I_0
    F1 ==>|"sí (existe D)"| D_0
    F2 ==>|"sí (sube por izq)"| PI_1
    F2 ==>|"sí (sube por der)"| PD_2
    F2 ==>|"no (sin padre: raíz)"| FIN

    classDef f0 fill:#C8E6C9,stroke:#2E7D32,color:#111,stroke-width:2px;
    classDef f1 fill:#FFF9C4,stroke:#FBC02D,color:#111,stroke-width:2px;
    classDef f2 fill:#FFCCBC,stroke:#D84315,color:#111,stroke-width:2px;
    classDef finStyle fill:#ECEFF1,stroke:#455A64,color:#111,stroke-width:2px;

    class F0,I_0,D_0 f0
    class F1,PI_1 f1
    class F2,PD_2 f2
    class FIN finStyle

    style ACTUAL fill:#E8EAF6,stroke:#3949AB,stroke-width:2px
    style HI fill:#8080800F,stroke:#9E9E9E,stroke-dasharray:5 4
    style HD fill:#8080800F,stroke:#9E9E9E,stroke-dasharray:5 4
    style PI fill:#8080800F,stroke:#9E9E9E,stroke-dasharray:5 4
    style PD fill:#8080800F,stroke:#9E9E9E,stroke-dasharray:5 4
```

---

#### C. Puente entre Fases y Órdenes: Codificación en Máscara Binaria

Numerar las fases como `0`, `1` y `2` es un recurso didáctico para distinguir los tres contactos al rodear un nodo, pero en el código ([`containers/binarytree.h`](containers/binarytree.h)) cada fase es una potencia de 2 que ocupa **una posición de bit exclusiva** dentro de un número binario de 3 cifras (`Phase::Pre = 0b100`, `Phase::In = 0b010`, `Phase::Post = 0b001`). Así, cualquier orden de recorrido [`Order`](containers/binarytree.h) funciona como un panel de 3 interruptores donde cada cifra binaria **enciende (`1`)** o **apaga (`0`)** la parada del iterador en esa fase mediante `while (m_ptr && !(m_phase & Ord))`:

![Puente entre Fases y Máscara Binaria](docs/euler_bitmask.svg)

---

#### D. Composición por Máscara de Bits: Diagrama de Venn de los 14 Recorridos

Al combinar los 3 bits de `{Phase::Pre, Phase::In, Phase::Post}`, las $2^3 - 1 = 7$ regiones no vacías de intersección definen los 3 órdenes **Puros** y los 4 órdenes **Híbridos** (con el recorrido de Euler completo `0b111` en la intersección triple central), que multiplicados por las 2 direcciones (`Asc` y `Desc`) producen los **14 recorridos**:

![Diagrama de Venn de los 14 Recorridos](docs/euler_venn.svg)

### 2. Desacoplamiento Estructural: Separación de Contenedor y Recorrido mediante Vistas (BaseView)

#### A. Motivación y Principio de Responsabilidad Única

En diseños previos, el orden de recorrido formaba parte del parámetro de plantilla del árbol:
```cpp
// ❌ Diseño anterior acoplado:
template <typename T, TraversalOrder Order = InOrder>
struct BinaryTreeAscTraits;

BinaryTree<BinaryTreeAscTraits<int, TraversalOrder::PreOrder>> preTree;
BinaryTree<BinaryTreeAscTraits<int, TraversalOrder::PostOrder>> postTree;
```
Este diseño obligaba a **clonar el árbol o a instanciar contenedores separados** cada vez que se requería un recorrido distinto, violando el Principio de Responsabilidad Única (*SRP*):
1. **La estructura del árbol** (`BinaryTree` / `AVLTree`) es responsable de la propiedad jerárquica, las rotaciones, la inserción, el borrado y la sincronización concurrente (`m_mutex`).
2. **El recorrido** es simplemente una **proyección efímera (vista)** sobre el conjunto de nodos existentes, la cual no debe forzar la creación de un nuevo contenedor.

Con la refactorización arquitectural:
```cpp
// ✔️ Diseño desacoplado actual:
using DefaultTree = BinaryTree<BinaryTreeAscTraits<int>>;
DefaultTree tree;

// Acceso en O(1) tiempo y espacio a cualquiera de los 14 recorridos:
auto preView  = tree.traverse<Order::PreOrder>();
auto postView = tree.traverse<Order::PostOrder, Direction::Desc>();
```

---

#### B. Arquitectura de Jerarquía: `IterableCore` vs `ContainerCore`

Para permitir que tanto contenedores propietarios como vistas no propietarias compartan algoritmos y operadores, la jerarquía base se segregó en dos niveles ortogonales:

| Característica | `IterableCore<Traits>` | `ContainerCore<Traits>` |
| :--- | :--- | :--- |
| **Rol** | Rango iterable y sincronizable | Contenedor dinámico propietario de memoria |
| **Sincronización** | `virtual std::recursive_mutex& mutex() const = 0` | Posee `mutable std::recursive_mutex m_mutex` |
| **Métodos de Iteración** | `begin()`, `end()`, `empty()` | Heredados de `IterableCore` |
| **Mutabilidad / Ciclo de vida** | *Ninguno* (solo lectura de rango) | `clear()`, `push_back()`, `copy_from()`, `move_from()`, `rebuild()` |
| **Heredado por** | `BaseView`, `Callable`, `Searchable`, `Printable`, `ContainerCore` | `GeneralContainer`, `Applicable`, `Reversible`, `Readable`, `Drawable` |

---

#### C. Filtrado Estático de Mixins con Conceptos C++20 (`FilterViewMixin`)

Un contenedor concreto puede incorporar mixins mutantes como `Applicable` (que modifica nodos) o `Readable` (que llama a `clear()` y `push_back()`). Sin embargo, una **Vista (`BaseView`) no es dueña del contenedor ni debe modificar su topología**.

Mediante metaprogramación de conceptos C++20 en [`containers/GeneralContainer.h`](containers/GeneralContainer.h), el sistema clasifica y filtra los mixins en tiempo de compilación:

```cpp
template <template <typename> typename M, typename Traits>
concept IsIterableMixin = std::derived_from<M<Traits>, IterableCore<Traits>>;

template <template <typename> typename M, typename Traits>
concept IsViewMixin =
    IsIterableMixin<M, Traits> &&
    !std::derived_from<M<Traits>, ContainerCore<Traits>>;

template <template <typename> typename M, typename Traits>
struct IgnoredMixin {};

template <template <typename> typename M, typename Traits>
using FilterViewMixin = std::conditional_t<
    IsViewMixin<M, Traits>,
    M<Traits>,
    IgnoredMixin<M, Traits>
>;
```

##### ¿Cómo se ensambla `BaseView`?
```cpp
template <typename Traits, template <typename> typename... Mixins>
    requires (IsIterableMixin<Mixins, Traits> && ...)
class BaseView :
    virtual public IterableCore<Traits>,
    virtual public FilterViewMixin<Mixins, Traits>... {
    // Almacena una referencia al mutex del contenedor y dos iteradores:
    std::recursive_mutex& m_rMutex;
    ForwardIterator       m_begin;
    ForwardIterator       m_end;
};
```

1. Si el contenedor tiene `Callable`, `Searchable` o `Printable` (que solo dependen de `IterableCore`), la vista los **hereda automáticamente**.
2. Si el contenedor tiene `Applicable`, `Readable` o `Reversible` (que dependen de `ContainerCore`), `FilterViewMixin` los transforma en `IgnoredMixin`, descartándolos a nivel de tipos sin overhead de memoria.
3. Se verifica formalmente con `static_assert` en [`Demos.cpp`](Demos.cpp):
   ```cpp
   static_assert(IsCallableWith<RevView, ...>);    // ✔️ Callable permitido en vista
   static_assert(IsSearchableWith<RevView, ...>);  // ✔️ Searchable permitido en vista
   static_assert(!IsApplicableWith<RevView, ...>); // ❌ Applicable descartado en vista
   ```

---

#### D. Vistas Bidireccionales con `Reversible::reversed()`

El mixin `Reversible` reemplaza y amplía la funcionalidad del antiguo `ReverseCallable`:
- Define `rbegin()` y `rend()`.
- Proporciona `.reversed()`, el cual construye una vista reversa `ReverseView` en $O(1)$:
  ```cpp
  auto reversed() const {
      std::lock_guard lock(this->mutex());
      using ReverseView = typename Traits::template View<Node, BackwardIterator>;
      return ReverseView(this->mutex(), rbegin(), rend());
  }
  ```
- Simplifica la implementación de `.rcall(...)`:
  ```cpp
  template<typename Func, typename... Args>
  decltype(auto) rcall(Func func, Args&&... args) const {
      return reversed().call(std::forward<Func>(func), std::forward<Args>(args)...);
  }
  ```
- Dado que `ReverseView` hereda `Printable` y `Searchable`, ahora es posible imprimir y buscar en orden inverso de forma directa:
  ```cpp
  std::cout << "Vista invertida: " << vec.reversed() << std::endl;
  Node& last = vec.reversed().FirstThat(IsGreaterThan<Node, int>, 5);
  ```

---

#### E. Vistas de Recorridos en Árboles con `tree.traverse<Order, Direction>()`

En [`containers/binarytree.h`](containers/binarytree.h), `BaseBinaryTree` ofrece el método plantilla `.traverse()`:
```cpp
template <Order Ord, Direction Dir = Direction::Asc>
auto traverse() const {
    std::lock_guard lock(this->mutex());
    using Iter = BinaryTreeIterator<value_type, Ord, Dir>;
    using EulerView = typename Parent::Traits::template View<Node, Iter>;
    return EulerView(this->mutex(), Iter(m_pRoot), Iter(nullptr));
}
```

Cualquiera de los 14 recorridos de Euler es accesible de inmediato como un objeto iterable de primera clase con soporte para:
- Impresión directa con `operator<<`: `std::cout << tree.traverse<Order::PreOrder>();`
- Búsqueda funcional: `tree.traverse<Order::PostOrder>().FirstThat(pred, key);`
- Invocación genérica: `tree.traverse<Order::PreInOrder>().call(func, args...);`
- Bucles basados en rango: `for (const auto& node : tree.traverse<Order::PreInPostOrder, Direction::Desc>())`

### 3. Mecanismo de Rebalanceo AVL por Rotación Unificada (rotate_up)

En [`containers/avltree.h`](containers/avltree.h), `BaseAVLTree` implementa el auto-balanceo extendiendo `BaseBinaryTree` mediante el patrón **Template Method**:
1. Las operaciones de mutación (`insert` y `remove`) delegan la restauración del árbol al hook virtual `rebalance(node)`.
2. Al insertar o eliminar, `BaseAVLTree::rebalance(node)` asciende por los ancestros actualizando `m_height = 1 + std::max(h_izq, h_der)`.
3. Cuando detecta un nodo desbalanceado $Z$ con factor de balance $|bf(Z)| > 1$ (donde $bf = h \text{izq} - h \text{der}$):
   - Identifica al hijo más alto $Y = Z\text{->m pChild}[bf Z < 0]$.
   - Identifica al nieto más alto $X = Y\text{->m pChild}[bf Y < 0]$.

#### A. Primitiva de Rotación Unificada `rotate_up(node)`

En lugar de codificar 4 variantes manuales de rotación (`LL`, `RR`, `LR`, `RL`), la arquitectura unifica cualquier rotación en una única primitiva simétrica parametrizada por la posición relativa del nodo:

```cpp
void rotate_up(NodePtr node) {
    NodePtr node_p = node->m_parent;
    NodePtr node_g = node_p->m_parent;
    TX dp = node_p && node == node_p->m_pChild[1];  // true (1) si node es hijo derecho
    TX dg = node_g && node_p == node_g->m_pChild[1]; // true (1) si parent es hijo derecho
    connect(node_p, node->m_pChild[1-dp], dp);      // Adopta el subárbol cruzado
    connect(node, node_p, 1-dp);                    // node_p pasa a ser hijo de node
    connect(node_g, node, dg);                      // node se enlaza al abuelo
}
```

Cada invocación a `connect(parent, child, d)` enlaza los punteros bidireccionales y recalcula inmediatamente `parent->m_height`.

#### B. Clasificación de Casos: Simple (1x) vs. Compuesta (2x)

- **Caso en Línea (LL / RR)** ($bf Z \cdot bf Y > 0$ o $bf Y = 0$):
  Tanto $Y$ como $X$ se encuentran alineados en la misma dirección. Se resuelve con **1 rotación** ascendiendo al hijo $Y$:
  ```cpp
  rotate_up(node->m_parent); // Asciende Y al lugar de Z
  ```
- **Caso en Zig-Zag (LR / RL)** ($bf Z \cdot bf Y < 0$):
  Existe un cambio de dirección en el camino $Z \to Y \to X$ ("codo"). Se resuelve con **2 rotaciones** sucesivas ascendiendo al nieto $X$:
  ```cpp
  rotate_up(node); // 1ra rotación: asciende X al lugar de Y (convierte LR en LL)
  rotate_up(node); // 2da rotación: asciende X al lugar de Z (balancea el subárbol)
  ```

![Rotaciones para rebalancear AVL](docs/avl_step_up.svg)

El diagrama animado superior ilustra cómo la transformación geométrica preserva de forma estricta la invariante del recorrido in-order:

$T 1 < (X \text{ o } Y) < T 2 < (Y \text{ o } X) < T 3 < Z < T 4$

### 4. Invariante de Reconstrucción de Contenedores (rebuild)

Cuando se ejecutan operaciones masivas que transforman el valor de los nodos in-place (como `ApplyFunction`), las invariantes estructurales de ciertos contenedores pueden invalidarse.

Para solventar esto de manera limpia y polimórfica:
1. `ContainerCore` introduce el método virtual no-op:
   ```cpp
   virtual void rebuild() {}
   ```
2. El mixin `Applicable` ejecuta la función sobre cada nodo y de inmediato invoca a `rebuild()`:
   ```cpp
   template <typename Func, typename... Args>
   void ApplyFunction(Func func, Args&&... args) {
       std::lock_guard lock(this->mutex());
       this->call(std::forward<Func>(func), std::forward<Args>(args)...);
       this->rebuild(); // Hook de restauración del invariante
   }
   ```
3. En [`containers/Heap.h`](containers/Heap.h), `BaseHeap` sobreescribe `rebuild()`:
   ```cpp
   template <typename Traits, template <typename> typename... Mixins>
   void BaseHeap<Traits, Mixins...>::rebuild() {
       std::lock_guard lock(m_mutex);
       for (size_t i = 1; i < size(); ++i) HeapifyUp(i);
   }
   ```
   Garantizando que tras un `ApplyFunction(AddOne)` o `ApplyFunction(AddX, 10)`, el árbol binario completo que respalda el montículo restablezca su propiedad de orden en $O(N)$ sin requerir intervención manual del cliente.

### 5. Subsistema de Renderizado ASCII en Consola (sketch y ConsoleGraphRenderer)

El subsistema visual en [`utilities/render.h`](utilities/render.h) separa la representación lógica del grafo de su renderizado visual:

1. **`VisualGraph sketch() const`**:
   `BaseBinaryTree::sketch()` calcula la geometría del árbol en anchura y altura:

   $\text{segment} \text{width} = 2 \cdot \text{cell} \text{width} \cdot 2^{h - k - 1}$

   Asigna coordenadas $X, Y$ a cada nodo por niveles y genera aristas etiquetadas como `LeftChild` o `RightChild`.

2. **`ConsoleGraphRenderer`**:
   Construye un lienzo matricial en memoria:
   - Dibuja barras horizontales de extensión `_`.
   - Traza ramas diagonales simétricas:
     - `/` hacia el hijo izquierdo (`LeftChild`).
     - `\` hacia el hijo derecho (`RightChild`).
   - Centra las etiquetas de los nodos sobre sus respectivas bifurcaciones.

Ejemplo de salida en terminal:
```text
(Altura: 3)
          __50__
         /      \
        /        \
      20          80
      /\          /\
     /  \        /  \
   10    30    70    90
```

### 6. Guía de Uso y Demostración Práctica de la API

#### A. Creación y Recorridos Dinámicos de Árboles

```cpp
#include "containers/binarytree.h"
#include "containers/avltree.h"

// 1. Instanciación con Traits por defecto (InOrder estándar)
BinaryTree<BinaryTreeAscTraits<int>> tree;
tree.insert(50, 1);
tree.insert(20, 2);
tree.insert(80, 3);
tree.insert(10, 4);
tree.insert(30, 5);

// 2. Iteración estándar (InOrder Ascendente)
std::cout << "InOrder natural: " << tree << std::endl;

// 3. Proyección en O(1) de los 14 recorridos de Euler
std::cout << "PreOrder: "  << tree.traverse<Order::PreOrder>()  << std::endl;
std::cout << "PostOrder: " << tree.traverse<Order::PostOrder>() << std::endl;
std::cout << "Euler Desc: " 
          << tree.traverse<Order::PreInPostOrder, Direction::Desc>() 
          << std::endl;

// 4. Búsqueda y algoritmos sobre vistas
auto& found = tree.traverse<Order::PostOrder>().FirstThat(
    [](const auto& node, int x) { return node.getValue() > x; }, 
    25
);
std::cout << "Primer nodo en PostOrder > 25: " << found << std::endl;
```

#### B. Manejo de Contenedores Reversibles

```cpp
#include "containers/vector.h"

Vector<VectorAscTraits<int>> vec = {{1, 10}, {2, 20}, {3, 30}, {4, 40}};

// Imprimir directamente en sentido inverso sin alterar la estructura:
std::cout << "Invertido: " << vec.reversed() << std::endl;

// Búsqueda desde el final hacia el inicio:
auto& last_gt = vec.reversed().FirstThat(
    [](const auto& node, int x) { return node.getValue() > x; },
    20
);
```

#### C. Renderizado Gráfico de Árboles

```cpp
#include "utilities/render.h"

// Dibujo directo en consola utilizando el mixin Drawable:
tree.draw(ConsoleGraphRenderer(std::cout));
```
