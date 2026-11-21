/**
 * @file spreadsheet.h
 * @ingroup containers
 * @brief Motor de hoja de cálculo reactiva respaldado por `HashTable`, `BinaryTree` (AST Euler PostOrder), `LinkedList` y `Vector`.
 */

#ifndef __SPREADSHEET_H__
#define __SPREADSHEET_H__

#include "hash_table.h"
#include "trees/binary_tree.h"
#include "lists/linked_list.h"
#include "vector.h"
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

    /** @brief Parsea una cadena como `"a1"` o `"D4"` en `CellCoord`. */
    static CellCoord parse(const std::string& raw) {
        std::string s;
        for (char c : raw) {
            if (!std::isspace(static_cast<unsigned char>(c)))
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
        if (row_val <= 0 || col_val <= 0 || col_val > 26 || row_val > 99) return {};
        return CellCoord(col_val - 1, row_val - 1, true);
    }
};

/**
 * @enum AstNodeKind
 * @brief Tipo semántico de cada nodo en el árbol binario de sintaxis abstracta (`BinaryTreeNode<AstToken>`).
 */
enum class AstNodeKind : TX {
    Number  = 0, /**< Literal numérico (ej. `5`, `4.5`). */
    CellRef = 1, /**< Referencia a otra celda (ej. `A1`, `B1`). */
    BinaryOp = 2,/**< Operador binario (`+`, `-`, `*`, `/`, `^`). */
    FuncRange = 3/**< Función agregada sobre rango (`SUM`, `AVG`, `MIN`, `MAX`). */
};

/**
 * @struct AstToken
 * @brief Nodo almacenado dentro de `BinaryTreeNode<AstToken>` para evaluación en `Order::PostOrder`.
 */
struct AstToken {
    AstNodeKind kind = AstNodeKind::Number;
    std::string text;       /**< Representación textual (`"A1"`, `"+"`, `"4"`, `"SUM(A1:B2)"`). */
    double      number = 0; /**< Valor numérico cuando `kind == AstNodeKind::Number`. */
    char        op = 0;     /**< Operador cuando `kind == AstNodeKind::BinaryOp`. */
    std::string func_name;  /**< Nombre de función (`SUM`, `AVG`, `MIN`, `MAX`). */
    std::string range_start;/**< Inicio de rango (`A1`). */
    std::string range_end;  /**< Fin de rango (`B2`). */

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
    bool        is_formula = false;
    bool        has_error = false;
    std::string error_msg;     /**< `#CYCLE!`, `#DIV/0!`, `#SYNTAX!`, `#REF!`. */
    std::string postorder_rpn; /**< Secuencia en PostOrder generada por `BinaryTreeIterator`. */
    std::string ast_root_label;/**< Etiqueta de la raíz del AST para el inspector visual. */
    std::string ast_left_label;/**< Subárbol izquierdo resumido. */
    std::string ast_right_label;/**< Subárbol derecho resumido. */

    auto operator<=>(const SpreadsheetCell& other) const {
        return id <=> other.id;
    }

