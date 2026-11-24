/**
 * @file types.h
 * @ingroup texcel
 * @brief Tipos base de celdas, rangos, catálogo unificado de funciones y nodos AST para Texcel.
 */

#ifndef __TEXCEL_TYPES_H__
#define __TEXCEL_TYPES_H__

#include "../containers/trees/binary_tree.h"
#include "../containers/hash_table.h"
#include "../containers/vector.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace texcel_detail {
template <int (*Fn)(int)>
inline bool isChar(char ch) { return Fn(static_cast<unsigned char>(ch)) != 0; }

template <int (*Fn)(int)>
inline char toChar(char ch) { return static_cast<char>(Fn(static_cast<unsigned char>(ch))); }
} // namespace texcel_detail

/**
 * @struct CellCoord
 * @brief Coordenada 0-indexada `(col, row)` de una celda con conversión desde/hacia notación Excel (`A1`, `ZZ999`).
 */
struct CellCoord {
    static constexpr TX MAX_COLS = 702; /**< Columnas soportadas: `A..ZZ` (`702`). */
    static constexpr TX MAX_ROWS = 999; /**< Filas soportadas: `1..999` (`999`). */

    TX col = 0; /**< Columna 0-indexada (`0 = A`, `1 = B`, ...). */
    TX row = 0; /**< Fila 0-indexada (`0 = fila 1`, `1 = fila 2`, ...). */
    bool valid = false;

    CellCoord() = default;
    CellCoord(TX c, TX r, bool v = true) : col(c), row(r), valid(v) {}

    auto operator<=>(const CellCoord&) const = default;

    /** @brief Convierte un índice de columna (`0 -> A`, `25 -> Z`, `26 -> AA`) a etiqueta de letras. */
    static std::string colToLabel(TX c) {
        if (c < 0) return "?";
        char rev[8];
        int len = 0;
        TX n = c;
        while (n >= 0 && len < 8) {
            rev[len++] = static_cast<char>('A' + (n % 26));
            n = (n / 26) - 1;
        }
        char buf[8];
        for (int i = 0; i < len; ++i) buf[i] = rev[len - 1 - i];
        return std::string(buf, static_cast<size_t>(len));
    }

    /** @brief Retorna el identificador canónico en mayúsculas (ej. `"A1"`, `"D4"`). */
    std::string toId() const {
        if (!valid) return "INVALID";
        char buf[16];
        char rev[12];
        int len = 0;
        TX n = col;
        while (n >= 0 && len < 8) {
            rev[len++] = static_cast<char>('A' + (n % 26));
            n = (n / 26) - 1;
        }
        int pos = 0;
        for (int i = len - 1; i >= 0; --i) buf[pos++] = rev[i];
        TX r = row + 1;
        len = 0;
        do {
            rev[len++] = static_cast<char>('0' + (r % 10));
            r /= 10;
        } while (r > 0 && len < 12);
        for (int i = len - 1; i >= 0; --i) buf[pos++] = rev[i];
        return std::string(buf, static_cast<size_t>(pos));
    }

    /** @brief Parsea una cadena como `"a1"`, `"AA12"` o `"$D$4"` en `CellCoord`. */
    static CellCoord parse(std::string_view raw) {
        char s[16];
        size_t slen = 0;
        for (char c : raw) {
            if (!texcel_detail::isChar<std::isspace>(c) && c != '$') {
                if (slen >= sizeof(s)) return {};
                s[slen++] = texcel_detail::toChar<std::toupper>(c);
            }
        }
        if (slen < 2) return {};
        size_t i = 0;
        TX col_val = 0;
        while (i < slen && texcel_detail::isChar<std::isupper>(s[i])) {
            col_val = col_val * 26 + (s[i] - 'A' + 1);
            ++i;
        }
        if (i == 0 || i == slen) return {};
        TX row_val = 0;
        for (; i < slen; ++i) {
            if (!texcel_detail::isChar<std::isdigit>(s[i])) return {};
            row_val = row_val * 10 + (s[i] - '0');
        }
        if (row_val <= 0 || col_val <= 0 || col_val > MAX_COLS || row_val > MAX_ROWS) return {};
        return CellCoord(col_val - 1, row_val - 1, true);
    }
};

