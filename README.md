# 2026-II-CS-UNI-CC0E5-AEDAv

## 🏛️ Arquitectura del Repositorio

### 1. Vista General del Sistema

```mermaid
flowchart LR
    TRAITS["<b>1. TRAITS</b><br/>Orden (Asc/Desc) +<br/>Estructura (Node, Iter)"]
    MIXINS["<b>2. MIXINS</b><br/>Callable, Printable,<br/>Searchable, Drawable..."]
    CONT["<b>3. CONTAINERS</b><br/>Vector, Heap, Árbol,<br/>4 variantes de Listas"]
    ITER["<b>4. ITERADORES</b><br/>CRTP GeneralIterator<br/>(Forward / Backward)"]
    NODOS["<b>5. NODOS</b><br/>GeneralNode, LinkedListNode,<br/>BinaryTreeNode"]
    LIBRES["<b>6. LIBRES & UTILS</b><br/>::call, operator&lt;&lt;,&gt;&gt;,<br/>ConsoleGraphRenderer"]

    TRAITS ==>|"Inyecta tipos y Compare"| CONT
    MIXINS ==>|"Ensambla capacidades"| CONT
    CONT ==>|"begin() / end() / rbegin()"| ITER
    ITER ==>|"Apuntan y recorren"| NODOS
    LIBRES -.->|"Operan vía iteradores"| CONT

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
    CC["<b>ContainerCore#lt;Traits#gt;</b><br/>Base abstracta: m_mutex recursivo,<br/>begin(), end(), empty(), clear()"]

    subgraph MIX["Sistema de Mixins (Capacidades modulares)"]
        direction TB
        CAL["Callable#lt;Traits#gt;<br/><i>.call(func, args...)</i>"]
        APP["Applicable#lt;Traits#gt;<br/><i>.ApplyFunction(...)</i>"]
        SEA["Searchable#lt;Traits#gt;<br/><i>.FirstThat(...)</i>"]
        RCAL["ReverseCallable#lt;Traits#gt;<br/><i>.rcall(), rbegin(), rend()</i>"]
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
        
        LL["LinkedList#lt;Traits#gt;"]
        LC["CircularLinkedList#lt;Traits#gt;"]
        LDE["DoubleLinkedList#lt;Traits#gt;"]
        LDEC["CircularDoubleLinkedList#lt;Traits#gt;"]
        
        BT["BinaryTree#lt;Traits#gt;<br/><i>BaseBinaryTree</i>"]

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
    class BT arbol

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

    subgraph ITER["Jerarquía de Iteradores (GeneralIterator.h)"]
        direction TB
        GI["<b>GeneralIterator#lt;Derived, T#gt;</b><br/>CRTP: m_ptr, m_pStart, operator*, ==, !="]
        
        VFI["VectorForwardIterator#lt;T#gt;<br/><i>operator++ (++m_ptr)</i>"]
        VBI["VectorBackwardIterator#lt;T#gt;<br/><i>operator++ (--m_ptr)</i>"]
        
        LLFI["LinkedListForwardIterator#lt;T#gt;<br/><i>Avanza por m_pNext (soporta ciclos)</i>"]
        LLBI["LinkedListBackwardIterator#lt;T#gt;<br/><i>Retrocede por m_pPrev (soporta ciclos)</i>"]
        
        BTI["BinaryTreeIterator#lt;T, Dir#gt;<br/><i>Dir=0: Forward | Dir=1: Backward<br/>(In-order con m_parent y m_pChild)</i>"]

        GI --> VFI
        GI --> VBI
        GI --> LLFI
        GI --> LLBI
        GI --> BTI
    end

    subgraph NODOS["Jerarquía de Nodos (GeneralNode.h)"]
        direction TB
        GN["<b>GeneralNode#lt;T#gt;</b><br/>m_value (T), m_ref (Ref)<br/>getValue(), value(), stream I/O"]
        
        LLN["<b>LinkedListNode#lt;T#gt;</b><br/>m_pNext, m_pPrev<br/><i>(Singly, Circular y Doubly)</i>"]
        
        BTN["<b>BinaryTreeNode#lt;T#gt;</b><br/>m_pChild[2] (izq/der), m_parent"]

        GN --> LLN
        GN --> BTN
    end

    CALL ==>|"opera mediante"| GI
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
    class BTI,BTN arbol
    class CALL,APP_FN,FIRST libre

    style ALGO fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
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
        BTT["<b>BinaryTreeTraits#lt;T#gt;</b><br/>Node: BinaryTreeNode<br/>Iter: BinaryTreeForward/Backward"]

        LLT --> DLLT
    end

    subgraph COMBINED["Traits Compuestos (Orden + Estructura)"]
        direction TB
        V_TR["VectorAscTraits / VectorDescTraits"]
        H_TR["HeapAscTraits / HeapDescTraits"]
        LL_TR["LinkedListAscTraits / LinkedListDescTraits"]
        DLL_TR["DoubleLinkedListAscTraits / DoubleLinkedListDescTraits"]
        BT_TR["BinaryTreeAscTraits / BinaryTreeDescTraits"]
    end

    subgraph USERS["Contenedores Parametrizados"]
        direction TB
        U_V["Vector#lt;VectorAscTraits#lt;T#gt;#gt;"]
        U_H["Heap#lt;HeapAscTraits#lt;T#gt;#gt;"]
        U_LL["LinkedList#lt;LinkedListAscTraits#lt;T#gt;#gt;"]
        U_DLL["DoubleLinkedList#lt;DoubleLinkedListAscTraits#lt;T#gt;#gt;"]
        U_BT["BinaryTree#lt;BinaryTreeAscTraits#lt;T#gt;#gt;"]
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

    classDef orden fill:#F3E5F5,stroke:#8E24AA,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef struct fill:#E8EAF6,stroke:#3949AB,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef comb fill:#FFF9C4,stroke:#FBC02D,color:#1A1A1A,stroke-width:1.5px,font-size:15px;
    classDef user fill:#E0F2F1,stroke:#00897B,color:#1A1A1A,stroke-width:1.5px,font-size:15px;

    class DT,ASC,DESC orden
    class VT,HT,LLT,DLLT,BTT struct
    class V_TR,H_TR,LL_TR,DLL_TR,BT_TR comb
    class U_V,U_H,U_LL,U_DLL,U_BT user

    style ORDEN fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style STRUCT fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style COMBINED fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
    style USERS fill:#8080800F,stroke:#9E9E9E,stroke-width:1px
```

</details>
