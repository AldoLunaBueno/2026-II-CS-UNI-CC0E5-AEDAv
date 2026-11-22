/**
 * @file engine.h
 * @brief Motor de hoja de cálculo reactiva (`HashTable` + `BinaryTree` PostOrder + `LinkedList`).
 */

#ifndef __TEXCEL_ENGINE_H__
#define __TEXCEL_ENGINE_H__

#include "types.h"
#include "parser.h"
#include "../containers/hash_table.h"
#include "../containers/lists/linked_list.h"

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
    using ValueVec    = Vector<VectorAscTraits<CellEvalValue>>;
    using DepList     = LinkedList<LinkedListAscTraits<std::string>>;
    using DepGraph    = BaseHashTable<HashTableTraits<std::string, DepList>, Callable>;
    using AstNode     = BinaryTreeNode<AstToken>;
    using PrePostIter = BinaryTreeIterator<AstToken, Order::PrePostOrder, Direction::Asc>;
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

    static bool isPureNumber(const std::string& s) {
        if (s.empty()) return false;
        size_t i = 0;
        if (s[i] == '+' || s[i] == '-') ++i;
        if (i >= s.size()) return false;
        bool has_digit = false, has_dot = false;
        for (; i < s.size(); ++i) {
            if (std::isdigit(static_cast<unsigned char>(s[i]))) {
                has_digit = true;
            } else if (s[i] == '.' && !has_dot) {
                has_dot = true;
            } else {
                return false;
            }
        }
        return has_digit;
    }

    /** @brief Evalúa si una celda cumple un criterio estilo Excel (`">5"`, `"<=10"`, `"<>0"`, `"Total"`, `5`). */
    static bool matchesCriteria(const CellEvalValue& cell, bool cell_empty, const CellEvalValue& crit) {
        if (!crit.is_text) {
            return !cell_empty && !cell.is_text && std::abs(cell.num - crit.num) < 1e-9;
        }

        const std::string& raw_c = crit.text;
        std::string op_str;
        std::string rhs;

        if (raw_c.starts_with(">=") || raw_c.starts_with("<=") || raw_c.starts_with("<>") || raw_c.starts_with("!=")) {
            op_str = raw_c.substr(0, 2);
            rhs = raw_c.substr(2);
        } else if (!raw_c.empty() && (raw_c[0] == '>' || raw_c[0] == '<' || raw_c[0] == '=')) {
            op_str = raw_c.substr(0, 1);
            rhs = raw_c.substr(1);
        } else {
            if (raw_c.empty()) return cell_empty;
            return !cell_empty && cell.asString() == raw_c;
        }

        rhs = trim(rhs);
        if (rhs.empty()) {
            if (op_str == "<>" || op_str == "!=") return !cell_empty;
            if (op_str == "=") return cell_empty;
            return false;
        }

        if (isPureNumber(rhs)) {
            if (cell_empty || cell.is_text) {
                return (op_str == "<>" || op_str == "!=");
            }
            double target = std::stod(rhs);
            if (op_str == ">")  return cell.num > target;
            if (op_str == "<")  return cell.num < target;
            if (op_str == ">=") return cell.num >= target - 1e-9;
            if (op_str == "<=") return cell.num <= target + 1e-9;
            if (op_str == "=")  return std::abs(cell.num - target) < 1e-9;
            if (op_str == "<>" || op_str == "!=") return std::abs(cell.num - target) >= 1e-9;
            return false;
        }

        std::string lhs_str = cell_empty ? "" : cell.asString();
        if (op_str == "=")  return !cell_empty && lhs_str == rhs;
        if (op_str == "<>" || op_str == "!=") return cell_empty || lhs_str != rhs;
        if (cell_empty) return false;
        if (op_str == ">")  return lhs_str > rhs;
        if (op_str == "<")  return lhs_str < rhs;
        if (op_str == ">=") return lhs_str >= rhs;
        if (op_str == "<=") return lhs_str <= rhs;
        return false;
    }

    /**
     * @brief Evalúa un árbol `BinaryTreeNode<AstToken>` in-place sin pila externa mediante `Order::PrePostOrder`.
     *
     * - En `Phase::Pre` (bajando): propaga `expected_type` (aritmético vs texto) desde operadores hacia sus hijos.
     * - En `Phase::Post` (subiendo): evalúa hojas, valida tipos (`#VALUE!`) y combina `m_pChild[0]` y `m_pChild[1]`
     *   guardando el resultado directamente en cada nodo del árbol.
     */
    CellEvalValue evaluateAstPostOrder(AstNode* root,
                                       StringVec& visiting,
                                       std::string& out_rpn,
                                       std::string& out_err) {
        if (!root) return {};
        std::ostringstream rpn_ss;
        bool first = true;

        for (PrePostIter it(root), end(nullptr); it != end; ++it) {
            AstNode& node = *it;
            AstToken& tok = node.value();

            // 1. Fase Pre (enter): propagar tipo esperado hacia los hijos
            if (it.phase() == Phase::Pre) {
                if (tok.kind == AstNodeKind::BinaryOp) {
                    ExpectedType child_exp = (tok.op == '&') ? ExpectedType::Text : ExpectedType::Number;
                    if (node.m_pChild[0]) node.m_pChild[0]->value().expected_type = child_exp;
                    if (node.m_pChild[1]) node.m_pChild[1]->value().expected_type = child_exp;
                }
                continue;
            }

            // 2. Fase Post (exit): emitir RPN, evaluar in-place y validar tipo
            if (!first) rpn_ss << " ";
            rpn_ss << "[" << tok.text << "]";
            first = false;

            if (tok.kind == AstNodeKind::Number) {
                tok.evaluated = CellEvalValue{false, tok.number, ""};
            } else if (tok.kind == AstNodeKind::StringLiteral) {
                tok.evaluated = CellEvalValue{true, 0.0, tok.func_name};
            } else if (tok.kind == AstNodeKind::CellRef) {
                CellEvalValue ref_val = resolveCellValue(tok.text, visiting, out_err);
                if (!out_err.empty()) {
                    out_rpn = rpn_ss.str();
                    return {};
                }
                tok.evaluated = ref_val;
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
                        CellEvalValue cv = resolveCellValue(cid, visiting, out_err);
                        if (!out_err.empty()) {
                            out_rpn = rpn_ss.str();
                            return {};
                        }
                        if (!cv.is_text) {
                            double v = cv.num;
                            if (count == 0) { mn = mx = v; }
                            else { mn = std::min(mn, v); mx = std::max(mx, v); }
                            acc += v;
                            ++count;
                        }
                    }
                }
                if (tok.func_name == "SUM") tok.evaluated = CellEvalValue{false, acc, ""};
                else if (tok.func_name == "AVG") tok.evaluated = CellEvalValue{false, count > 0 ? acc / count : 0.0, ""};
                else if (tok.func_name == "MIN") tok.evaluated = CellEvalValue{false, mn, ""};
                else if (tok.func_name == "MAX") tok.evaluated = CellEvalValue{false, mx, ""};
            } else if (tok.kind == AstNodeKind::FuncCountIfs) {
                const size_t n_conds = tok.cond_pairs.size();
                if (n_conds == 0) {
                    out_err = "#VALUE!";
                    out_rpn = rpn_ss.str();
                    return {};
                }

                TX base_cols = 0, base_rows = 0;
                ValueVec crit_vals;
                for (size_t k = 0; k < n_conds; ++k) {
                    const auto& pair = tok.cond_pairs[k].getValue();
                    CellCoord a = CellCoord::parse(pair.range_start);
                    CellCoord b = CellCoord::parse(pair.range_end);
                    TX cols = b.col - a.col + 1;
                    TX rows = b.row - a.row + 1;
                    if (k == 0) {
                        base_cols = cols;
                        base_rows = rows;
                    } else if (cols != base_cols || rows != base_rows) {
                        out_err = "#VALUE!";
                        out_rpn = rpn_ss.str();
                        return {};
                    }

                    StringVec dummy_refs;
                    AstNode* c_ast = nullptr;
                    try {
                        FormulaParser c_parser(pair.criteria_expr, dummy_refs);
                        c_ast = c_parser.parse();
                    } catch (...) {
                        out_err = "#SYNTAX!";
                        out_rpn = rpn_ss.str();
                        return {};
                    }
                    std::string dummy_rpn, c_err;
                    CellEvalValue c_val = evaluateAstPostOrder(c_ast, visiting, dummy_rpn, c_err);
                    freeAst(c_ast);
                    if (!c_err.empty()) {
                        out_err = c_err;
                        out_rpn = rpn_ss.str();
                        return {};
                    }
                    crit_vals.push_back(c_val);
                }

                TX matched = 0;
                for (TX dr = 0; dr < base_rows; ++dr) {
                    for (TX dc = 0; dc < base_cols; ++dc) {
                        bool all_match = true;
                        for (size_t k = 0; k < n_conds; ++k) {
                            const auto& pair = tok.cond_pairs[k].getValue();
                            CellCoord a = CellCoord::parse(pair.range_start);
                            std::string cid = CellCoord(a.col + dc, a.row + dr, true).toId();
                            bool is_empty = (!m_raw_table.contains(cid) || m_raw_table[cid].empty());
                            CellEvalValue cv;
                            if (!is_empty) {
                                cv = resolveCellValue(cid, visiting, out_err);
                                if (!out_err.empty()) {
                                    out_rpn = rpn_ss.str();
                                    return {};
                                }
                            }
                            if (!matchesCriteria(cv, is_empty, crit_vals[k].getValue())) {
                                all_match = false;
                                break;
                            }
                        }
                        if (all_match) ++matched;
                    }
                }
                tok.evaluated = CellEvalValue{false, static_cast<double>(matched), ""};
            } else if (tok.kind == AstNodeKind::BinaryOp) {
                if (!node.m_pChild[0] || !node.m_pChild[1]) {
                    out_err = "#SYNTAX!";
                    out_rpn = rpn_ss.str();
                    return {};
                }
                const CellEvalValue& lhs = node.m_pChild[0]->getValue().evaluated;
                const CellEvalValue& rhs = node.m_pChild[1]->getValue().evaluated;

                if (tok.op == '&') {
                    tok.evaluated = CellEvalValue{true, 0.0, lhs.asString() + rhs.asString()};
                } else {
                    double lnum = lhs.asNumber();
                    double rnum = rhs.asNumber();
                    double res = 0.0;
                    switch (tok.op) {
                        case '+': res = lnum + rnum; break;
                        case '-': res = lnum - rnum; break;
                        case '*': res = lnum * rnum; break;
                        case '/':
                            if (std::abs(rnum) < 1e-12) {
                                out_err = "#DIV/0!";
                                out_rpn = rpn_ss.str();
                                return {};
                            }
                            res = lnum / rnum;
                            break;
                        case '^': res = std::pow(lnum, rnum); break;
                        default:
                            out_err = "#OP!";
                            return {};
                    }
                    tok.evaluated = CellEvalValue{false, res, ""};
                }
            }

            // Validación de tipo propagado en Phase::Pre (ej. `"Hola" + 5` -> `#VALUE!`)
            if (tok.expected_type == ExpectedType::Number && tok.evaluated.is_text) {
                if (tok.evaluated.text.empty()) {
                    tok.evaluated = CellEvalValue{false, 0.0, ""};
                } else if (isPureNumber(tok.evaluated.text)) {
                    tok.evaluated = CellEvalValue{false, std::stod(tok.evaluated.text), ""};
                } else {
                    out_err = "#VALUE!";
                    out_rpn = rpn_ss.str();
                    return {};
                }
            }
        }

        out_rpn = rpn_ss.str();
        return root->getValue().evaluated;
    }

    static std::string summarizeSubtree(AstNode* node) {
        if (!node) return "∅";
        const AstToken& t = node->getValue();
        if (!node->m_pChild[0] && !node->m_pChild[1]) return t.text;
        return "(" + summarizeSubtree(node->m_pChild[0]) + " " + t.text + " " + summarizeSubtree(node->m_pChild[1]) + ")";
    }

    CellEvalValue resolveCellValue(const std::string& cell_id, StringVec& visiting, std::string& out_err) {
        if (containsStr(visiting, cell_id)) {
            out_err = "#CYCLE!";
            return {};
        }
        if (!m_raw_table.contains(cell_id) || m_raw_table[cell_id].empty()) {
            return {};
        }

        visiting.push_back(cell_id);
        std::string raw = m_raw_table[cell_id];

        // Caso 1: Texto literal explícito con apóstrofe inicial ('123)
        if (!raw.empty() && raw[0] == '\'') {
            std::string txt = raw.substr(1);
            SpreadsheetCell& cell = m_cells[cell_id];
            cell.id = cell_id;
            cell.raw_expression = raw;
            cell.is_text = true;
            cell.text_value = txt;
            cell.value = 0.0;
            cell.is_formula = false;
            cell.has_error = false;
            cell.error_msg.clear();
            cell.postorder_rpn = "\"" + txt + "\"";
            cell.ast_root_label = "\"" + txt + "\"";
            cell.ast_left_label.clear();
            cell.ast_right_label.clear();
            visiting.pop_back();
            return CellEvalValue{true, 0.0, txt};
        }

        bool explicit_formula = (!raw.empty() && raw[0] == '=');
        StringVec refs;
        AstNode* ast = nullptr;
        bool parse_ok = false;
        try {
            FormulaParser parser(raw, refs);
            ast = parser.parse();
            parse_ok = true;
        } catch (...) {
            parse_ok = false;
        }

        // Caso 2: No empieza con '=' y no es fórmula válida -> Texto literal (ej. "Producto", "Hola")
        if (!parse_ok && !explicit_formula && !isPureNumber(raw)) {
            SpreadsheetCell& cell = m_cells[cell_id];
            cell.id = cell_id;
            cell.raw_expression = raw;
            cell.is_text = true;
            cell.text_value = raw;
            cell.value = 0.0;
            cell.is_formula = false;
            cell.has_error = false;
            cell.error_msg.clear();
            cell.postorder_rpn = "\"" + raw + "\"";
            cell.ast_root_label = "\"" + raw + "\"";
            cell.ast_left_label.clear();
            cell.ast_right_label.clear();
            visiting.pop_back();
            return CellEvalValue{true, 0.0, raw};
        }

        if (!parse_ok) {
            visiting.pop_back();
            out_err = "#SYNTAX!";
            return {};
        }

        std::string rpn;
        std::string local_err;
        CellEvalValue eval_res = evaluateAstPostOrder(ast, visiting, rpn, local_err);

        SpreadsheetCell& cell = m_cells[cell_id];
        cell.id = cell_id;
        cell.raw_expression = raw;
        cell.is_formula = explicit_formula || (refs.size() > 0) || (ast && ast->m_pChild[0] != nullptr);
        cell.postorder_rpn = rpn;
        cell.ast_root_label = ast ? ast->getValue().text : "";
        cell.ast_left_label = (ast && ast->m_pChild[0]) ? summarizeSubtree(ast->m_pChild[0]) : "";
        cell.ast_right_label = (ast && ast->m_pChild[1]) ? summarizeSubtree(ast->m_pChild[1]) : "";

        freeAst(ast);
        visiting.pop_back();

        if (!local_err.empty()) {
            cell.has_error = true;
            cell.error_msg = local_err;
            cell.is_text = false;
            cell.text_value.clear();
            cell.value = 0.0;
            out_err = local_err;
            return {};
        }

        cell.has_error = false;
        cell.error_msg.clear();
        cell.is_text = eval_res.is_text;
        cell.text_value = eval_res.text;
        cell.value = eval_res.num;
        return eval_res;
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

        updateDependencies(cid, expr);

        if (expr.empty()) {
            m_raw_table[cid] = "";
            m_cells[cid] = SpreadsheetCell{cid, "", 0.0, "", false, false, false, "", "", "", "", ""};
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

        bool explicit_formula = (!p.expression.empty() && p.expression[0] == '=');
        if (!p.expression.empty() && p.expression[0] == '\'') {
            p.valid = true;
            p.is_text = true;
            p.formatted_value = p.expression.substr(1);
            p.postorder_rpn = "\"" + p.formatted_value + "\"";
            p.ast_root = p.postorder_rpn;
            return p;
        }

        AstNode* ast = nullptr;
        try {
            FormulaParser parser(p.expression, p.referenced_cells);
            ast = parser.parse();
        } catch (const std::exception& e) {
            if (!explicit_formula && !isPureNumber(p.expression)) {
                p.valid = true;
                p.is_text = true;
                p.formatted_value = p.expression;
                p.postorder_rpn = "\"" + p.expression + "\"";
                p.ast_root = p.postorder_rpn;
                return p;
            }
            p.valid = false;
            p.error_msg = e.what();
            return p;
        }

        StringVec visiting;
        visiting.push_back(p.target_cell);
        std::string rpn, err;
        CellEvalValue eval_res = evaluateAstPostOrder(ast, visiting, rpn, err);

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
        p.is_text = eval_res.is_text;
        p.value = eval_res.num;
        p.formatted_value = eval_res.asString();
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

#endif // __TEXCEL_ENGINE_H__
