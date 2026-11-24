# Entendiendo Texcel <!-- omit in toc -->

- [Bloque I: Modelo de datos y lenguaje de fórmulas](#bloque-i-modelo-de-datos-y-lenguaje-de-fórmulas)
  - [Parte 1: El átomo de Texcel (Coordenada, Valor y Celda)](#parte-1-el-átomo-de-texcel-coordenada-valor-y-celda)
  - [Parte 2: De texto plano a un Árbol de Operaciones (`FormulaParser`)](#parte-2-de-texto-plano-a-un-árbol-de-operaciones-formulaparser)
- [Bloque II: El motor de cálculo y dependencias](#bloque-ii-el-motor-de-cálculo-y-dependencias)
  - [Parte 3: Cómo se evalúa el árbol de operaciones (`engine.h`)](#parte-3-cómo-se-evalúa-el-árbol-de-operaciones-engineh)
  - [Parte 4: Almacenamiento, propagación de cambios y detección de ciclos (`engine.h`)](#parte-4-almacenamiento-propagación-de-cambios-y-detección-de-ciclos-engineh)
  - [Parte 5: Carga masiva en paralelo y múltiples hojas (`engine.h`)](#parte-5-carga-masiva-en-paralelo-y-múltiples-hojas-engineh)
- [Bloque III: La interfaz de terminal (TUI) y persistencia](#bloque-iii-la-interfaz-de-terminal-tui-y-persistencia)
  - [Parte 6: La interfaz de terminal (TUI) (`tui.h`)](#parte-6-la-interfaz-de-terminal-tui-tuih)
  - [Parte 7: Modo Point, Undo/Redo, Portapapeles e Inspector AST](#parte-7-modo-point-undoredo-portapapeles-e-inspector-ast)
  - [Parte 8: Persistencia en archivos `.txl` y configuración (`config.h`)](#parte-8-persistencia-en-archivos-txl-y-configuración-configh)
- [Bloque IV: Funciones avanzadas, concurrencia e infraestructura de soporte](#bloque-iv-funciones-avanzadas-concurrencia-e-infraestructura-de-soporte)
  - [Parte 9: Catálogo unificado de funciones, rangos y criterios (`types.h` y `engine.h`)](#parte-9-catálogo-unificado-de-funciones-rangos-y-criterios-typesh-y-engineh)
  - [Parte 10: Concurrencia, previsualización y utilidades de soporte (`engine.h`, `clipboard.h`, `text.h`, `loading_anim.h`)](#parte-10-concurrencia-previsualización-y-utilidades-de-soporte-engineh-clipboardh-texth-loading_animh)
- [Resumen: El viaje completo de una fórmula](#resumen-el-viaje-completo-de-una-fórmula)

---

Para construir nuestro entendimiento de **Texcel** desde cero, conviene ver primero el mapa mínimo de cómo está organizado en [`texcel.h`](../src/texcel/texcel.h#L10-L13). Todo el subproyecto se apoya en 4 piezas que encajan en cadena:

1. `types.h` — **Los datos básicos** (qué es una celda, una coordenada y un valor).
2. `parser.h` — **El traductor** (convierte texto como `"=A1+4"` en un árbol de operaciones).
3. `engine.h` — **El cerebro** (guarda las hojas, calcula fórmulas y actualiza dependencias).
4. `tui.h` — **La pantalla** (dibuja la cuadrícula en la terminal y lee el teclado).

---

## Bloque I: Modelo de datos y lenguaje de fórmulas
*¿Cómo se representan las celdas en memoria y cómo se traducen las fórmulas de texto a un árbol de operaciones?*

### Parte 1: El átomo de Texcel (Coordenada, Valor y Celda)

Empecemos por la base absoluta: **cómo representa Texcel una celda, dónde está y qué contiene**, todo definido en [`types.h`](../src/texcel/types.h).

#### 1. ¿Cómo sabe el programa dónde está una celda? (`CellCoord`)
En Excel tú escribes `"A1"` o `"D4"`, pero para programar una cuadrícula es mucho más fácil trabajar con dos números enteros empezando desde cero: `(col, row)`. Por ejemplo, `A1` es `(0, 0)` y `D4` es `(3, 3)`.

La estructura [`CellCoord`](../src/texcel/types.h#L36-L113) se encarga de esa traducción en ambos sentidos, con un límite fijo de `702` columnas (`A..ZZ`) y `999` filas (`1..999`):

- **De texto a números ([`CellCoord::parse`](../src/texcel/types.h#L88-L112)):** cuando el usuario escribe `"$d$4"` o `" a1 "`, este método limpia espacios y signos `$`, pasa todo a mayúsculas y convierte las letras en base 26 (`A=1, B=2, ...`) y los dígitos en fila. Al final resta `1` a ambos para dejarlos 0-indexados (`col_val - 1, row_val - 1`). Si el formato es inválido o se pasa de `ZZ999`, devuelve una coordenada con `valid = false`.
- **De números a texto ([`CellCoord::toId`](../src/texcel/types.h#L65-L85)):** hace exactamente el camino inverso para poder mostrar `"A1"` o `"AA12"` en pantalla o usarlos como clave de búsqueda.

#### 2. ¿Qué puede "valer" una celda? (`CellEvalValue`)
Una celda calculada no siempre es un número: puede ser un texto (como `"Aprobado"`) o un error (como `#DIV/0!`). En lugar de mezclar todo en un solo `string`, Texcel usa [`CellEvalValue`](../src/texcel/types.h#L241-L264):

```cpp
struct CellEvalValue {
    bool        is_text = false;
    double      num = 0.0;
    std::string text;
    std::string err_code;
    ...
};
```

En palabras sencillas, esta cajita tiene **tres estados posibles**:
1. **Error:** si `err_code` no está vacío (`hasError()`), el valor falló y guarda el código del error.
2. **Texto:** si `is_text` es `true`, el resultado real está en `text`.
3. **Número:** si `is_text` es `false` (y no hay error), el resultado numérico está en `num`.

Gracias a esto, cuando una fórmula suma celdas puede operar directamente con `num` sin tener que reconvertir texto a número cada vez, y cuando toca dibujar la celda en pantalla simplemente llama a `asString()`.

#### 3. La celda completa (`SpreadsheetCell`)
Aquí está la distinción más importante de toda hoja de cálculo: **una cosa es lo que tú escribes en la celda y otra distinta lo que la celda muestra**.

[`SpreadsheetCell`](../src/texcel/types.h#L294-L330) junta todo el historial de una celda en un solo lugar:

- **Lo que entró vs. lo que salió:**
  - `raw_expression`: guarda el texto exacto que ingresaste (por ejemplo `"= A1 + 4"`).
  - `result`: guarda el `CellEvalValue` ya calculado (por ejemplo `10`).
  - `displayValue()`: decide qué se ve en la cuadrícula. Si está vacía devuelve `""`; si `has_error` es verdadero devuelve `error_msg` (`#CYCLE!`, `#DIV/0!`, etc.); si no, devuelve `result.asString()`.
- **El estado de actualización (`dirty`):**
  - `dirty` empieza en `true`. Es una bandera que significa *"el resultado guardado en `result` podría estar desactualizado porque esta celda (o algo de lo que depende) acaba de cambiar"*. Así el motor evita recalcular toda la hoja innecesariamente.
- **De quién depende (`inputs` y `range_inputs`):**
  - Si una celda tiene `=A1 + SUM(B1:B5)`, guarda `"A1"` en `inputs` (celdas puntuales) y `B1:B5` en `range_inputs` (rangos rectangulares). Guardarlos separados permite saber rápido si un cambio en una celda afecta a esta fórmula sin tener que expandir cientos de celdas una por una.

---Analyzed parser.h:L1-200
Excelente elección. Una vez que sabemos qué es una celda, el siguiente paso es entender cómo **Texcel lee una fórmula escrita como texto** (por ejemplo, `= A1 + 4 * 2`) y la transforma en una estructura que la computadora pueda calcular sin confundirse con las prioridades matemáticas.

De eso se encarga [`FormulaParser`](../src/texcel/parser.h#L20-L341) en [`parser.h`](../src/texcel/parser.h).

---

### Parte 2: De texto plano a un Árbol de Operaciones (`FormulaParser`)

El trabajo de [`parse()`](../src/texcel/parser.h#L172-L181) es recibir un `std::string` (saltando el `=` inicial) y devolver un **árbol binario de sintaxis abstracta (AST)** hecho con nodos `BinaryTreeNode<AstToken>`.

Además, mientras lee la fórmula, va llenando dos listas (`m_refs` y `m_ranges`) con todas las celdas y rangos que encontró en el camino (`addRef` y `addRangeRef`), para que luego la celda sepa exactamente de quién depende.

El parser funciona en **tres niveles**, desde las piezas más pequeñas hasta las operaciones combinadas:

#### 1. Los ladrillos básicos ([`parsePrimary`](../src/texcel/parser.h#L255-L340))
Es la base. Lee un elemento individual mirando el siguiente carácter:
- **Paréntesis `(...)`:** si ve `(`, llama de nuevo a `parseBinary(1)` para resolver toda la sub-fórmula interna primero y exige que cierre con `)`.
- **Textos `"..."`:** lee hasta la comilla de cierre (soportando `\"` escapados) y crea un nodo `AstNodeKind::StringLiteral`.
- **Números:** lee dígitos y punto decimal, los convierte con `std::stod` y crea un nodo `AstNodeKind::Number`.
- **Palabras o referencias (empiezan con letra o `$`):**
  1. Si justo después viene `(`, es una **función** (`SUM(...)`, `IF(...)`) y salta a `parseFunctionCall()`.
  2. Si es `TRUE` o `FALSE`, lo guarda como literal booleano.
  3. Si no, lo interpreta con `CellCoord::parse()`. Si inmediatamente hay `:`, lee el segundo extremo y crea un rango `AstNodeKind::CellRange` (ej. `A1:B3`); si no hay `:`, crea una referencia simple `AstNodeKind::CellRef` (ej. `A1`).

#### 2. El signo negativo delante de algo ([`parseUnary`](../src/texcel/parser.h#L202-L212))
Justo encima de `parsePrimary` está `parseUnary()`, que maneja `+` y `-` unarios (como `-A1` o `-5`).

Tiene un truco muy práctico: en lugar de crear un tipo de nodo especial para "negación", convierte `-x` en **`0 - x`** (un nodo de resta común con `0` a la izquierda y `x` a la derecha). Así el evaluador usa la misma lógica de siempre.

#### 3. Respetar la jerarquía matemática ([`parseBinary`](../src/texcel/parser.h#L184-L200))
¿Cómo sabe el parser que `= A1 + 4 * 2` significa `A1 + (4 * 2)` y no `(A1 + 4) * 2`?

Usa un algoritmo llamado **Precedence Climbing**, apoyado en la tabla [`BINARY_OPS`](../src/texcel/types.h#L466-L479) de `types.h`, donde cada operador tiene un nivel de prioridad:
- Nivel `1`: comparaciones (`=`, `<>`, `<`, `>`, `<=`, `>=`)
- Nivel `2`: concatenación (`&`)
- Nivel `3`: suma y resta (`+`, `-`)
- Nivel `4`: multiplicación y división (`*`, `/`)
- Nivel `5`: potencia (`^`, marcada como asociativa a la derecha para que `2^3^2` sea `2^(3^2)`)

El algoritmo hace lo siguiente:
1. Lee el lado izquierdo (`left = parseUnary()`).
2. Espía el operador que sigue (`peekBinaryOp`). Si su prioridad es menor al mínimo pedido (`op.prec < min_prec`), se detiene y devuelve `left`.
3. Si tiene suficiente prioridad, consume el operador y llama recursivamente a `parseBinary` pidiendo una prioridad mayor (`op.prec + 1`, o `op.prec` si es `^`).
4. Como resultado, **los operadores más fuertes (`*`, `/`, `^`) quedan más abajo en el árbol** y los más débiles (`+`, `=`) quedan arriba:

```text
      (+)
     /   \
   A1    (*)
        /   \
       4     2
```

![Ascenso de precedencia (Precedence Climbing)](images/texcel_precedence_climbing.svg)

*(En el diagrama animado se muestra el caso extendido `= A1 + 4 * 2 ^ 3 - B1 >= 20` paso a paso; si abres [texcel_precedence_climbing.svg](images/texcel_precedence_climbing.svg) directamente en el navegador, puedes pausar y avanzar con `◀` / `▶` o las flechas del teclado).*

#### 4. ¿Cómo caben 3 o más argumentos en un árbol *binario*? ([`parseFunctionCall`](../src/texcel/parser.h#L214-L253))
Cada `BinaryTreeNode` solo tiene **dos hijos** (izquierdo y derecho). Entonces, ¿cómo representa una función como `=IF(A1 > 10, "Sí", "No")` que tiene 3 argumentos?

1. Primero valida que la función exista en el catálogo (`findTexcelFunc`) y que la cantidad de argumentos cumpla con `min_args`, `max_args` y `even_args` (para pares rango/criterio en `COUNTIFS`).
2. Luego coloca el **primer argumento** en el hijo izquierdo del nodo `FuncCall`, y encadena los demás en el hijo derecho usando nodos auxiliares llamados **`ArgPair` (`","`)**:

```text
       FuncCall("IF")
       /            \
   (A1 > 10)     ArgPair(",")
                 /          \
               "Sí"         "No"
```

De esta forma, **cualquier fórmula**, por compleja que sea, termina convertida en un árbol estrictamente binario. (Además, `FormulaParser` protege la memoria con `AstPtr`, un `std::unique_ptr` con destructor personalizado, por si ocurre un error de sintaxis a mitad del análisis).

---

## Bloque II: El motor de cálculo y dependencias
*¿Cómo se evalúan las fórmulas, se almacenan las celdas, se propagan los cambios y se paraleliza la carga?*

### Parte 3: Cómo se evalúa el árbol de operaciones ([`engine.h`](../src/texcel/engine.h))

Ya tenemos el árbol binario construido en memoria. Ahora el turno es del motor en [`engine.h`](../src/texcel/engine.h), que debe recorrer ese árbol y reducirlo a un único `CellEvalValue`.

#### 0. El atajo antes del árbol ([`evaluateRawExpression`](../src/texcel/engine.h#L850-L865))
Antes de invocar al parser o evaluar ningún árbol, el motor revisa si el texto empieza por `=`.
- Si **no** empieza por `=` (por ejemplo, escribiste `42` o `Hola`), no pierde tiempo construyendo un árbol: llama a [`evaluateLiteralFast`](../src/texcel/engine.h#L838-L848) y devuelve directamente el número o el texto.
- Si **sí** empieza por `=`, construye el árbol y se lo pasa a [`evaluateAstPostOrder`](../src/texcel/engine.h#L736-L836).

#### 1. Un recorrido en dos fases sin recursión ([`PrePostIter`](../src/texcel/engine.h#L165))
En lugar de usar funciones recursivas que podrían llenar la pila del sistema con fórmulas muy anidadas, `evaluateAstPostOrder` usa un iterador especial del proyecto: `BinaryTreeIterator<AstToken, Order::PrePostOrder, Direction::Asc>`.

La magia de este iterador es que visita cada nodo del árbol **dos veces**, en dos momentos distintos:
1. **`Phase::Pre` (al bajar hacia los hijos):** sirve para dar órdenes *de arriba hacia abajo* antes de calcular nada.
2. **`Phase::Post` (al subir desde los hijos):** ocurre cuando los hijos izquierdo y derecho ya fueron calculados, y sirve para combinar sus resultados *de abajo hacia arriba*.

Además, cada nodo guarda su propio resultado en `tok.evaluated` (evaluación *in-place*), así que el motor no necesita crear ninguna pila auxiliar externa.

#### 2. ¿Qué hace al bajar? (`Phase::Pre`, [`engine.h:749-788`](../src/texcel/engine.h#L749-L788))
Al entrar a un nodo antes de visitar sus hijos, el motor prepara dos cosas clave:

- **Cortocircuito en `IF` e `IFERROR`:**
  Recuerda que en `IF(cond, si_true, si_false)` el hijo izquierdo es `cond` y el hijo derecho es `ArgPair(si_true, si_false)`. Como el recorrido visita primero al hijo izquierdo, cuando el iterador está por bajar al hijo derecho, **`cond` ya está evaluado**.
  El motor revisa ese resultado:
  - Si `cond` fue verdadero, marca la rama `si_false` con `skip_eval = true`.
  - Si `cond` fue falso (o tuvo error), marca `si_true` (o todo el `ArgPair`) con `skip_eval = true`.
  - Algo parecido hace con `IFERROR(expr, respaldo)`: si `expr` no dio error, marca `respaldo` con `skip_eval = true`.
  Luego, si un nodo tiene `skip_eval = true`, propaga esa marca a sus dos hijos para que esa rama ni siquiera se toque. Así, por ejemplo, `=IF(A1=0, 0, 10/A1)` nunca lanza `#DIV/0!` cuando `A1` vale `0`.
- **Avisar si se espera un número (`expect_numeric`):**
  Si el nodo es un operador aritmético (`+`, `-`, `*`, `/`, `^`), activa `expect_numeric = true` en sus dos hijos para que sepan que deben entregar un número y no texto suelto.

#### 3. ¿Qué hace al subir? (`Phase::Post`, [`engine.h:790-827`](../src/texcel/engine.h#L790-L827))
Cuando el iterador regresa al nodo después de haber resuelto sus hijos:

1. Si se pidió traza RPN (`out_rpn`), va concatenando el token en notación postfija (por ejemplo, `[A1] [4] [2] [*] [+]`), que es lo que luego se puede inspeccionar en la interfaz.
2. Si `tok.skip_eval` es `true`, lo salta sin hacer nada.
3. Si no se saltó, calcula `tok.evaluated` según el tipo de nodo:
   - **`CellRef` (`A1`):** consulta el valor actual de esa celda mediante `readCellByCoord(...)`.
   - **`FuncCall` (`SUM`, `IF`, `ROUND`, etc.):** recoge los argumentos de la espina derecha (`collectFuncArgs`) y llama a [`evalFunction`](../src/texcel/engine.h#L572-L708).
   - **`BinaryOp` (`+`, `*`, `/`, etc.):** toma `evaluated` del hijo izquierdo y del hijo derecho y llama a [`evalBinaryOp`](../src/texcel/engine.h#L710-L730), que también detecta errores como dividir por cero (`#DIV/0!`) o potencias inválidas (`#NUM!`).
   - *(Los números y textos literales ya traían su `evaluated` listo desde el parser).*
4. **Conversión estricta si el padre pidió número:** si `expect_numeric` estaba activo y el resultado resultó ser texto, convierte `""` en `0.0`, textos numéricos como `"15"` en `15.0`, y cualquier otro texto como `"hola"` en `#VALUE!`.

#### 4. El resultado final ([`engine.h:830-835`](../src/texcel/engine.h#L830-L835))
Cuando el ciclo `for` termina, todos los subárboles se han ido colapsando hacia arriba hasta dejar la respuesta definitiva en **`root->getValue().evaluated`**. Si tiene error, se guarda en `out_err`; si no, se devuelve ese `CellEvalValue` para almacenarlo en la celda.

---

### Parte 4: Almacenamiento, propagación de cambios y detección de ciclos ([`engine.h`](../src/texcel/engine.h))

Hasta ahora vimos cómo funciona una celda individual y cómo se calcula una fórmula aislada. Pero una hoja de cálculo real tiene muchas celdas conectadas entre sí: si editas `A1`, todas las celdas que dependen de ella deben enterarse y actualizarse automáticamente.

#### 1. ¿Dónde viven las celdas en memoria? ([`engine.h:160-190`](../src/texcel/engine.h#L160-L190))
En lugar de usar una sola matriz gigante llena de espacios vacíos, `SpreadsheetEngine` combina estas estructuras especializadas:

- **`m_raw_table`** (*Tabla hash `clave → texto`*: `HashTable<string, string>`): guarda únicamente lo que escribió el usuario (ej. `"A1" → "=B1+2"`). Es la fuente de verdad que se guarda y carga desde disco.
- **`m_cells`** (*Tabla hash `clave → celda completa`*: `BaseHashTable<string, SpreadsheetCell>`): guarda el objeto [`SpreadsheetCell`](../src/texcel/types.h#L294-L330) completo de cada celda activa (ej. `"A1" → SpreadsheetCell{ raw="=B1+2", result=10, dirty=false, ... }`). Al estar respaldada por nodos con direcciones de memoria estables, otras estructuras pueden guardar punteros directos a sus elementos sin que se invaliden al agregar celdas nuevas.
- **`m_grid_index`** (*Matriz 2D dinámica de punteros*: `Vector<Vector<SpreadsheetCell*>>`): un vector de filas donde cada fila guarda punteros directos a `m_cells` (ej. `m_grid_index[0][0] = &m_cells["A1"]`, o `nullptr` si la celda está vacía). Sirve como atajo de velocidad: cuando una fórmula o la pantalla necesita leer `(col, row)`, [`fastGridPtr`](../src/texcel/engine.h#L233-L238) accede por índice en $O(1)$ sin tener que convertir números a `"A1"` ni buscar en la tabla hash.
- **`m_dependents` y `m_range_subs`** forman el **grafo inverso de dependencias** (quién observa a quién):
  - **`m_dependents`** (*Tabla hash `celda → lista de observadores`*: `BaseHashTable<string, LinkedList<string>>`): ejemplo `"A1" → ["B1", "C5"]` cuando `B1` tiene `=A1+5` y `C5` tiene `=A1*2`.
  - **`m_range_subs`** (*Vector de suscripciones a rangos*: `Vector<RangeSubscription>`, donde cada entrada es `{subscriber_id, CellRangeRect}`): ejemplo `[ {"D1", A1:A100}, {"E2", B2:D10} ]`. Así, si `D1` tiene `=SUM(A1:A100)`, guarda un solo rectángulo en vez de meter 100 entradas en `m_dependents`.

#### 2. ¿Qué pasa cuando editas una celda? ([`setCell`](../src/texcel/engine.h#L1402-L1434))
Cuando cambias el contenido de una celda (por ejemplo `A1`), el motor ejecuta cuatro pasos en orden estricto:

1. **Limpiar suscripciones viejas (`unsubscribeInputs`):** si `A1` antes tenía una fórmula que leía de `Z9`, se borra de la lista de observadores de `Z9`.
2. **Marcar a todos los afectados ([`collectAndMarkDirtySubgraph`](../src/texcel/engine.h#L1130-L1197)):** hace un recorrido en anchura (**BFS**) empezando desde `A1`. Busca quién la observa directamente en `m_dependents` y qué rangos en `m_range_subs` contienen su coordenada `(col, row)`. A cada celda afectada le enciende **`dirty = true`** y la mete en la cola para seguir buscando quién depende de ellas, en cadena.
3. **Evaluar la celda editada (`evalCellQuiet`):** calcula el nuevo valor de `A1` y, si es fórmula, registra sus nuevas dependencias en `m_dependents` y `m_range_subs`.
4. **Propagar en cascada (`propagateAffected`):** recorre la lista de afectadas y recalcula cada una. Si una fórmula lee otra celda que todavía sigue `dirty`, [`readCellByCoord`](../src/texcel/engine.h#L390-L412) la resuelve al vuelo primero antes de usar su valor.

*(Nota: cuando se carga una hoja completa desde archivo con [`recalculateAll`](../src/texcel/engine.h#L1282-L1397), en lugar de ir celda por celda, el motor ordena las dependencias topológicamente y evalúa en paralelo por niveles).*

#### 3. ¿Cómo se evitan bucles infinitos? (`#CYCLE!`, [`resolveCellValue`](../src/texcel/engine.h#L1055-L1087))
Imagina que `A1` dice `=B1 + 1` y `B1` dice `=A1 + 1`. Si el motor se limitara a pedir valores recursivamente, quedaría atrapado para siempre.

Para evitarlo, cada evaluación lleva consigo un vector llamado `visiting`, que representa el **camino activo** de celdas que se están calculando en ese instante:

1. Al empezar a resolver `A1`, se agrega a la ruta (`visiting.push_back("A1")`).
2. `A1` necesita `B1`, así que `B1` se agrega (`["A1", "B1"]`).
3. `B1` pide leer `A1`. Antes de entrar, [`readCellByCoord`](../src/texcel/engine.h#L390-L396) (y [`resolveCellValue`](../src/texcel/engine.h#L1058-L1061)) revisa si `"A1"` **ya está dentro de `visiting`**.
4. Como ya está, corta la recursión de inmediato y devuelve el error **`#CYCLE!`**.
5. Cuando una celda termina de evaluarse sin ciclos, se retira de la ruta (`visiting.pop_back()`). Así, si dos fórmulas distintas leen una misma celda `X1` en momentos distintos del mismo cálculo, no se confunde con un ciclo.

---

### Parte 5: Carga masiva en paralelo y múltiples hojas ([`engine.h`](../src/texcel/engine.h))

Cuando editas una sola celda, propagar cambios con BFS es rapidísimo. Pero, ¿qué pasa cuando abres un archivo `.txl` grande con miles de celdas y fórmulas desde cero? Hacer `setCell` miles de veces una por una sería lento.

#### 1. Carga en bloque en $O(N)$ ([`recalculateAll`](../src/texcel/engine.h#L1282-L1397))
Al cargar una hoja completa, [`recalculateAll`](../src/texcel/engine.h#L1282-L1397) reconstruye todo en cinco pasos masivos:

1. **Medir el tablero ([`engine.h:1298-1315`](../src/texcel/engine.h#L1298-L1315)):** recorre `m_raw_table` para encontrar la columna y fila máximas usadas y reserva de golpe todo el espacio de `m_grid_index`.
2. **Construir `m_cells` en paralelo ([`engine.h:1320-1352`](../src/texcel/engine.h#L1320-L1352)):** aprovecha que `m_raw_table` ya está ordenado por clave y usa `buildFromInOrder` para armar el árbol AVL en $O(N)$ sin rotaciones, repartiendo el trabajo en hilos. En ese mismo paso evalúa los literales (`42`, `"Hola"`) y parsea las fórmulas para extraer sus `inputs` y `range_inputs`.
3. **Juntar aristas de dependencia ([`engine.h:1354-1367`](../src/texcel/engine.h#L1354-L1367)):** recopila qué fórmulas quedaron `dirty` y todas las conexiones `referenciada -> dependiente`.
4. **Construir `m_dependents` en paralelo ([`engine.h:1370-1392`](../src/texcel/engine.h#L1370-L1392)):** ordena las aristas y vuelve a usar `buildFromInOrder` para armar el grafo inverso de un solo golpe.
5. **Evaluar las fórmulas en paralelo ([`engine.h:1394-1395`](../src/texcel/engine.h#L1394-L1395)):** delega todas las fórmulas pendientes a `evalDirtyGraphParallel`.

#### 2. Evaluación paralela por orden topológico de Kahn ([`evalDirtyGraphParallel`](../src/texcel/engine.h#L900-L1030))
No puedes evaluar `=B1 + 1` en paralelo con `B1 = A1 * 2` al azar, porque necesitas que `B1` termine primero. Para coordinar múltiples hilos sin que choquen, el motor usa el **algoritmo de Kahn**:

- **Umbral mínimo ([`engine.h:905-910`](../src/texcel/engine.h#L905-L910)):** si hay menos de `64` fórmulas (`PARALLEL_EVAL_THRESHOLD`), las evalúa en un solo hilo para no perder tiempo despertando el *thread pool*.
- **Contar dependencias (`in_degree`, [`engine.h:924-962`](../src/texcel/engine.h#L924-L962)):** para cada fórmula `i`, cuenta cuántas otras fórmulas sucias necesita que terminen antes que ella (`in_degree[i]`, un entero atómico) y arma la lista `children` de a quién desbloquea cuando ella termine.
- **Cola de trabajo `ready` ([`engine.h:964-1022`](../src/texcel/engine.h#L964-L1022)):**
  1. Todas las fórmulas con `in_degree == 0` (no esperan a ninguna otra fórmula) entran a `ready`.
  2. Cada hilo del [`TexcelThreadPool`](../src/texcel/engine.h#L30) toma una fórmula `u` de `ready` y la calcula con `evalFormulaUnlocked`.
  3. Al terminar `u`, resta `1` atómicamente al `in_degree` de cada hijo `v` en `children[u]`. Si alguno llega a `0`, queda desbloqueado. *(Como optimización, el mismo hilo toma el primer hijo desbloqueado directamente en su bucle sin volver a bloquear el mutex de la cola).*
- **Respaldo para ciclos ([`engine.h:1024-1029`](../src/texcel/engine.h#L1024-L1029)):** si al terminar los hilos quedaron nodos sin completar (`completed < n`), es porque su `in_degree` nunca llegó a `0` al estar atrapados en un ciclo. Esos pocos nodos se evalúan al final en el hilo principal para asignarles `#CYCLE!`.

**Ejemplo práctico:**
Imagina una hoja donde `A1 = 10` y `B1 = 20` (ya listos) y tenemos estas 5 fórmulas pendientes (suponiendo que la hoja superó el umbral de 64 fórmulas). El grafo de dependencias que se forma es:

```text
  [A1=10] (listo)         [B1=20] (listo)
         │                       │
         ▼                       ▼
   ┌───────────┐           ┌───────────┐       ┌─────────────────────┐
   │ C1 = A1*2 │           │ D1 = B1+5 │       │ Ciclo (no entra en  │
   │ in_deg: 0 │           │ in_deg: 0 │       │ ready):             │
   └─────┬─────┘           └─────┬─────┘       │                     │
         │                       │             │  ┌────┐     ┌────┐  │
         └───────────┬───────────┘             │  │ F1 │────▶│ G1 │  │
                     ▼                         │  └────┘◀────└────┘  │
               ┌───────────┐                   │  in_deg:1   in_deg:1│
               │ E1 = C1+D1│                   └─────────────────────┘
               │ in_deg: 2 │
               └───────────┘
```

| Celda | Fórmula | `in_degree` inicial | `children` (a quién desbloquea) |
| :--- | :--- | :---: | :--- |
| `C1` | `=A1 * 2` | `0` | `[E1]` |
| `D1` | `=B1 + 5` | `0` | `[E1]` |
| `E1` | `=C1 + D1` | `2` (espera a `C1` y `D1`) | `[]` |
| `F1` | `=G1 + 1` | `1` (espera a `G1`) | `[G1]` |
| `G1` | `=F1 + 1` | `1` (espera a `F1`) | `[F1]` |

1. **Inicio:** solo `C1` y `D1` tienen `in_degree == 0`, así que `ready = [C1, D1]`.
2. **En paralelo:** el *Hilo 1* toma `C1` (`20`) y el *Hilo 2* toma `D1` (`25`) al mismo tiempo.
   - El *Hilo 1* termina `C1` y resta `1` a `in_degree[E1]` (baja de `2` a `1`).
   - El *Hilo 2* termina `D1` y resta `1` a `in_degree[E1]` (baja de `1` a `0` → **`E1` queda desbloqueada** y el *Hilo 2* la toma directo).
3. **Siguiente paso:** el *Hilo 2* calcula `E1 = 20 + 25 = 45`. La cola `ready` queda vacía.
4. **Cierre de ciclos:** como `F1` y `G1` siguen con `in_degree == 1`, nunca entraron a `ready`. Al ver que `completed < n`, el hilo principal las evalúa secuencialmente al final y ambas reciben `#CYCLE!`.

#### 3. Puente hacia el exterior: una hoja por instancia ([`state.h`](../src/texcel/helpers/state.h))
Un detalle clave de diseño: `SpreadsheetEngine` gestiona **una sola hoja activa en memoria**. El soporte multi-hoja no vive dentro del motor de cálculo, sino en la capa superior ([`TuiState`](../src/texcel/helpers/state.h#L214) y [`TexcelConfig`](../src/texcel/helpers/config.h#L26-L165)), que coordina múltiples archivos `.txl` en `output/sheets/`:

- Cuando el usuario cambia de hoja desde la interfaz (`ModalKind::Sheets`, [`state.h:45`](../src/texcel/helpers/state.h#L45)), el motor simplemente reemplaza `m_raw_table` con el nuevo archivo y ejecuta `recalculateAll()` para reconstruir la cuadrícula desde cero.
- En el **Bloque III** veremos cómo la interfaz en terminal expone ese gestor de hojas y cómo se guardan los `.txl` en disco (Parte 8).

---

## Bloque III: La interfaz de terminal (TUI) y persistencia
*¿Cómo interactúa el usuario mediante teclado y ratón, y cómo se dibuja todo sin parpadeo?*

### Parte 6: La interfaz de terminal (TUI) ([`tui.h`](../src/texcel/tui.h))

Todo lo que vimos hasta ahora ocurre "por debajo" en memoria. Para que puedas ver la hoja, moverte con las flechas o el ratón y escribir fórmulas en vivo, entra la capa TUI, organizada de forma acíclica en cuatro piezas:

- [`tui.h`](../src/texcel/tui.h) (`SpreadsheetTUI`): el orquestador del bucle principal de entrada y refresco.
- [`state.h`](../src/texcel/helpers/state.h) (`TuiState`): el estado visual de la interfaz (posición del cursor, rango seleccionado, texto en edición, historial *Undo/Redo*, portapapeles y modal abierto).
- [`renderer.h`](../src/texcel/helpers/renderer.h): dibuja todos los paneles y modales en pantalla.
- [`input.h`](../src/texcel/helpers/input.h): interpreta teclas, atajos, secuencias `ESC` y eventos de ratón.

#### 1. El bucle principal y el modo *raw* ([`runInteractive`](../src/texcel/tui.h#L53-L207))
Normalmente una terminal espera a que presiones `Enter` para entregar lo que escribiste. Para reaccionar tecla por tecla, `runInteractive` configura `STDIN_FILENO` en **modo *raw*** usando `termios` ([`tui.h:56-63`](../src/texcel/tui.h#L56-L63)) y desactiva `ICANON`, `ECHO`, `ISIG` e `IXON` (así atajos como `Ctrl+S` o `Ctrl+C` llegan al programa en lugar de pausarlo o matarlo). Un `TermGuard` con RAII ([`tui.h:73-89`](../src/texcel/tui.h#L73-L89)) asegura que la terminal vuelva a su estado normal al salir.

Luego entra en un `while` que lee byte por byte y decide en qué orden atenderlo ([`tui.h:99-206`](../src/texcel/tui.h#L99-L206)):

1. **Redimensionado (`SIGWINCH`):** si cambiaste el tamaño de la ventana, recalcula el layout y redibuja.
2. **Atajos globales:** `Ctrl+D` (salir), `Ctrl+C` (copiar), `Ctrl+X` (prefijo de comandos), `Ctrl+S` (guardar), `Ctrl+Z`/`Ctrl+Y` (deshacer/rehacer), `Ctrl+B` (buscar), `Ctrl+F` (funciones), `Ctrl+G` (ir a celda) y `Ctrl+O` (hojas).
3. **Secuencias `ESC` ([`tui.h:178-196`](../src/texcel/tui.h#L178-L196)):** delega a [`handleEscapeSequence`](../src/texcel/helpers/input.h#L671) para distinguir flechas, `Shift+Flechas` o eventos de ratón SGR (clic, rueda, arrastre). Si estás arrastrando el ratón, agrupa las ráfagas pendientes con `poll` para evitar redibujar cuadros intermedios innecesarios.
4. **Foco activo ([`tui.h:197-203`](../src/texcel/tui.h#L197-L203)):** según dónde estés, envía la tecla a `handleModalKey`, `handleNameBoxKey`, `handleSearchKey`, `handleGridKey` (navegación en cuadrícula) o `handleConsoleKey` (escribiendo en la barra de fórmulas).
5. **Refresco:** si el comando devolvió que debe continuar, llama a `renderAll()`.

#### 2. Dibujar sin parpadeo ([`renderAll`](../src/texcel/helpers/renderer.h#L985-L1033))
Cada vez que la pantalla se actualiza, [`renderAll`](../src/texcel/helpers/renderer.h#L985-L1033) sigue dos ideas clave:

- **Previsualización en vivo ([`renderer.h:993`](../src/texcel/helpers/renderer.h#L993)):** antes de pintar, consulta `st.engine.previewCommand(st.input_buffer, active_id)` para calcular al vuelo lo que daría la fórmula que estás escribiendo (sin modificar aún la celda real).
- **Doble búfer en memoria ([`renderer.h:995-1032`](../src/texcel/helpers/renderer.h#L995-L1032)):** en lugar de imprimir directo a `std::cout` línea por línea (lo que causaría parpadeo), redirige todo a un `std::ostringstream frame_buf` y pinta en orden:
  1. [`renderHeaderBar`](../src/texcel/helpers/renderer.h#L151): barra superior con modo, nombre de hoja y botones.
  2. [`renderGridWindow`](../src/texcel/helpers/renderer.h#L290): matriz de celdas visibles, cursor, selección y colores de dependencias.
  3. [`renderInspectorWindow`](../src/texcel/helpers/renderer.h#L477): panel derecho con la fórmula, árbol AST y referencias de la celda activa.
  4. [`renderCommandWindow`](../src/texcel/helpers/renderer.h#L729): barra de comandos inferior, *calltips* de funciones y resultado en vivo.
  5. [`renderActivityWindow`](../src/texcel/helpers/renderer.h#L801): historial reciente de eventos.
  6. Modales flotantes (`Sheets`, `Functions`, `Help`) si hay alguno abierto.

Al terminar, envía todo el `frame_buf` de un solo golpe con `Ansi::flushSynchronizedFrame(...)`.

---

### Parte 7: Modo Point, Undo/Redo, Portapapeles e Inspector AST

Más allá de dibujar celdas, la capa TUI implementa cuatro ayudas clave para que editar fórmulas en terminal se sienta fluido:

#### 1. Modo *Point* (señalar celdas al escribir fórmulas, [`handlePointModeKey`](../src/texcel/helpers/input.h#L856-L881))
Cuando estás escribiendo una fórmula como `=SUM(` o `=A1 + ` y mueves el cursor sobre la cuadrícula para elegir celdas sin teclear sus nombres, entra en juego el modo *Point*:

- Guarda lo que llevabas escrito en `point_base` y va insertando automáticamente en `input_buffer` la celda (o rango si mantienes selección) donde esté el cursor.
- Si presionas un operador binario (`+`, `-`, `*`, `/`, etc.), `,` o `:`, fija la referencia actual en `point_base` para que puedas seguir señalando la siguiente celda.
- Si presionas `)`, cierra el paréntesis, sale de modo *Point* y devuelve el foco a `CommandConsole`. Si presionas `Enter`, confirma directamente la fórmula.

#### 2. Undo / Redo por parches diferenciales ([`UndoManager`](../src/texcel/helpers/state.h#L127-L159))
Para soportar `Ctrl+Z` y `Ctrl+Y` sin duplicar toda la hoja en memoria, `UndoManager` guarda un historial de hasta `100` transacciones (`MAX_HISTORY`):

- Cada `Transaction` almacena la posición del cursor antes/después y un vector de `CellPatch` (`cid`, `before_raw`, `after_raw`) únicamente de las celdas modificadas.
- Al agrupar los cambios por transacción, editar o pegar un rango completo de 20 celdas se deshace o rehace con un solo `Ctrl+Z` / `Ctrl+Y`, y el motor vuelve a propagar las dependencias automáticamente.

#### 3. Portapapeles con ajuste relativo ([`CellClipboard`](../src/texcel/helpers/state.h#L162-L179))
Cuando copias (`Ctrl+C`) o cortas un bloque de celdas, `CellClipboard` guarda:
- La esquina de origen (`src_c0`, `src_r0`), dimensiones (`cols x rows`), el texto crudo de cada celda (`raw_cells`) y una copia en TSV (`tsv_snapshot`).
- Al pegar en otra parte de la hoja (`Ctrl+V`), calcula el desplazamiento `(dcol, drow)` y ajusta automáticamente las referencias relativas dentro de las fórmulas con `SpreadsheetEngine::shiftFormulaRefs` ([`state.h:735`](../src/texcel/helpers/state.h#L735)), igual que en Excel (respetando referencias fijas con `$`).

#### 4. Inspector AST en vivo ([`renderInspectorWindow`](../src/texcel/helpers/renderer.h#L477-L525))
El panel derecho muestra en tiempo real la radiografía de la celda seleccionada (o de la fórmula que estás escribiendo en ese instante si `preview.active` está encendido):

- Muestra el tipo (`Fórmula`, `Número`, `Texto` o `Vacía`), la fórmula original, el resultado y las dependencias (`formatDependenciesBounded`).
- Además, convierte el árbol `BinaryTreeNode<AstToken>` en un gráfico ASCII/Unicode mediante `HorizontalTreeLayout` o `VerticalTreeLayout` ([`renderer.h:430-447`](../src/texcel/helpers/renderer.h#L430-L447)), coloreando la raíz, los operadores intermedios y las hojas para que puedas ver la estructura exacta de tu fórmula mientras la editas.

---

### Parte 8: Persistencia en archivos `.txl` y configuración ([`config.h`](../src/texcel/helpers/config.h))

Para que tus hojas no se pierdan al cerrar la terminal, Texcel separa los datos de cada hoja (`.txl`) de las preferencias del usuario (`.config`), ambos dentro de `output/sheets/`.

#### 1. ¿Qué se guarda en un `.txl`? ([`saveSheet`](../src/texcel/helpers/state.h#L1373-L1405) y [`loadSheet`](../src/texcel/helpers/state.h#L1407-L1432))
Un archivo `.txl` no guarda resultados calculados ni árboles AST, sino únicamente la fuente de verdad: **`engine.rawTable()`** (el mapa `celda -> texto crudo`).

- **Serialización directa:** como `RawTable` (`HashTable`) implementa los mixins `Printable` y `Readable` del proyecto, guardar es literalmente `ofs << engine.rawTable()` y cargar es `ifs >> engine.rawTable()`.
- **Escritura atómica (`.txl.tmp` + `rename`):** para evitar que un fallo o cierre brusco deje el archivo a medias o corrupto, `saveSheet` escribe primero todo en `nombre.txl.tmp` y, solo si la escritura fue exitosa, lo renombra sobre `nombre.txl`.
- **Reconstrucción al abrir:** al cargar con `loadSheet`, muestra un `TexcelLoadingSpinner`, limpia el motor, lee `rawTable()`, llama a [`recalculateAll`](../src/texcel/engine.h#L1282-L1397) para reconstruir y evaluar todas las fórmulas en paralelo, y reinicia el historial de `Undo/Redo`.

#### 2. Configuración persistente y anchos de columna ([`TexcelConfig`](../src/texcel/helpers/config.h#L26-L165))
[`TexcelConfig`](../src/texcel/helpers/config.h#L26-L165) administra el listado de archivos `.txl` (`refreshSheetList`) y guarda un archivo `.config` (también de forma atómica con `.config.tmp` + `rename`) con:

- **`autosave=0|1`:** si está activo, cada edición guarda automáticamente sin esperar a `Ctrl+S`.
- **`current=<hoja>`:** recuerda cuál fue la última hoja abierta para cargarla al iniciar.
- **`colw.<hoja>=col:ancho,...`:** guarda solo las columnas cuyo ancho fue redimensionado por el usuario (acotado entre `MIN_COL_WIDTH = 5` y `MAX_COL_WIDTH = 38`, frente al ancho base `DEFAULT_COL_WIDTH = 9`).

---

## Bloque IV: Funciones avanzadas, concurrencia e infraestructura de soporte
*¿Cómo se centralizan las funciones, se paraleliza el trabajo y se conecta la aplicación con la terminal y el sistema?*

### Parte 9: Catálogo unificado de funciones, rangos y criterios ([`types.h`](../src/texcel/types.h) y [`engine.h`](../src/texcel/engine.h))

Para cerrar el recorrido por Texcel, veamos cómo se definen las 17 funciones disponibles y cómo se evalúan rangos y criterios sin desperdiciar tiempo en celdas vacías.

#### 1. Un solo catálogo para parser, motor y TUI ([`TEXCEL_FUNCTIONS`](../src/texcel/types.h#L194-L212))
En lugar de duplicar la lista de funciones en el parser, en el evaluador y en la pantalla de ayuda, [`types.h:182-212`](../src/texcel/types.h#L182-L212) define una única tabla `constexpr` (`TEXCEL_FUNCTIONS`) donde cada entrada `TexcelFuncInfo` reúne:

- **Reglas gramaticales:** `name`, `id` (`FuncId`), `min_args`, `max_args` (`0` = ilimitados) y `even_args` (exige pares `rango, criterio` en `COUNTIFS`).
- **Ayuda visual:** `summary`, `syntax`, `detail` y `example`.

Así, cuando el parser valida argumentos, el motor ejecuta la función, o la TUI muestra *calltips* y el modal `Ctrl+F`, todos consultan exactamente la misma fuente de verdad mediante `findTexcelFunc`.

#### 2. Recorrer rangos en funciones agregadas ([`forEachArgValue`](../src/texcel/engine.h#L461-L484))
Funciones como `SUM`, `AVG`, `MIN`, `MAX`, `COUNT` o `CONCAT` pueden recibir tanto valores sueltos como rangos (`A1:B10`). [`forEachArgValue`](../src/texcel/engine.h#L461-L484) unifica ambos casos pasando al callback dos banderas clave:

- **`from_range`:** indica si el valor vino de expandir un `CellRange` o si fue un argumento puntual. Esto permite que `SUM(A1:A5)` ignore celdas con texto o vacías dentro del rango, mientras que `SUM("hola")` sí devuelva `#VALUE!`.
- **`is_empty`:** permite distinguir una celda realmente vacía de un `0` explícito (fundamental en `COUNT` frente a `COUNTA`).

#### 3. Cómo optimiza `COUNTIF` y `COUNTIFS` ([`evalCountIf`](../src/texcel/engine.h#L494-L570))
Evaluar criterios sobre rangos grandes podría ser costoso si se hiciera de forma ingenua. [`evalCountIf`](../src/texcel/engine.h#L494-L570) aplica tres optimizaciones concretas:

1. **Precompilación y validación de forma ([`engine.h:504-532`](../src/texcel/engine.h#L504-L532)):** verifica que todos los rangos tengan las mismas dimensiones (`base_cols x base_rows`; si no, devuelve `#VALUE!`) y compila cada criterio una sola vez (`compileCriterion`) antes de entrar al bucle.
2. **Recorte al área usada ([`engine.h:534-550`](../src/texcel/engine.h#L534-L550)):** si escribiste `=COUNTIF(A1:A999, ">10")` pero la hoja solo tiene datos hasta la fila 20 (`m_max_used_row`), y el criterio no acepta celdas vacías (`!all_match_empty`), recorta automáticamente los límites del bucle para no recorrer cientos de celdas vacías.
3. **Evaluación sincronizada con corte temprano ([`engine.h:552-569`](../src/texcel/engine.h#L552-L569)):** avanza por desplazamiento `(dc, dr)` leyendo la misma posición relativa en cada rango y sale con `break` en cuanto uno de los criterios no se cumple.

---

### Parte 10: Concurrencia, previsualización y utilidades de soporte ([`engine.h`](../src/texcel/engine.h), [`clipboard.h`](../src/texcel/helpers/clipboard.h), [`text.h`](../src/texcel/helpers/text.h), [`loading_anim.h`](../src/texcel/helpers/loading_anim.h))

Debajo de las funciones visibles, Texcel se apoya en cuatro piezas de infraestructura reutilizables:

#### 1. Pool de hilos persistente ([`TexcelThreadPool`](../src/texcel/engine.h#L30-L104))
Crear y destruir hilos del sistema operativo en cada operación es costoso. Por eso `engine.h` mantiene un *singleton* (`TexcelThreadPool::instance()`) con un máximo de `1..8` hilos (`std::thread::hardware_concurrency()`):

- Los hilos trabajadores permanecen vivos esperando en una `std::condition_variable` y reparten tareas mediante un contador atómico (`m_next_idx`).
- Este mismo pool impulsa tanto la construcción paralela de los árboles AVL en `recalculateAll` como la evaluación concurrente de Kahn en `evalDirtyGraphParallel`.

#### 2. Previsualización segura y desplazamiento de referencias ([`previewCommand`](../src/texcel/engine.h#L1556) y [`shiftFormulaRefs`](../src/texcel/engine.h#L1612))
- **`previewCommand`:** permite evaluar en vivo lo que el usuario está escribiendo en la barra de fórmulas (y detectar errores o dependencias) **sin modificar** `m_cells` ni el grafo de dependencias de la hoja real.
- **`shiftFormulaRefs`:** al copiar y pegar un bloque de celdas, recorre la fórmula textual buscando referencias (`A1`, `$A$1`, `B$2`) y suma el desplazamiento `(dcol, drow)` únicamente a las coordenadas que no están fijadas con `$`.

#### 3. Portapapeles del sistema y utilidades de terminal ([`clipboard.h`](../src/texcel/helpers/clipboard.h) y [`text.h`](../src/texcel/helpers/text.h))
- **Doble canal de portapapeles ([`TexcelClipboard::copy`](../src/texcel/helpers/clipboard.h#L81-L90)):** al copiar celdas (`Ctrl+C`), emite la secuencia de escape `OSC 52` en Base64 (que funciona incluso a través de sesiones SSH) y, si detecta una herramienta nativa disponible (`wl-copy`, `xsel`, `xclip` o `clip.exe` en [`detectNativeClipboardCmd`](../src/texcel/helpers/clipboard.h#L64-L79)), también escribe en ella.
- **Control de terminal y texto Unicode ([`text.h`](../src/texcel/helpers/text.h)):** agrupa las secuencias ANSI para activar/desactivar ratón SGR y *bracketed paste*, así como `alignCell` y `clipPlain` para alinear y recortar texto respetando el ancho visual real en terminal.

#### 4. Animación de carga en segundo plano ([`TexcelLoadingSpinner`](../src/texcel/helpers/loading_anim.h#L32))
Cuando se abre un archivo `.txl` grande, `loadSheet` lanza `TexcelLoadingSpinner` en un hilo dedicado que dibuja un efecto de brillo (*shimmer*, [`loading_anim.h:44-65`](../src/texcel/helpers/loading_anim.h#L44-L65)) en la barra de estado mientras el hilo principal y el pool reconstruyen la hoja.

---

## Resumen: El viaje completo de una fórmula

Para unir todas las piezas en una sola imagen, esto es lo que ocurre desde que escribes `= A1 + B1` en la celda `C1` y presionas `Enter` hasta que ves el resultado en pantalla:

```text
  [Teclado] ─▶ tui.h / input.h ─▶ engine.setCell("C1", "= A1 + B1")
                                        │
        ┌───────────────────────────────┴───────────────────────────────┐
        ▼                                                               ▼
  1. Desuscribir y marcar                                      2. Parsear y evaluar
     dependientes (BFS dirty)                                     FormulaParser ─▶ AST ─▶ PrePostIter
        │                                                               │
        └───────────────────────────────┬───────────────────────────────┘
                                        ▼
                      3. Propagar cambios en cascada + UndoManager
                                        │
                                        ▼
                      4. renderAll() ─▶ frame_buf ─▶ Terminal sin parpadeo
```

1. **Entrada (`tui.h` y `input.h`):** `runInteractive` lee tus teclas en modo *raw*. Al presionar `Enter` en la consola de comandos, `handleCommandSubmit` envía la expresión al motor (y antes, mientras escribías, `renderAll` ya la iba previsualizando en vivo).
2. **Limpieza y marcado (`engine.h`):** `setCell` normaliza `"C1"`, borra sus dependencias anteriores (`unsubscribeInputs`) y ejecuta un BFS (`collectAndMarkDirtySubgraph`) para marcar `dirty = true` en todas las celdas que dependían de `C1`.
3. **Traducción (`parser.h`):** `FormulaParser` convierte `"= A1 + B1"` en un árbol binario de `AstToken` (respetando precedencias y el catálogo `TEXCEL_FUNCTIONS`), mientras anota `"A1"` y `"B1"` como dependencias de `C1`.
4. **Cálculo (`engine.h`):** `evaluateAstPostOrder` recorre el árbol con `PrePostIter` (en dos fases `Pre`/`Post`, protegiéndose de bucles con `visiting` para detectar `#CYCLE!`) y guarda el `CellEvalValue` resultante en `C1`.
5. **Cascada, historial y guardado (`engine.h`, `state.h`, `config.h`):** el motor recalcula las celdas dependientes marcadas como `dirty`, `UndoManager` registra el `CellPatch` para `Ctrl+Z`, y si `autosave` está activo se escribe el `.txl` de forma atómica.
6. **Pantalla (`renderer.h`):** `renderAll` dibuja la cuadrícula, el Inspector AST, la barra de comandos y el registro en un `frame_buf` en memoria y lo envía de golpe a la terminal.