/**
 * @struct CellRangeRect
 * @brief Rango rectangular normalizado `(c0, r0)..(c1, r1)` para suscripción e inspección en $O(1)$.
 */
struct CellRangeRect {
    TX c0 = 0, r0 = 0, c1 = 0, r1 = 0;

    CellRangeRect() = default;
    CellRangeRect(const CellCoord& a, const CellCoord& b)
        : c0(std::min(a.col, b.col)),
          r0(std::min(a.row, b.row)),
          c1(std::max(a.col, b.col)),
          r1(std::max(a.row, b.row)) {}
    CellRangeRect(TX col0, TX row0, TX col1, TX row1)
        : c0(std::min(col0, col1)),
          r0(std::min(row0, row1)),
          c1(std::max(col0, col1)),
          r1(std::max(row0, row1)) {}

    auto operator<=>(const CellRangeRect&) const = default;

    bool contains(TX col, TX row) const {
        return col >= c0 && col <= c1 && row >= r0 && row <= r1;
    }

    TX cellCount() const {
        return (c1 - c0 + 1) * (r1 - r0 + 1);
    }

    std::string toId() const {
        std::string a = CellCoord(c0, r0, true).toId();
        std::string b = CellCoord(c1, r1, true).toId();
        return (a == b) ? a : (a + ":" + b);
    }

    friend std::ostream& operator<<(std::ostream& os, const CellRangeRect& r) {
        return os << r.toId();
    }
};

/**
 * @enum AstNodeKind
 * @brief Tipo semántico de cada nodo en el árbol binario de sintaxis abstracta (`BinaryTreeNode<AstToken>`).
 */
enum class AstNodeKind : TX {
    Number        = 0, /**< Literal numérico (ej. `5`, `4.5`). */
    CellRef       = 1, /**< Referencia a otra celda (ej. `A1`, `B1`). */
    CellRange     = 2, /**< Rango rectangular de celdas (ej. `A1:B3`). */
    BinaryOp      = 3, /**< Operador binario (`+`, `-`, `*`, `/`, `^`, `&`, `=`, `<>`, `<`, `>`, `<=`, `>=`). */
    StringLiteral = 4, /**< Literal de texto entre comillas o booleano (`"Hola"`, `TRUE`). */
    FuncCall      = 5, /**< Llamada a función del catálogo unificado. */
    ArgPair       = 6  /**< Espina binaria de argumentos `,` para `FuncCall`. */
};

/** @brief Identificador numérico de función soportada en fórmulas. */
enum class FuncId : uint8_t {
    None = 0,
    Sum, Avg, Min, Max, Count, CountA,
    Round, Abs, Sqrt,
    If, IfError, And, Or, Not,
    Concat, CountIf, CountIfs
};

/**
 * @struct TexcelFuncInfo
 * @brief Especificación unificada (gramática + documentación TUI) de cada función de Texcel.
 */
struct TexcelFuncInfo {
    const char* name;
    FuncId      id;
    size_t      min_args;
    size_t      max_args; /**< `0` = número ilimitado de argumentos. */
    bool        even_args;
    const char* summary;
    const char* syntax;
    const char* detail;
    const char* example;
};