    /** @brief Formatea el valor numérico de forma compacta (sin ceros decimales sobrantes). */
    static std::string formatNumber(double v) {
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

    /** @brief Texto que debe mostrarse dentro de la celda en la matriz. */
    std::string displayValue() const {
        if (raw_expression.empty()) return "";
        if (has_error) return error_msg;
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
    std::string target_cell;
    std::string expression;
    double      value = 0.0;
    std::string formatted_value;
    std::string error_msg;
    std::string postorder_rpn;
    std::string ast_root;
    std::string ast_left;
    std::string ast_right;
    Vector<VectorAscTraits<std::string>> referenced_cells;
};

/**
 * @class SpreadsheetEngine
 * @brief Motor de hoja de cálculo basado en los contenedores propios del curso (`HashTable`, `BinaryTree`, `Vector`).
 *
 * - Almacena celdas dispersas en `HashTable`.
 * - Compila cada fórmula en un árbol de expresión con `BinaryTreeNode<AstToken>`.
 * - Evalúa las fórmulas recorriendo el árbol en `Order::PostOrder` mediante `BinaryTreeIterator`.
 * - Detecta referencias circulares (`#CYCLE!`) y recalcula automáticamente todas las celdas dependientes.
 */
class SpreadsheetEngine {
public:
    using StringVec   = Vector<VectorAscTraits<std::string>>;
    using DoubleVec   = Vector<VectorAscTraits<double>>;
    using DepList     = LinkedList<LinkedListAscTraits<std::string>>;
    using DepGraph    = BaseHashTable<HashTableTraits<std::string, DepList>, Callable>;
    using AstNode     = BinaryTreeNode<AstToken>;
    using PostIter    = BinaryTreeIterator<AstToken, Order::PostOrder, Direction::Asc>;
    using RawTable    = HashTable<HashTableTraits<std::string, std::string>>;
    using CellTable   = BaseHashTable<HashTableTraits<std::string, SpreadsheetCell>, Callable>;

private:
    RawTable  m_raw_table;  /**< Tabla serializable `(celda -> fórmula)` con mixins `Printable` y `Readable`. */
    CellTable m_cells;      /**< Tabla de estado evaluado de cada celda activa. */
    DepGraph  m_inputs;     /**< `celda -> LinkedList` de celdas que lee directamente. */
    DepGraph  m_dependents; /**< `celda -> LinkedList` de celdas observadoras que dependen de ella. */
    StringVec m_logs;       /**< Historial cronológico de operaciones y propagaciones. */

    static bool listContains(const DepList& lst, const std::string& target) {
        for (const auto& node : lst) {
            if (node.getValue() == target) return true;
        }
        return false;
    }

    static void listAddUnique(DepList& lst, const std::string& val) {
        if (!listContains(lst, val)) lst.push_back(val, 0);
    }

    static void listRemove(DepList& lst, const std::string& target) {
        DepList filtered;
        for (const auto& node : lst) {
            if (node.getValue() != target) {
                filtered.push_back(node.getValue(), 0);
            }
        }
        lst = std::move(filtered);
    }

    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    static std::string toUpper(std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }

    static bool containsStr(const StringVec& vec, const std::string& target) {
        for (size_t i = 0; i < vec.size(); ++i) {
            if (vec[i].getValue() == target) return true;
        }
        return false;
    }

    static void addUniqueStr(StringVec& vec, const std::string& val) {
        if (!containsStr(vec, val)) vec.push_back(val);
    }

    /** @brief Libera recursivamente un árbol de expresión `BinaryTreeNode<AstToken>`. */
    static void freeAst(AstNode* node) {
        if (!node) return;
        freeAst(node->m_pChild[0]);
        freeAst(node->m_pChild[1]);
        delete node;
    }

    /** @brief Conecta hijos izquierdo y derecho a un `BinaryTreeNode<AstToken>` padre. */
    static AstNode* makeBinaryNode(const AstToken& tok, AstNode* left, AstNode* right) {
        AstNode* n = new AstNode(tok, 0);
        n->m_pChild[0] = left;
        n->m_pChild[1] = right;
        if (left)  left->m_parent = n;
        if (right) right->m_parent = n;
        return n;
    }

    /**
     * @class FormulaParser
     * @brief Parser de descenso recursivo que construye un árbol `BinaryTreeNode<AstToken>`.
     */
    class FormulaParser {
        std::string m_src;
        size_t      m_pos = 0;
        StringVec&  m_refs;

        void skipSpaces() {
            while (m_pos < m_src.size() && std::isspace(static_cast<unsigned char>(m_src[m_pos])))
                ++m_pos;
        }

        char peek() {
            skipSpaces();
            return (m_pos < m_src.size()) ? m_src[m_pos] : '\0';
        }

        char get() {
            skipSpaces();
            return (m_pos < m_src.size()) ? m_src[m_pos++] : '\0';
        }

    public:
        FormulaParser(const std::string& src, StringVec& refs) : m_src(src), m_refs(refs) {}

        AstNode* parse() {
            skipSpaces();
            if (m_pos < m_src.size() && m_src[m_pos] == '=') ++m_pos;
            skipSpaces();
            if (m_pos >= m_src.size()) throw std::runtime_error("Expresión vacía");
            AstNode* root = parseExpr();
            skipSpaces();
            if (m_pos < m_src.size()) {
                freeAst(root);
                throw std::runtime_error("Token inesperado");
            }
            return root;
        }

    private:
        // Expr := Term { ('+' | '-') Term }
        AstNode* parseExpr() {
            AstNode* left = parseTerm();
            while (true) {
                char c = peek();
                if (c == '+' || c == '-') {
                    get();
                    AstNode* right = nullptr;
                    try {
                        right = parseTerm();
                    } catch (...) {
                        freeAst(left);
                        throw;
                    }
                    AstToken tok;
                    tok.kind = AstNodeKind::BinaryOp;
                    tok.op = c;
                    tok.text = std::string(1, c);
                    left = makeBinaryNode(tok, left, right);
                } else {
                    break;
                }
            }
            return left;
        }

        // Term := Power { ('*' | '/') Power }
        AstNode* parseTerm() {
            AstNode* left = parsePower();
            while (true) {
                char c = peek();
                if (c == '*' || c == '/') {
                    get();
                    AstNode* right = nullptr;
                    try {
                        right = parsePower();
                    } catch (...) {
                        freeAst(left);
                        throw;
                    }
                    AstToken tok;
                    tok.kind = AstNodeKind::BinaryOp;
                    tok.op = c;
                    tok.text = std::string(1, c);
                    left = makeBinaryNode(tok, left, right);
                } else {
                    break;
                }
            }
            return left;
        }

        // Power := Unary [ '^' Power ]
        AstNode* parsePower() {
            AstNode* left = parseUnary();
            if (peek() == '^') {
                get();
                AstNode* right = nullptr;
                try {
                    right = parsePower();
                } catch (...) {
                    freeAst(left);
                    throw;
                }
                AstToken tok;
                tok.kind = AstNodeKind::BinaryOp;
                tok.op = '^';
                tok.text = "^";
                left = makeBinaryNode(tok, left, right);
            }
            return left;
        }

        // Unary := ('+' | '-') Unary | Primary
        AstNode* parseUnary() {
            char c = peek();
            if (c == '+') {
                get();
                return parseUnary();
            }
            if (c == '-') {
                get();
                AstNode* operand = parseUnary();
                AstToken zero_tok;
                zero_tok.kind = AstNodeKind::Number;
                zero_tok.number = 0.0;
                zero_tok.text = "0";
                AstNode* zero_node = new AstNode(zero_tok, 0);

                AstToken op_tok;
                op_tok.kind = AstNodeKind::BinaryOp;
                op_tok.op = '-';
                op_tok.text = "-";
                return makeBinaryNode(op_tok, zero_node, operand);
            }
            return parsePrimary();
        }

        // Primary := '(' Expr ')' | Number | Function(A1:B2) | CellRef
        AstNode* parsePrimary() {
            skipSpaces();
            char c = peek();
            if (c == '(') {
                get();
                AstNode* inside = parseExpr();
                if (peek() != ')') {
                    freeAst(inside);
                    throw std::runtime_error("Falta ')'");
                }
                get();
                return inside;
            }

            // Literal numérico
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                size_t start = m_pos;
                bool has_dot = false;
                while (m_pos < m_src.size()) {
                    char ch = m_src[m_pos];
                    if (std::isdigit(static_cast<unsigned char>(ch))) {
                        ++m_pos;
                    } else if (ch == '.' && !has_dot) {
                        has_dot = true;
                        ++m_pos;
                    } else {
                        break;
                    }
                }
                std::string num_str = m_src.substr(start, m_pos - start);
                double val = std::stod(num_str);
                AstToken tok;
                tok.kind = AstNodeKind::Number;
                tok.number = val;
                tok.text = SpreadsheetCell::formatNumber(val);
                return new AstNode(tok, 0);
            }

            // Identificador: Celda (A1, B1, D4) o Función (SUM(A1:B3), AVG, MIN, MAX)
            if (std::isalpha(static_cast<unsigned char>(c))) {
                size_t start = m_pos;
                while (m_pos < m_src.size() && std::isalnum(static_cast<unsigned char>(m_src[m_pos]))) {
                    ++m_pos;
                }
                std::string ident = toUpper(m_src.substr(start, m_pos - start));

                if (peek() == '(') {
                    get(); // consume '('
                    skipSpaces();
                    size_t r1_start = m_pos;
                    while (m_pos < m_src.size() && std::isalnum(static_cast<unsigned char>(m_src[m_pos]))) ++m_pos;
                    std::string c1_str = toUpper(m_src.substr(r1_start, m_pos - r1_start));
                    if (peek() != ':') throw std::runtime_error("Se esperaba ':' en rango");
                    get(); // consume ':'
                    skipSpaces();
                    size_t r2_start = m_pos;
                    while (m_pos < m_src.size() && std::isalnum(static_cast<unsigned char>(m_src[m_pos]))) ++m_pos;
                    std::string c2_str = toUpper(m_src.substr(r2_start, m_pos - r2_start));
                    if (peek() != ')') throw std::runtime_error("Falta ')' en función");
                    get(); // consume ')'

                    CellCoord a = CellCoord::parse(c1_str);
                    CellCoord b = CellCoord::parse(c2_str);
                    if (!a.valid || !b.valid) throw std::runtime_error("Rango inválido");
                    if (ident != "SUM" && ident != "AVG" && ident != "MIN" && ident != "MAX")
                        throw std::runtime_error("Función desconocida");

                    TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                    TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                    for (TX r = r_min; r <= r_max; ++r) {
                        for (TX col = c_min; col <= c_max; ++col) {
                            addUniqueStr(m_refs, CellCoord(col, r, true).toId());
                        }
                    }

                    AstToken tok;
                    tok.kind = AstNodeKind::FuncRange;
                    tok.func_name = ident;
                    tok.range_start = a.toId();
                    tok.range_end = b.toId();
                    tok.text = ident + "(" + a.toId() + ":" + b.toId() + ")";
                    return new AstNode(tok, 0);
                }

                CellCoord coord = CellCoord::parse(ident);
                if (!coord.valid) throw std::runtime_error("Celda inválida: " + ident);
                std::string cid = coord.toId();
                addUniqueStr(m_refs, cid);

                AstToken tok;
                tok.kind = AstNodeKind::CellRef;
                tok.text = cid;
                return new AstNode(tok, 0);
            }

            throw std::runtime_error("Carácter inesperado");
        }
    };

    /**
     * @brief Evalúa un árbol `BinaryTreeNode<AstToken>` recorriéndolo con `BinaryTreeIterator` en `Order::PostOrder`.
     */
    double evaluateAstPostOrder(AstNode* root,
                                StringVec& visiting,
                                std::string& out_rpn,
                                std::string& out_err) {
        if (!root) return 0.0;
        DoubleVec stack;
        std::ostringstream rpn_ss;
        bool first = true;

        for (PostIter it(root), end(nullptr); it != end; ++it) {
            const AstToken& tok = (*it).getValue();
            if (!first) rpn_ss << " ";
            rpn_ss << "[" << tok.text << "]";
            first = false;

            if (tok.kind == AstNodeKind::Number) {
                stack.push_back(tok.number);
            } else if (tok.kind == AstNodeKind::CellRef) {
                double ref_val = resolveCellValue(tok.text, visiting, out_err);
                if (!out_err.empty()) {
                    out_rpn = rpn_ss.str();
                    return 0.0;
                }
                stack.push_back(ref_val);
            } else if (tok.kind == AstNodeKind::FuncRange) {
                CellCoord a = CellCoord::parse(tok.range_start);
                CellCoord b = CellCoord::parse(tok.range_end);
                TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                double acc = 0.0;
                double mn = 0.0, mx = 0.0;
                TX count = 0;
                for (TX r = r_min; r <= r_max; ++r) {
                    for (TX col = c_min; col <= c_max; ++col) {
                        std::string cid = CellCoord(col, r, true).toId();
                        double v = resolveCellValue(cid, visiting, out_err);
                        if (!out_err.empty()) {
                            out_rpn = rpn_ss.str();
                            return 0.0;
                        }
                        if (count == 0) { mn = mx = v; }
                        else { mn = std::min(mn, v); mx = std::max(mx, v); }
                        acc += v;
                        ++count;
                    }
                }
                if (tok.func_name == "SUM") stack.push_back(acc);
                else if (tok.func_name == "AVG") stack.push_back(count > 0 ? acc / count : 0.0);
                else if (tok.func_name == "MIN") stack.push_back(mn);
                else if (tok.func_name == "MAX") stack.push_back(mx);
            } else if (tok.kind == AstNodeKind::BinaryOp) {
                if (stack.size() < 2) {
                    out_err = "#SYNTAX!";
                    out_rpn = rpn_ss.str();
                    return 0.0;
                }
                double rhs = stack.back().getValue();
                stack.pop_back();
                double lhs = stack.back().getValue();
                stack.pop_back();
                double res = 0.0;
                switch (tok.op) {
                    case '+': res = lhs + rhs; break;
                    case '-': res = lhs - rhs; break;
                    case '*': res = lhs * rhs; break;
                    case '/':
                        if (std::abs(rhs) < 1e-12) {
                            out_err = "#DIV/0!";
                            out_rpn = rpn_ss.str();
                            return 0.0;
                        }
                        res = lhs / rhs;
                        break;
                    case '^': res = std::pow(lhs, rhs); break;
                    default:
                        out_err = "#OP!";
                        return 0.0;
                }
                stack.push_back(res);
            }
        }

        out_rpn = rpn_ss.str();
        if (stack.size() != 1) {
            out_err = "#SYNTAX!";
            return 0.0;
        }
        return stack.back().getValue();
    }

    static std::string summarizeSubtree(AstNode* node) {
        if (!node) return "∅";
        const AstToken& t = node->getValue();
        if (!node->m_pChild[0] && !node->m_pChild[1]) return t.text;
        return "(" + summarizeSubtree(node->m_pChild[0]) + " " + t.text + " " + summarizeSubtree(node->m_pChild[1]) + ")";
    }

    double resolveCellValue(const std::string& cell_id, StringVec& visiting, std::string& out_err) {
        if (containsStr(visiting, cell_id)) {
            out_err = "#CYCLE!";
            return 0.0;
        }
        if (!m_raw_table.contains(cell_id) || m_raw_table[cell_id].empty()) {
            return 0.0;
        }

        visiting.push_back(cell_id);
        std::string raw = m_raw_table[cell_id];
        StringVec refs;
        AstNode* ast = nullptr;
        try {
            FormulaParser parser(raw, refs);
            ast = parser.parse();
        } catch (...) {
            visiting.pop_back();
            out_err = "#SYNTAX!";
            return 0.0;
        }

        std::string rpn;
        std::string local_err;
        double val = evaluateAstPostOrder(ast, visiting, rpn, local_err);

        SpreadsheetCell& cell = m_cells[cell_id];
        cell.id = cell_id;
        cell.raw_expression = raw;
        cell.is_formula = (refs.size() > 0) || (ast && ast->m_pChild[0] != nullptr) || (!raw.empty() && raw[0] == '=');
        cell.postorder_rpn = rpn;
        cell.ast_root_label = ast ? ast->getValue().text : "";
        cell.ast_left_label = (ast && ast->m_pChild[0]) ? summarizeSubtree(ast->m_pChild[0]) : "";
        cell.ast_right_label = (ast && ast->m_pChild[1]) ? summarizeSubtree(ast->m_pChild[1]) : "";

        freeAst(ast);
        visiting.pop_back();

        if (!local_err.empty()) {
            cell.has_error = true;
            cell.error_msg = local_err;
            cell.value = 0.0;
            out_err = local_err;
            return 0.0;
        }

        cell.has_error = false;
        cell.error_msg.clear();
        cell.value = val;
        return val;
    }

    /** @brief Actualiza las listas enlazadas de dependencias (`m_inputs` y `m_dependents`) para `cid`. */
    void updateDependencies(const std::string& cid, const std::string& expr) {
        // 1. Desuscribir `cid` de la LinkedList de dependientes de sus antiguas entradas
        if (m_inputs.contains(cid)) {
            for (const auto& old_in : m_inputs[cid]) {
                if (m_dependents.contains(old_in.getValue())) {
                    listRemove(m_dependents[old_in.getValue()], cid);
                }
            }
            m_inputs[cid].clear();
        }

        if (expr.empty()) return;

        // 2. Parsear nueva expresión para obtener referencias y suscribir `cid` en cada `DepList`
        StringVec new_refs;
        try {
            FormulaParser parser(expr, new_refs);
            AstNode* ast = parser.parse();
            freeAst(ast);
        } catch (...) {
            return;
        }

        DepList& in_list = m_inputs[cid];
        for (size_t i = 0; i < new_refs.size(); ++i) {
            const std::string& ref_id = new_refs[i].getValue();
            listAddUnique(in_list, ref_id);
            if (ref_id != cid) {
                listAddUnique(m_dependents[ref_id], cid);
            }
        }
    }

    /** @brief Reconstruye todas las listas enlazadas de dependencias (útil tras `load`). */
    void rebuildDependencyGraph() {
        m_inputs.clear();
        m_dependents.clear();
        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            std::string raw = entry_node.getValue().getValue();
            if (!raw.empty()) updateDependencies(cid, raw);
        }
    }

