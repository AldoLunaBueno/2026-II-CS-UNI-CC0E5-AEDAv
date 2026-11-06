# 2026-II-CS-UNI-CC0E5-AEDAv

## 🏛️ I. Arquitectura del Repositorio y Vistas de Subsistemas

### 1. Vista General del Sistema

```mermaid
flowchart LR
    TRAITS["<b>1. TRAITS</b><br/>Orden (Asc/Desc) +<br/>TraversalOrder +<br/>Estructura (Node, Iter)"]
    MIXINS["<b>2. MIXINS</b><br/>Callable, Printable,<br/>Searchable, Drawable..."]
    CONT["<b>3. CONTAINERS</b><br/>Vector, Heap, BinaryTree,<br/>AVLTree y 4 Listas"]
    ITER["<b>4. ITERADORES</b><br/>CRTP GeneralIterator<br/>(Forward / Backward / Euler)"]
    NODOS["<b>5. NODOS</b><br/>GeneralNode, LinkedListNode,<br/>BinaryTreeNode (m_height)"]
    LIBRES["<b>6. LIBRES & UTILS</b><br/>::call, operator&lt;&lt;,&gt;&gt;,<br/>ConsoleGraphRenderer"]

    TRAITS ==>|"Inyecta tipos, Order y Compare"| CONT
    MIXINS ==>|"Ensambla capacidades"| CONT
    CONT ==>|"begin() / end() / rbegin()"| ITER
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

<details>
<summary><b>2. Jerarquía de Contenedores y Sistema de Mixins</b></summary>

```mermaid
flowchart TB
    CC["<b>ContainerCore#lt;Traits#gt;</b><br/>Base abstracta: m_mutex recursivo, copy_from(), move_from(),<br/>begin(), end(), empty(), clear(), push_back()"]

    subgraph MIX["Sistema de Mixins (Capacidades modulares)"]
        direction TB
        CAL["Callable#lt;Traits#gt;<br/><i>.call(func, args...)</i>"]
        APP["Applicable#lt;Traits#gt;<br/><i>.ApplyFunction(...)</i>"]
        SEA["Searchable#lt;Traits#gt;<br/><i>.FirstThat(...)</i>"]
        RCAL["Reversible#lt;Traits#gt;<br/><i>.rcall(), rbegin(), rend()</i>"]
        PRI["Printable#lt;Traits#gt;<br/><i>friend operator#lt;#lt;</i>"]
        REA["Readable#lt;Traits#gt;<br/><i>friend operator#gt;#gt;</i>"]
        DRW["Drawable#lt;Traits#gt;<br/><i>.sketch(), .draw(...)</i>"]

        CAL --> APP
        CAL --> SEA
    end

    GC["<b>GeneralContainer#lt;Traits, Mixins...#gt;</b><br/>Virtual inheritance (Concept IsContainerCore)"]

    CC --> GC
    CC -. "virtual base" .-> MIX
    MIX -. "herencia múltiple" .-> GC

    subgraph CONT_STRUCT["Contenedores Concretos"]
        direction TB
        V["Vector#lt;Traits#gt;<br/><i>BaseVector</i>"]
        
        H["Heap#lt;Traits#gt;<br/><i>BaseHeap</i>"]
        MH["MinHeap / MaxHeap<br/><i>(Heap.h)</i>"]
        
        LL["LinkedList#lt;Traits#gt;<br/><i>BaseLinkedList (virtual linkNode)</i>"]
        LC["CircularLinkedList#lt;Traits#gt;"]
        LDE["DoubleLinkedList#lt;Traits#gt;"]
        LDEC["CircularDoubleLinkedList#lt;Traits#gt;"]
        
        BT["BinaryTree#lt;Traits#gt;<br/><i>BaseBinaryTree<br/>(insert, remove, connect, virtual rebalance)</i>"]
        AVL["AVLTree#lt;Traits#gt;<br/><i>BaseAVLTree<br/>(override rebalance, rotate_up, connect)</i>"]

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

    classDef base fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef mixin fill:#FCE4EC,stroke:#C2185B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef vec fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef lista fill:#FFF3E0,stroke:#EF6C00,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef arbol fill:#E8F5E9,stroke:#2E7D32,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class CC,GC base
    class CAL,APP,SEA,RCAL,PRI,REA,DRW mixin
    class V,H,MH vec
    class LL,LC,LDE,LDEC lista
    class BT,AVL arbol

    style MIX fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style CONT_STRUCT fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

</details>

<details>
<summary><b>3. Sistema de Iteradores, Nodos y Recorridos</b></summary>

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
        TO["<b>TraversalOrder</b><br/>Puros: PreOrder, InOrder, PostOrder<br/>Híbridos: PreIn, PrePost, InPost, PreInPost"]
        DIR["<b>Direction</b><br/>Asc (0) | Desc (1)"]
        PH -->|"suma de fases"| TO
    end

    subgraph ITER["Jerarquía de Iteradores (GeneralIterator.h)"]
        direction TB
        GI["<b>GeneralIterator#lt;Derived, T#gt;</b><br/>CRTP: m_ptr, m_pStart, operator*, ==, !="]
        
        VFI["VectorForwardIterator#lt;T#gt;<br/><i>operator++ (++m_ptr)</i>"]
        VBI["VectorBackwardIterator#lt;T#gt;<br/><i>operator++ (--m_ptr)</i>"]
        
        LLFI["LinkedListForwardIterator#lt;T#gt;<br/><i>Avanza por m_pNext (soporta ciclos)</i>"]
        LLBI["LinkedListBackwardIterator#lt;T#gt;<br/><i>Retrocede por m_pPrev (soporta ciclos)</i>"]
        
        BTI["BinaryTreeIterator#lt;T, Order, Dir#gt;<br/><i>Máquina de estados del recorrido de Euler<br/>step_until_phase() (14 recorridos posibles)</i>"]

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
    TO -. "parametriza Order" .-> BTI
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
    class BTI,BTN,PH,TO,DIR arbol
    class CALL,APP_FN,FIRST libre

    style ALGO fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style ENUMS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style ITER fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style NODOS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

