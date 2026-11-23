/**
 * @file types.h
 * @ingroup texcel
 * @brief Tipos base de celdas, nodos AST y utilidades compartidas para Texcel.
 */

#ifndef __TEXCEL_TYPES_H__
#define __TEXCEL_TYPES_H__

#include "../containers/trees/binary_tree.h"
#include "../containers/hash_table.h"
#include "../containers/vector.h"
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <cstdio>

namespace texcel_detail {
template <int (*Fn)(int)>
inline bool isChar(char ch) { return Fn(static_cast<unsigned char>(ch)) != 0; }

template <int (*Fn)(int)>
inline char toChar(char ch) { return static_cast<char>(Fn(static_cast<unsigned char>(ch))); }
} // namespace texcel_detail

/**
 * @struct CellCoord
 * @brief Coordenada 0-indexada `(col, row)` de una celda con conversión desde/hacia notación Excel (`A1`, `B1`, `D4`).
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

    /** @brief Convierte un índice de columna (`0 -> A`, `25 -> Z`) a etiqueta de letras. */
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
    static CellCoord parse(const std::string& raw) {
        std::string s;
        for (char c : raw) {
            if (!texcel_detail::isChar<std::isspace>(c) && c != '$')
                s += texcel_detail::toChar<std::toupper>(c);
        }
        if (s.size() < 2) return {};
        size_t i = 0;
        TX col_val = 0;
        while (i < s.size() && texcel_detail::isChar<std::isupper>(s[i])) {
            col_val = col_val * 26 + (s[i] - 'A' + 1);
            ++i;
        }
        if (i == 0 || i == s.size()) return {};
        TX row_val = 0;
        for (; i < s.size(); ++i) {
            if (!texcel_detail::isChar<std::isdigit>(s[i])) return {};
            row_val = row_val * 10 + (s[i] - '0');
        }
        if (row_val <= 0 || col_val <= 0 || col_val > MAX_COLS || row_val > MAX_ROWS) return {};
        return CellCoord(col_val - 1, row_val - 1, true);
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
    FuncCall      = 5, /**< Llamada a función (`SUM`, `AVG`, `MIN`, `MAX`, `CONCAT`, `IF`, `COUNTIF`, `COUNTIFS`). */
    ArgPair       = 6  /**< Espina binaria de argumentos `,` para `FuncCall`. */
};

/** @brief Identificador numérico de función soportada en fórmulas. */
enum class FuncId : uint8_t {
    None = 0, Sum, Avg, Min, Max, Concat, If, CountIf, CountIfs
};

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
 * @brief Valor polimórfico (numérico o texto) almacenado in-place en cada nodo del AST.
 */
struct CellEvalValue {
    bool        is_text = false;
    double      num = 0.0;
    std::string text;

    auto operator<=>(const CellEvalValue&) const = default;

    std::string asString() const {
        return is_text ? text : formatCellNumber(num);
    }
    double asNumber() const {
        return is_text ? 0.0 : num;
    }
};

/**
 * @struct AstToken
 * @brief Nodo almacenado dentro de `BinaryTreeNode<AstToken>` para evaluación in-place en `Order::PrePostOrder`.
 */
struct AstToken {
    AstNodeKind   kind = AstNodeKind::Number;
    std::string   text;       /**< Representación textual (`"A1"`, `"+"`, `"4"`, `"SUM"`). */
    char          op = 0;     /**< Operador cuando `kind == AstNodeKind::BinaryOp`. */
    FuncId        func_id = FuncId::None; /**< Identificador de función cuando `kind == AstNodeKind::FuncCall`. */
    CellCoord     range_a;    /**< Inicio de rango cuando `kind == AstNodeKind::CellRange`. */
    CellCoord     range_b;    /**< Fin de rango cuando `kind == AstNodeKind::CellRange`. */
    bool          expect_numeric = false; /**< Propagado hacia abajo en `Phase::Pre`. */
    CellEvalValue evaluated;              /**< Resultado calculado hacia arriba en `Phase::Post`. */

    auto operator<=>(const AstToken& other) const {
        return text <=> other.text;
    }

    friend std::ostream& operator<<(std::ostream& os, const AstToken& t) {
        return os << t.text;
    }
};

/**
 * @struct SpreadsheetCell
 * @brief Estado completo de una celda: expresión original, valor evaluado, errores y metadatos del AST.
 */
struct SpreadsheetCell {
    std::string   id;            /**< Coordenada canónica (`"A1"`). */
    std::string   raw_expression;/**< Fórmula o valor ingresado detrás de la celda (`"= A1 + 4"`). */
    CellEvalValue result;        /**< Resultado numérico o textual calculado. */
    bool          is_formula = false;
    bool          has_error = false;
    bool          dirty = true;  /**< Indica si el valor cacheado en `result` requiere recálculo. */
    std::string   error_msg;     /**< `#CYCLE!`, `#DIV/0!`, `#SYNTAX!`, `#REF!`. */
    std::string   postorder_rpn; /**< Secuencia en PostOrder generada por `BinaryTreeIterator`. */
    Vector<VectorAscTraits<std::string>> inputs; /**< Celdas referenciadas directamente. */

    auto operator<=>(const SpreadsheetCell& other) const {
        return id <=> other.id;
    }

    /** @brief Texto que debe mostrarse dentro de la celda en la matriz. */
    std::string displayValue() const {
        if (raw_expression.empty()) return "";
        if (has_error) return error_msg;
        return result.asString();
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
    Vector<VectorAscTraits<std::string>> referenced_cells;
};

namespace texcel_detail {
using StringVec = Vector<VectorAscTraits<std::string>>;
using StringSet = BaseHashTable<HashTableTraits<std::string, bool>, Callable>;
using AstNode   = BinaryTreeNode<AstToken>;

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isChar<std::isspace>(s[a])) ++a;
    while (b > a && isChar<std::isspace>(s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline std::string toUpper(std::string s) {
    for (char& c : s) c = toChar<std::toupper>(c);
    return s;
}

template <typename Container>
inline bool containsStr(const Container& c, const std::string& target) {
    for (const auto& node : c) {
        if (node.getValue() == target) return true;
    }
    return false;
}

template <typename Container>
inline void addUniqueStr(Container& c, const std::string& val) {
    if (!containsStr(c, val)) c.push_back(val, 0);
}

inline bool isPureNumber(const std::string& s) {
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
inline bool forEachCellInRange(const CellCoord& a, const CellCoord& b, Fn&& fn) {
    TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
    TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
    for (TX r = r_min; r <= r_max; ++r) {
        for (TX col = c_min; col <= c_max; ++col) {
            std::string cid = CellCoord(col, r, true).toId();
            if constexpr (std::is_same_v<std::invoke_result_t<Fn, const std::string&>, bool>) {
                if (!fn(cid)) return false;
            } else {
                fn(cid);
            }
        }
    }
    return true;
}
} // namespace texcel_detail

#endif // __TEXCEL_TYPES_H__