inline constexpr TexcelFuncInfo TEXCEL_FUNCTIONS[] = {
    {"SUM",      FuncId::Sum,      1, 0, false, "Suma valores o rangos",      "SUM(rango | v1, v2, ...)",       "Suma todos los valores numéricos.",                       "= SUM(A1:A5)"},
    {"AVG",      FuncId::Avg,      1, 0, false, "Promedio aritmético",        "AVG(rango | v1, v2, ...)",       "Calcula la media aritmética de los valores numéricos.",   "= AVG(B1:B5)"},
    {"MIN",      FuncId::Min,      1, 0, false, "Valor mínimo",               "MIN(rango | v1, v2, ...)",       "Devuelve el menor valor numérico.",                       "= MIN(A1:A10)"},
    {"MAX",      FuncId::Max,      1, 0, false, "Valor máximo",               "MAX(rango | v1, v2, ...)",       "Devuelve el mayor valor numérico.",                       "= MAX(A1:A10)"},
    {"COUNT",    FuncId::Count,    1, 0, false, "Cuenta celdas numéricas",    "COUNT(rango | v1, v2, ...)",     "Cuenta cuántas celdas contienen números.",                "= COUNT(B2:B20)"},
    {"COUNTA",   FuncId::CountA,   1, 0, false, "Cuenta celdas no vacías",    "COUNTA(rango | v1, v2, ...)",    "Cuenta celdas con cualquier valor (texto o número).",     "= COUNTA(A2:A20)"},
    {"ROUND",    FuncId::Round,    1, 2, false, "Redondea decimales",         "ROUND(valor, [decimales])",      "Redondea un número a la cantidad de decimales indicada.", "= ROUND(F5, 1)"},
    {"ABS",      FuncId::Abs,      1, 1, false, "Valor absoluto",             "ABS(valor)",                     "Devuelve el valor absoluto sin signo.",                   "= ABS(A1 - B1)"},
    {"SQRT",     FuncId::Sqrt,     1, 1, false, "Raíz cuadrada",              "SQRT(valor)",                    "Calcula la raíz cuadrada no negativa (#NUM! si < 0).",    "= SQRT(A1)"},
    {"IF",       FuncId::If,       2, 3, false, "Condición lógica",           "IF(cond, si_ok, [si_no])",       "Evalúa la condición con cortocircuito real de ramas.",    "= IF(A1>=11, \"OK\", \"NO\")"},
    {"IFERROR",  FuncId::IfError,  2, 2, false, "Captura errores de fórmula", "IFERROR(valor, si_error)",       "Devuelve si_error cuando valor produce #DIV/0!, etc.",    "= IFERROR(A1/B1, 0)"},
    {"AND",      FuncId::And,      1, 0, false, "Conjunción lógica (Y)",      "AND(cond1, cond2, ...)",         "True si todos los argumentos evaluados son verdaderos.",  "= AND(A1>=11, B1>=11)"},
    {"OR",       FuncId::Or,       1, 0, false, "Disyunción lógica (O)",      "OR(cond1, cond2, ...)",          "True si al menos un argumento es verdadero.",             "= OR(A1=20, B1=20)"},
    {"NOT",      FuncId::Not,      1, 1, false, "Negación lógica",            "NOT(cond)",                      "Invierte un valor lógico (True <-> False).",              "= NOT(R5)"},
    {"CONCAT",   FuncId::Concat,   1, 0, false, "Une textos y valores",       "CONCAT(v1, v2, ...)",            "Concatena textos, números o rangos (también &).",         "= CONCAT(\"Total: \", D4)"},
    {"COUNTIF",  FuncId::CountIf,  2, 2, false, "Cuenta con 1 criterio",      "COUNTIF(rango, criterio)",       "Cuenta celdas dentro del rango que cumplen el criterio.", "= COUNTIF(A1:A10, \">5\")"},
    {"COUNTIFS", FuncId::CountIfs, 2, 0, true,  "Cuenta con varios criterios","COUNTIFS(r1, c1, r2, c2, ...)",  "Cuenta posiciones donde se cumplen todos los criterios.", "= COUNTIFS(A1:A5, \">10\", B1:B5, \"OK\")"},
};
inline constexpr TX TEXCEL_FUNCTIONS_COUNT = static_cast<TX>(sizeof(TEXCEL_FUNCTIONS) / sizeof(TEXCEL_FUNCTIONS[0]));