</details>

<details>
<summary><b>4. Sistema de Traits y Políticas de Orden</b></summary>

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
        BTT["<b>BinaryTreeTraits#lt;T, Order#gt;</b><br/>Node: BinaryTreeNode<br/>Iter: BinaryTreeIterator (Asc/Desc)"]

        LLT --> DLLT
    end

    subgraph COMBINED["Traits Compuestos (Orden + Estructura)"]
        direction TB
        V_TR["VectorAscTraits / VectorDescTraits"]
        H_TR["HeapAscTraits / HeapDescTraits"]
        LL_TR["LinkedListAscTraits / LinkedListDescTraits"]
        DLL_TR["DoubleLinkedListAscTraits / DoubleLinkedListDescTraits"]
        BT_TR["BinaryTreeAscTraits#lt;T, Order = InOrder#gt;<br/>BinaryTreeDescTraits#lt;T, Order = InOrder#gt;"]
    end

    subgraph USERS["Contenedores Parametrizados"]
        direction TB
        U_V["Vector#lt;VectorAscTraits#lt;T#gt;#gt;"]
        U_H["Heap#lt;HeapAscTraits#lt;T#gt;#gt;"]
        U_LL["LinkedList / CircularLinkedList"]
        U_DLL["DoubleLinkedList / CircularDoubleLinkedList"]
        U_BT["BinaryTree#lt;BinaryTreeAscTraits#lt;T, Order#gt;#gt;"]
        U_AVL["AVLTree#lt;BinaryTreeAscTraits#lt;T, Order#gt;#gt;"]
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

    classDef orden fill:#F3E5F5,stroke:#8E24AA,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef struct fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef comb fill:#FFF9C4,stroke:#FBC02D,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef user fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class DT,ASC,DESC orden
    class VT,HT,LLT,DLLT,BTT struct
    class V_TR,H_TR,LL_TR,DLL_TR,BT_TR comb
    class U_V,U_H,U_LL,U_DLL,U_BT,U_AVL user

    style ORDEN fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style STRUCT fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style COMBINED fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style USERS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

</details>

<details>
<summary><b>5. Patrón Template Method en Árboles (BinaryTree / AVLTree) y Renderizado</b></summary>

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

    subgraph REND["Subsistema de Renderizado (render.h)"]
        direction TB
        VG["<b>VisualGraph</b><br/>SimpleVector#lt;VisualNode#gt;<br/>SimpleVector#lt;VisualEdge#gt;"]
        GR["<b>GraphRenderer</b><br/>ConsoleGraphRenderer / SvgGraphRenderer"]
        VG --> GR
    end

    REB_B -. "override en AVL" .-> REB_A
    CONN_A ==>|"reutiliza enlace"| CONN_B
    SK ==>|"alimenta a"| VG

    classDef bst fill:#E8F5E9,stroke:#2E7D32,color:#1A1A1A,stroke-width:1.5px;
    classDef avl fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px;
    classDef rnd fill:#ECEFF1,stroke:#546E7A,color:#1A1A1A,stroke-width:1.5px;

    class INS,REM,I_INS,I_REM,CONN_B,REB_B,SK bst
    class REB_A,ROT,CONN_A avl
    class VG,GR rnd

    style BST fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style AVL_SUB fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style REND fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

</details>

---

## ⚙️ II. Detalle de Aspectos Específicos

<details>
<summary><b>1. Máquina de Estados del Recorrido de Euler por Fases</b></summary>

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

Numerar las fases como `0`, `1` y `2` es un recurso didáctico para distinguir los tres contactos al rodear un nodo, pero en el código ([`containers/binarytree.h`](containers/binarytree.h)) cada fase es una potencia de 2 que ocupa **una posición de bit exclusiva** dentro de un número binario de 3 cifras (`Phase::Pre = 0b100`, `Phase::In = 0b010`, `Phase::Post = 0b001`). Así, cualquier `TraversalOrder` funciona como un panel de 3 interruptores donde cada cifra binaria **enciende (`1`)** o **apaga (`0`)** la parada del iterador en esa fase mediante `while (m_ptr && !(m_phase & Order))`:

![Puente entre Fases y Máscara Binaria](docs/euler_bitmask.svg)

---

#### D. Composición por Máscara de Bits: Diagrama de Venn de los 14 Recorridos

Al combinar los 3 bits de `{Phase::Pre, Phase::In, Phase::Post}`, las $2^3 - 1 = 7$ regiones no vacías de intersección definen los 3 órdenes **Puros** y los 4 órdenes **Híbridos** (con el recorrido de Euler completo `0b111` en la intersección triple central), que multiplicados por las 2 direcciones (`Asc` y `Desc`) producen los **14 recorridos**:

![Diagrama de Venn de los 14 Recorridos](docs/euler_venn.svg)

</details>