    /**
     * @brief Propaga el recálculo en cascada recorriendo la `LinkedList` de dependientes de cada celda.
     */
    StringVec propagateFrom(const std::string& root_id) {
        StringVec updated;
        DepList queue;
        StringVec visited;
        visited.push_back(root_id);

        if (m_dependents.contains(root_id)) {
            for (const auto& dep_node : m_dependents[root_id]) {
                const std::string& dep_id = dep_node.getValue();
                if (!containsStr(visited, dep_id)) {
                    queue.push_back(dep_id, 0);
                    visited.push_back(dep_id);
                }
            }
        }

        while (!queue.empty()) {
            // Extraer el frente de la LinkedList y avanzar la cola
            std::string curr_id = (*queue.begin()).getValue();
            DepList next_queue;
            bool first = true;
            for (const auto& qnode : queue) {
                if (first) { first = false; continue; }
                next_queue.push_back(qnode.getValue(), 0);
            }
            queue = std::move(next_queue);

            if (!m_raw_table.contains(curr_id) || m_raw_table[curr_id].empty()) continue;
            std::string prev_disp = m_cells.contains(curr_id) ? m_cells[curr_id].displayValue() : "";

            StringVec eval_visiting;
            std::string err;
            resolveCellValue(curr_id, eval_visiting, err);

            std::string now_disp = m_cells[curr_id].displayValue();
            if (now_disp != prev_disp) {
                addUniqueStr(updated, curr_id + "=" + now_disp);
            }

            // Notificar a todas las celdas en la LinkedList de `curr_id`
            if (m_dependents.contains(curr_id)) {
                for (const auto& dep_node : m_dependents[curr_id]) {
                    const std::string& dep_id = dep_node.getValue();
                    if (!containsStr(visited, dep_id)) {
                        queue.push_back(dep_id, 0);
                        visited.push_back(dep_id);
                    }
                }
            }
        }
        return updated;
    }

public:
    SpreadsheetEngine() {
        addLog("Motor iniciado (HashTable + BinaryTree + LinkedList)");
    }