inline const TexcelFuncInfo* findTexcelFunc(std::string_view upper_ident) {
    for (const auto& fn : TEXCEL_FUNCTIONS) {
        if (upper_ident == fn.name) return &fn;
    }
    return nullptr;
}

/** @brief Formatea un valor numérico de forma compacta (sin ceros decimales sobrantes). */
inline std::string formatCellNumber(double v) {
    if (std::isnan(v) || std::isinf(v)) return "#NUM!";
    if (std::abs(v - std::round(v)) < 1e-9) {
        return std::to_string(static_cast<long long>(std::round(v)));
    }
    char buf[48];
    int n = std::snprintf(buf, sizeof(buf), "%.2f", v);
    if (n <= 0) return "0";
    std::string s(buf, static_cast<size_t>(n));
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

/**
 * @struct CellEvalValue
 * @brief Valor polimórfico (numérico, texto o código de error) almacenado in-place en cada nodo del AST.
 */
struct CellEvalValue {
    bool        is_text = false;
    double      num = 0.0;
    std::string text;
    std::string err_code;

    auto operator<=>(const CellEvalValue&) const = default;

    bool hasError() const { return !err_code.empty(); }

    static CellEvalValue makeError(std::string code) {
        CellEvalValue v;
        v.err_code = std::move(code);
        return v;
    }

    std::string asString() const {
        if (hasError()) return err_code;
        return is_text ? text : formatCellNumber(num);
    }
    double asNumber() const {
        return (hasError() || is_text) ? 0.0 : num;
    }
};

/**
 * @struct AstToken
 * @brief Nodo almacenado dentro de `BinaryTreeNode<AstToken>` para evaluación in-place en `Order::PrePostOrder`.
 */
struct AstToken {
    AstNodeKind   kind = AstNodeKind::Number;
    std::string   text;                  /**< Representación textual (`"A1"`, `"+"`, `"4"`, `"SUM"`). */
    char          op = 0;                /**< Operador cuando `kind == AstNodeKind::BinaryOp`. */
    FuncId        func_id = FuncId::None;/**< Identificador de función cuando `kind == AstNodeKind::FuncCall`. */
    CellCoord     range_a;               /**< Inicio de rango cuando `kind == AstNodeKind::CellRange`. */
    CellCoord     range_b;               /**< Fin de rango cuando `kind == AstNodeKind::CellRange`. */
    bool          expect_numeric = false;/**< Propagado hacia abajo en `Phase::Pre`. */
    bool          skip_eval = false;     /**< Cortocircuito propagado en `Phase::Pre` para ramas descartadas (`IF`/`IFERROR`). */
    CellEvalValue evaluated;             /**< Resultado calculado hacia arriba en `Phase::Post`. */

    auto operator<=>(const AstToken& other) const {
        return text <=> other.text;
    }

    friend std::ostream& operator<<(std::ostream& os, const AstToken& t) {
        return os << t.text;
    }
};

/**
 * @struct SpreadsheetCell
 * @brief Estado completo de una celda: expresión original, valor evaluado, errores y referencias compactas.
 */
struct SpreadsheetCell {
    std::string   id;             /**< Coordenada canónica (`"A1"`). */
    std::string   raw_expression; /**< Fórmula o valor ingresado detrás de la celda (`"= A1 + 4"`). */
    CellEvalValue result;         /**< Resultado numérico o textual calculado. */
    bool          is_formula = false;
    bool          has_error = false;
    bool          dirty = true;   /**< Indica si el valor cacheado en `result` requiere recálculo. */
    std::string   error_msg;      /**< `#CYCLE!`, `#DIV/0!`, `#SYNTAX!`, `#VALUE!`, `#NUM!`. */
    std::string   postorder_rpn;  /**< Secuencia en PostOrder generada por `BinaryTreeIterator`. */
    Vector<VectorAscTraits<std::string>>   inputs;       /**< Celdas puntuales referenciadas directamente. */
    Vector<VectorAscTraits<CellRangeRect>> range_inputs; /**< Rangos rectangulares referenciados (`K1:ZZ1`). */

    auto operator<=>(const SpreadsheetCell& other) const {
        return id <=> other.id;
    }

    /** @brief Texto que debe mostrarse dentro de la celda en la matriz. */
    std::string displayValue() const {
        if (raw_expression.empty()) return "";
        if (has_error) return error_msg;
        return result.asString();
    }

    bool referencesCoord(TX col, TX row, const std::string& cid) const {
        for (size_t i = 0; i < range_inputs.size(); ++i) {
            if (range_inputs[i].getValue().contains(col, row)) return true;
        }
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (inputs[i].getValue() == cid) return true;
        }
        return false;
    }

    friend std::ostream& operator<<(std::ostream& os, const SpreadsheetCell& c) {
        return os << c.raw_expression;
    }
};

