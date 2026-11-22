/**
 * @file types.h
 * @ingroup texcel
 * @brief Tipos base de celdas, nodos AST y utilidades compartidas para Texcel.
 */

#ifndef __TEXCEL_TYPES_H__
#define __TEXCEL_TYPES_H__

#include "../containers/trees/binary_tree.h"
#include "../containers/vector.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <stdexcept>

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
        std::string s;
        TX n = c;
        while (n >= 0) {
            s.insert(s.begin(), static_cast<char>('A' + (n % 26)));
            n = (n / 26) - 1;
        }
        return s;
    }

    /** @brief Retorna el identificador canónico en mayúsculas (ej. `"A1"`, `"D4"`). */
    std::string toId() const {
        if (!valid) return "INVALID";
        return colToLabel(col) + std::to_string(row + 1);
    }

    /** @brief Parsea una cadena como `"a1"`, `"AA12"` o `"$D$4"` en `CellCoord`. */
    static CellCoord parse(const std::string& raw) {
        std::string s;
        for (char c : raw) {
            if (!std::isspace(static_cast<unsigned char>(c)) && c != '$')
                s += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (s.size() < 2) return {};
        size_t i = 0;
        TX col_val = 0;
        while (i < s.size() && std::isupper(static_cast<unsigned char>(s[i]))) {
            col_val = col_val * 26 + (s[i] - 'A' + 1);
            ++i;
        }
        if (i == 0 || i == s.size()) return {};
        TX row_val = 0;
        for (; i < s.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return {};
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

/** @brief Formatea un valor numérico de forma compacta (sin ceros decimales sobrantes). */
inline std::string formatCellNumber(double v) {
    if (std::isnan(v) || std::isinf(v)) return "#NUM!";
    if (std::abs(v - std::round(v)) < 1e-9) {
        return std::to_string(static_cast<long long>(std::round(v)));
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    std::string s = oss.str();
    while (s.size() > 1 && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

/**
 * @enum ExpectedType
 * @brief Tipo esperado propagado desde el nodo padre hacia sus hijos durante `Phase::Pre`.
 */
enum class ExpectedType : TX {
    Any    = 0, /**< Sin restricción (raíz o contexto libre). */
    Number = 1, /**< Contexto aritmético (`+`, `-`, `*`, `/`, `^`): texto no numérico produce `#VALUE!`. */
    Text   = 2  /**< Contexto textual (`&`, `CONCAT`). */
};

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
    std::string   text;       /**< Representación textual (`"A1"`, `"+"`, `"4"`, `"SUM(A1:B2)"`). */
    double        number = 0; /**< Valor numérico cuando `kind == AstNodeKind::Number`. */
    char          op = 0;     /**< Operador cuando `kind == AstNodeKind::BinaryOp`. */
    std::string   func_name;  /**< Nombre de función o contenido de `StringLiteral`. */
    std::string   range_start;/**< Inicio de rango (`A1`). */
    std::string   range_end;  /**< Fin de rango (`B2`). */
    ExpectedType  expected_type = ExpectedType::Any;       /**< Propagado hacia abajo en `Phase::Pre`. */
    CellEvalValue evaluated;                               /**< Resultado calculado hacia arriba en `Phase::Post`. */

    auto operator<=>(const AstToken& other) const {
        return text <=> other.text;
    }

    friend std::ostream& operator<<(std::ostream& os, const AstToken& t) {
        return os << t.text;
    }
    friend std::istream& operator>>(std::istream& is, AstToken& t) {
        return is >> t.text;
    }
};

/**
 * @struct SpreadsheetCell
 * @brief Estado completo de una celda: expresión original, valor evaluado, errores y metadatos del AST.
 */
struct SpreadsheetCell {
    std::string id;            /**< Coordenada canónica (`"A1"`). */
    std::string raw_expression;/**< Fórmula o valor ingresado detrás de la celda (`"A1 + 4"`). */
    double      value = 0.0;   /**< Resultado numérico calculado (`9`). */
    std::string text_value;    /**< Resultado textual cuando `is_text == true`. */
    bool        is_text = false;
    bool        is_formula = false;
    bool        has_error = false;
    std::string error_msg;     /**< `#CYCLE!`, `#DIV/0!`, `#SYNTAX!`, `#REF!`. */
    std::string postorder_rpn; /**< Secuencia en PostOrder generada por `BinaryTreeIterator`. */

    auto operator<=>(const SpreadsheetCell& other) const {
        return id <=> other.id;
    }

    /** @brief Formatea el valor numérico de forma compacta (sin ceros decimales sobrantes). */
    static std::string formatNumber(double v) {
        return formatCellNumber(v);
    }

    /** @brief Texto que debe mostrarse dentro de la celda en la matriz. */
    std::string displayValue() const {
        if (raw_expression.empty()) return "";
        if (has_error) return error_msg;
        if (is_text) return text_value;
        return formatNumber(value);
    }

    friend std::ostream& operator<<(std::ostream& os, const SpreadsheetCell& c) {
        return os << c.raw_expression;
    }
    friend std::istream& operator>>(std::istream& is, SpreadsheetCell& c) {
        return is >> c.raw_expression;
    }
};

/**
 * @struct EvalPreview
 * @brief Resultado de previsualización en vivo mientras el usuario escribe en la consola de comandos.
 */
struct EvalPreview {
    bool        active = false;
    bool        valid = false;
    bool        is_text = false;
    std::string target_cell;
    std::string expression;
    double      value = 0.0;
    std::string formatted_value;
    std::string error_msg;
    std::string postorder_rpn;
    Vector<VectorAscTraits<std::string>> referenced_cells;
};

namespace texcel_detail {
using StringVec = Vector<VectorAscTraits<std::string>>;
using AstNode   = BinaryTreeNode<AstToken>;

inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

inline std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline bool containsStr(const StringVec& vec, const std::string& target) {
    for (size_t i = 0; i < vec.size(); ++i) {
        if (vec[i].getValue() == target) return true;
    }
    return false;
}

inline void addUniqueStr(StringVec& vec, const std::string& val) {
    if (!containsStr(vec, val)) vec.push_back(val);
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
} // namespace texcel_detail

#endif // __TEXCEL_TYPES_H__