    /** @brief Registra un evento en el historial de la ventana de actividad. */
    void addLog(const std::string& entry) {
        m_logs.push_back(entry);
    }

    /** @brief Retorna el historial de eventos. */
    const StringVec& getLogs() const { return m_logs; }

    /** @brief Acceso a la tabla cruda serializable (`Printable` / `Readable`). */
    RawTable& rawTable() { return m_raw_table; }
    const RawTable& rawTable() const { return m_raw_table; }

    /** @brief Reconstruye el grafo `LinkedList` y recalcula todas las celdas activas. */
    StringVec recalculateAll(const std::string& changed_id = "") {
        rebuildDependencyGraph();

        BaseHashTable<HashTableTraits<std::string, std::string>, Callable> prev_display;
        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            prev_display[cid] = m_cells.contains(cid) ? m_cells[cid].displayValue() : "";
        }

        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            std::string raw = entry_node.getValue().getValue();
            if (raw.empty()) continue;

            StringVec visiting;
            std::string err;
            resolveCellValue(cid, visiting, err);
        }

        StringVec updated;
        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            std::string raw = entry_node.getValue().getValue();
            if (raw.empty() || cid == changed_id) continue;

            std::string now_disp = m_cells[cid].displayValue();
            if (now_disp != prev_display[cid]) {
                updated.push_back(cid + "=" + now_disp);
            }
        }
        return updated;
    }

    /**
     * @brief Asigna un valor o fórmula a una celda (ej. `setCell("A1", "5")`, `setCell("B1", "A1 + 4")`).
     */
    bool setCell(const std::string& cell_id_raw, const std::string& expression_raw) {
        CellCoord coord = CellCoord::parse(cell_id_raw);
        if (!coord.valid) {
            addLog("✖ Coordenada inválida: " + cell_id_raw);
            return false;
        }
        std::string cid = coord.toId();
        std::string expr = trim(expression_raw);
        if (!expr.empty() && expr[0] == '=') expr = trim(expr.substr(1));

        updateDependencies(cid, expr);

        if (expr.empty()) {
            m_raw_table[cid] = "";
            m_cells[cid] = SpreadsheetCell{cid, "", 0.0, false, false, "", "", "", "", ""};
            StringVec cascaded = propagateFrom(cid);
            addLog("⌫ " + cid + " limpiada");
            return true;
        }

        m_raw_table[cid] = expr;
        StringVec visiting;
        std::string err;
        resolveCellValue(cid, visiting, err);
        StringVec cascaded = propagateFrom(cid);

        const SpreadsheetCell& cell = m_cells[cid];
        std::ostringstream log_ss;
        if (cell.has_error) {
            log_ss << "⚠ " << cid << " = " << expr << " ➔ " << cell.error_msg;
        } else {
            log_ss << "✔ " << cid << " = " << expr << " ➔ " << cell.displayValue();
        }
        if (cascaded.size() > 0) {
            log_ss << " (↻ ";
            for (size_t i = 0; i < cascaded.size(); ++i) {
                if (i > 0) log_ss << ", ";
                log_ss << cascaded[i].getValue();
            }
            log_ss << ")";
        }
        addLog(log_ss.str());
        return !cell.has_error;
    }

    /**
     * @brief Ejecuta un comando completo estilo `A1 = 5` o `B1 = A1 + 4`.
     * @param cmd Cadena de comando; si no tiene `=`, se asigna a `fallback_cell`.
     */
    bool executeCommand(const std::string& cmd, const std::string& fallback_cell, std::string* out_assigned_cell = nullptr) {
        std::string s = trim(cmd);
        if (s.empty()) return false;

        size_t eq_pos = s.find('=');
        if (eq_pos != std::string::npos && eq_pos > 0) {
            std::string lhs = trim(s.substr(0, eq_pos));
            std::string rhs = trim(s.substr(eq_pos + 1));
            CellCoord c = CellCoord::parse(lhs);
            if (c.valid) {
                if (out_assigned_cell) *out_assigned_cell = c.toId();
                return setCell(c.toId(), rhs);
            }
        }

        if (out_assigned_cell) *out_assigned_cell = fallback_cell;
        return setCell(fallback_cell, s);
    }

    /** @brief Consulta el estado de una celda (retorna celda vacía por defecto si no existe). */
    SpreadsheetCell getCell(const std::string& cell_id) {
        CellCoord c = CellCoord::parse(cell_id);
        std::string cid = c.valid ? c.toId() : cell_id;
        if (m_cells.contains(cid)) return m_cells[cid];
        SpreadsheetCell empty;
        empty.id = cid;
        return empty;
    }

    /** @brief Obtiene la lista de celdas de las que depende directamente `cell_id` (sus entradas). */
    StringVec getCellInputs(const std::string& cell_id) {
        StringVec refs;
        if (!m_raw_table.contains(cell_id) || m_raw_table[cell_id].empty()) return refs;
        try {
            FormulaParser parser(m_raw_table[cell_id], refs);
            AstNode* ast = parser.parse();
            freeAst(ast);
        } catch (...) {}
        return refs;
    }

    /** @brief Obtiene la lista de celdas que dependen de `cell_id` desde su `LinkedList` de observadores. */
    StringVec getCellDependents(const std::string& cell_id) {
        StringVec deps;
        if (!m_dependents.contains(cell_id)) return deps;
        for (const auto& dep_node : m_dependents[cell_id]) {
            addUniqueStr(deps, dep_node.getValue());
        }
        return deps;
    }

    /**
     * @brief Evalúa en vivo una entrada parcial de consola sin mutar la hoja de cálculo.
     *
     * Permite que las 4 ventanas (Matriz, Inspector AST y Preview) se actualicen mientras el usuario tipea.
     */
    EvalPreview previewCommand(const std::string& raw_input, const std::string& active_cell_id) {
        EvalPreview p;
        std::string s = trim(raw_input);
        if (s.empty()) return p;

        // Ignorar comandos administrativos
        std::string upper = toUpper(s);
        if (upper == "DEMO" || upper == "CLEAR" || upper == "QUIT" || upper == "EXIT" ||
            upper == "HELP" || upper.starts_with("SAVE") || upper.starts_with("LOAD")) {
            return p;
        }

        p.active = true;
        p.target_cell = active_cell_id;
        p.expression = s;

        size_t eq_pos = s.find('=');
        if (eq_pos != std::string::npos) {
            std::string lhs = trim(s.substr(0, eq_pos));
            std::string rhs = trim(s.substr(eq_pos + 1));
            if (!lhs.empty()) {
                CellCoord c = CellCoord::parse(lhs);
                if (c.valid) {
                    p.target_cell = c.toId();
                    p.expression = rhs;
                } else {
                    p.valid = false;
                    p.error_msg = "Celda destino inválida: " + lhs;
                    return p;
                }
            } else {
                p.expression = rhs;
            }
        }

        if (p.expression.empty()) {
            p.valid = false;
            p.error_msg = "Esperando expresión...";
            return p;
        }

        AstNode* ast = nullptr;
        try {
            FormulaParser parser(p.expression, p.referenced_cells);
            ast = parser.parse();
        } catch (const std::exception& e) {
            p.valid = false;
            p.error_msg = e.what();
            return p;
        }

        StringVec visiting;
        visiting.push_back(p.target_cell);
        std::string rpn, err;
        double val = evaluateAstPostOrder(ast, visiting, rpn, err);

        p.postorder_rpn = rpn;
        p.ast_root  = ast ? ast->getValue().text : "";
        p.ast_left  = (ast && ast->m_pChild[0]) ? summarizeSubtree(ast->m_pChild[0]) : "";
        p.ast_right = (ast && ast->m_pChild[1]) ? summarizeSubtree(ast->m_pChild[1]) : "";
        freeAst(ast);

        if (!err.empty()) {
            p.valid = false;
            p.error_msg = err;
            return p;
        }

        p.valid = true;
        p.value = val;
        p.formatted_value = SpreadsheetCell::formatNumber(val);
        return p;
    }

    /** @brief Limpia toda la hoja de cálculo y el grafo `LinkedList` de dependencias. */
    void clear() {
        m_raw_table.clear();
        m_cells.clear();
        m_inputs.clear();
        m_dependents.clear();
        addLog("Hoja de cálculo reiniciada");
    }

    /** @brief Carga el caso demostrativo explicado por el profesor Ernesto Cuadros-Vargas. */
    void loadProfessorDemo() {
        setCell("A1", "5");
        setCell("B1", "A1 + 4");
        setCell("D4", "A1 + B1");
    }
};

#endif // __SPREADSHEET_H__