/**
 * @struct EvalPreview
 * @brief Resultado de previsualización en vivo mientras el usuario escribe en la consola de comandos.
 */
struct EvalPreview {
    bool          active = false;
    bool          valid = false;
    std::string   target_cell;
    std::string   expression;
    CellEvalValue result;
    std::string   error_msg;
    std::string   postorder_rpn;
    Vector<VectorAscTraits<std::string>>   referenced_cells;
    Vector<VectorAscTraits<CellRangeRect>> referenced_ranges;

    bool referencesCoord(TX col, TX row, const std::string& cid) const {
        for (size_t i = 0; i < referenced_ranges.size(); ++i) {
            if (referenced_ranges[i].getValue().contains(col, row)) return true;
        }
        for (size_t i = 0; i < referenced_cells.size(); ++i) {
            if (referenced_cells[i].getValue() == cid) return true;
        }
        return false;
    }
};

namespace texcel_detail {
using StringVec = Vector<VectorAscTraits<std::string>>;
using RangeVec  = Vector<VectorAscTraits<CellRangeRect>>;
using StringSet = BaseHashTable<HashTableTraits<std::string, bool>, Callable>;
using AstNode   = BinaryTreeNode<AstToken>;

inline std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && isChar<std::isspace>(s[a])) ++a;
    while (b > a && isChar<std::isspace>(s[b - 1])) --b;
    return std::string(s.substr(a, b - a));
}

inline std::string toUpper(std::string s) {
    for (char& c : s) c = toChar<std::toupper>(c);
    return s;
}

inline bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (toChar<std::toupper>(a[i]) != toChar<std::toupper>(b[i])) return false;
    }
    return true;
}

template <typename Container, typename T>
inline bool containsVal(const Container& c, const T& target) {
    for (const auto& node : c) {
        if (node.getValue() == target) return true;
    }
    return false;
}

template <typename Container>
inline bool containsStr(const Container& c, const std::string& target) {
    return containsVal(c, target);
}

template <typename Container>
inline void addUniqueStr(Container& c, const std::string& val) {
    if (!containsStr(c, val)) c.push_back(val, 0);
}

inline void addUniqueRange(RangeVec& c, const CellRangeRect& r) {
    if (!containsVal(c, r)) c.push_back(r, 0);
}

inline bool isPureNumber(std::string_view s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    bool has_digit = false, has_dot = false;
    for (; i < s.size(); ++i) {
        if (isChar<std::isdigit>(s[i])) {
            has_digit = true;
        } else if (s[i] == '.' && !has_dot) {
            has_dot = true;
        } else {
            return false;
        }
    }
    return has_digit;
}

inline bool isTruthyValue(const CellEvalValue& v) {
    if (v.hasError()) return false;
    if (!v.is_text) return std::abs(v.num) > 1e-9;
    if (equalsIgnoreCase(v.text, "TRUE")) return true;
    if (equalsIgnoreCase(v.text, "FALSE") || v.text.empty()) return false;
    if (isPureNumber(v.text)) return std::abs(std::stod(v.text)) > 1e-9;
    return false;
}

/** @brief Compara dos valores (numéricos con tolerancia o textuales) usando código de operador (`=`, `!`, `<`, `>`, `L`, `G`). */
inline bool compareValues(char op_code, const CellEvalValue& lhs, const CellEvalValue& rhs) {
    constexpr double EPS = 1e-9;
    if (!lhs.is_text && !rhs.is_text) {
        switch (op_code) {
            case '=': return std::abs(lhs.num - rhs.num) < EPS;
            case '!': return std::abs(lhs.num - rhs.num) >= EPS;
            case '<': return lhs.num < rhs.num - EPS;
            case '>': return lhs.num > rhs.num + EPS;
            case 'L': return lhs.num <= rhs.num + EPS;
            case 'G': return lhs.num >= rhs.num - EPS;
            default:  return false;
        }
    }
    std::string ls = lhs.asString(), rs = rhs.asString();
    switch (op_code) {
        case '=': return ls == rs;
        case '!': return ls != rs;
        case '<': return ls < rs;
        case '>': return ls > rs;
        case 'L': return ls <= rs;
        case 'G': return ls >= rs;
        default:  return false;
    }
}

struct OpInfo {
    const char* lexeme;
    char        code;
    TX          prec;
    bool        numeric = true;
    bool        right_assoc = false;
};

inline constexpr OpInfo BINARY_OPS[] = {
    // 1. comparación
    {"<>", '!', 1, false}, {"!=", '!', 1, false},
    {"<=", 'L', 1, false}, {">=", 'G', 1, false},
    {"=",  '=', 1, false}, {"<",  '<', 1, false}, {">",  '>', 1, false},
    // 2. concatenación
    {"&",  '&', 2, false},
    // 3. suma y resta
    {"+",  '+', 3}, {"-",  '-', 3},
    // 4. división y multiplicación
    {"*",  '*', 4}, {"/",  '/', 4},
    // 5. potenciación (asociativa a la derecha)
    {"^",  '^', 5, true, true}
};

inline bool isBinaryOpChar(char ch) {
    for (const auto& op : BINARY_OPS) {
        if (op.lexeme[0] == ch && op.lexeme[1] == '\0') return true;
    }
    return false;
}

inline bool binaryOpIsNumeric(char code) {
    for (const auto& op : BINARY_OPS) {
        if (op.code == code) return op.numeric;
    }
    return false;
}

inline void freeAst(AstNode* node) {
    if (!node) return;
    freeAst(node->m_pChild[0]);
    freeAst(node->m_pChild[1]);
    delete node;
}

inline AstNode* makeBinaryNode(const AstToken& tok, AstNode* left, AstNode* right) {
    AstNode* n = new AstNode(tok, 0);
    n->m_pChild[0] = left;
    n->m_pChild[1] = right;
    if (left)  left->m_parent = n;
    if (right) right->m_parent = n;
    return n;
}

template <typename Fn>
inline bool forEachCoordInRange(const CellCoord& a, const CellCoord& b, Fn&& fn) {
    TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
    TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
    for (TX r = r_min; r <= r_max; ++r) {
        for (TX col = c_min; col <= c_max; ++col) {
            if constexpr (std::is_same_v<std::invoke_result_t<Fn, TX, TX>, bool>) {
                if (!fn(col, r)) return false;
            } else {
                fn(col, r);
            }
        }
    }
    return true;
}

template <typename Fn>
inline bool forEachCellInRange(const CellCoord& a, const CellCoord& b, Fn&& fn) {
    return forEachCoordInRange(a, b, [&](TX col, TX r) {
        std::string cid = CellCoord(col, r, true).toId();
        if constexpr (std::is_same_v<std::invoke_result_t<Fn, const std::string&>, bool>) {
            return fn(cid);
        } else {
            fn(cid);
            return true;
        }
    });
}
} // namespace texcel_detail

#endif // __TEXCEL_TYPES_H__
