/**
 * @file engine.h
 * @ingroup texcel
 * @brief Motor de hoja de cálculo reactiva (`HashTable` + `BinaryTree` PostOrder + `LinkedList`).
 */

#ifndef __TEXCEL_ENGINE_H__
#define __TEXCEL_ENGINE_H__

#include "types.h"
#include "parser.h"
#include "../containers/hash_table.h"
#include "../containers/lists/linked_list.h"
#include "../utilities/layouts/vertical_tree_layout.h"
#include "../utilities/layouts/horizontal_tree_layout.h"
#include "../utilities/ansi_style.h"

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

    static std::string trim(const std::string& s) { return texcel_detail::trim(s); }
    static std::string toUpper(std::string s)    { return texcel_detail::toUpper(std::move(s)); }
    static bool containsStr(const StringVec& vec, const std::string& target) { return texcel_detail::containsStr(vec, target); }
    static void addUniqueStr(StringVec& vec, const std::string& val)         { texcel_detail::addUniqueStr(vec, val); }
    static void freeAst(AstNode* node)                                       { texcel_detail::freeAst(node); }

    struct ParsedAssignment {
        std::string target_cell;
        std::string expression;
        bool        has_invalid_lhs = false;
        std::string invalid_lhs;
    };

    static ParsedAssignment parseAssignment(const std::string& s, const std::string& fallback_cell) {
        ParsedAssignment res{fallback_cell, s};
        size_t eq_pos = s.find('=');
        if (eq_pos == std::string::npos) return res;
        std::string lhs = trim(s.substr(0, eq_pos));
        std::string rhs = trim(s.substr(eq_pos + 1));
        if (lhs.empty()) {
            res.expression = rhs;
            return res;
        }
        CellCoord c = CellCoord::parse(lhs);
        if (c.valid) {
            res.target_cell = c.toId();
            res.expression = rhs;
        } else {
            res.has_invalid_lhs = true;
            res.invalid_lhs = lhs;
        }
        return res;
    }

    static bool isPureNumber(const std::string& s) {
        if (s.empty()) return false;
        size_t i = 0;
        if (s[i] == '+' || s[i] == '-') ++i;
        if (i >= s.size()) return false;
        bool has_digit = false, has_dot = false;
        for (; i < s.size(); ++i) {
            if (texcel_detail::isChar<std::isdigit>(s[i])) {
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
                    ExpectedType child_exp = texcel_detail::binaryOpExpectedType(tok.op);
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
            } else if (tok.kind == AstNodeKind::CellRange || tok.kind == AstNodeKind::ArgPair) {
                // Nodos estructurales consumidos por su padre FuncCall
            } else if (tok.kind == AstNodeKind::FuncCall) {
                Vector<VectorAscTraits<AstNode*>> args;
                if (node.m_pChild[0]) args.push_back(node.m_pChild[0]);
                AstNode* cur = node.m_pChild[1];
                while (cur) {
                    if (cur->getValue().kind == AstNodeKind::ArgPair) {
                        if (cur->m_pChild[0]) args.push_back(cur->m_pChild[0]);
                        cur = cur->m_pChild[1];
                    } else {
                        args.push_back(cur);
                        break;
                    }
                }

                const std::string& fn = tok.func_name;
                if (fn == "IF") {
                    if (args.size() != 3) { out_err = "#SYNTAX!"; out_rpn = rpn_ss.str(); return {}; }
                    const CellEvalValue& cond = args[0].getValue()->getValue().evaluated;
                    bool truthy = (!cond.is_text && std::abs(cond.num) > 1e-9) ||
                                  (cond.is_text && toUpper(cond.text) == "TRUE");
                    tok.evaluated = truthy ? args[1].getValue()->getValue().evaluated
                                           : args[2].getValue()->getValue().evaluated;
                } else if (fn == "CONCAT") {
                    std::string out_s;
                    for (size_t i = 0; i < args.size(); ++i) {
                        const AstToken& at = args[i].getValue()->getValue();
                        if (at.kind == AstNodeKind::CellRange) {
                            bool ok = texcel_detail::forEachCellInRange(
                                CellCoord::parse(at.range_start), CellCoord::parse(at.range_end),
                                [&](const std::string& cid) {
                                    CellEvalValue cv = resolveCellValue(cid, visiting, out_err);
                                    if (!out_err.empty()) return false;
                                    out_s += cv.asString();
                                    return true;
                                });
                            if (!ok) { out_rpn = rpn_ss.str(); return {}; }
                        } else {
                            out_s += at.evaluated.asString();
                        }
                    }
                    tok.evaluated = CellEvalValue{true, 0.0, out_s};
                } else if (fn == "SUM" || fn == "AVG" || fn == "MIN" || fn == "MAX") {
                    double acc = 0.0, mn = 0.0, mx = 0.0;
                    TX count = 0;
                    auto foldNum = [&](double v) {
                        if (count == 0) { mn = mx = v; }
                        else { mn = std::min(mn, v); mx = std::max(mx, v); }
                        acc += v;
                        ++count;
                    };
                    for (size_t i = 0; i < args.size(); ++i) {
                        const AstToken& at = args[i].getValue()->getValue();
                        if (at.kind == AstNodeKind::CellRange) {
                            bool ok = texcel_detail::forEachCellInRange(
                                CellCoord::parse(at.range_start), CellCoord::parse(at.range_end),
                                [&](const std::string& cid) {
                                    CellEvalValue cv = resolveCellValue(cid, visiting, out_err);
                                    if (!out_err.empty()) return false;
                                    if (!cv.is_text) foldNum(cv.num);
                                    return true;
                                });
                            if (!ok) { out_rpn = rpn_ss.str(); return {}; }
                        } else {
                            const CellEvalValue& ev = at.evaluated;
                            if (!ev.is_text) foldNum(ev.num);
                            else if (ev.text.empty()) foldNum(0.0);
                            else if (isPureNumber(ev.text)) foldNum(std::stod(ev.text));
                            else { out_err = "#VALUE!"; out_rpn = rpn_ss.str(); return {}; }
                        }
                    }
                    if (fn == "SUM") tok.evaluated = CellEvalValue{false, acc, ""};
                    else if (fn == "AVG") tok.evaluated = CellEvalValue{false, count > 0 ? acc / count : 0.0, ""};
                    else if (fn == "MIN") tok.evaluated = CellEvalValue{false, mn, ""};
                    else if (fn == "MAX") tok.evaluated = CellEvalValue{false, mx, ""};
                } else if (fn == "COUNTIF" || fn == "COUNTIFS") {
                    const size_t n_conds = args.size() / 2;
                    if (n_conds == 0 || args.size() % 2 != 0) { out_err = "#VALUE!"; out_rpn = rpn_ss.str(); return {}; }

                    TX base_cols = 0, base_rows = 0;
                    Vector<VectorAscTraits<CellCoord>> starts;
                    ValueVec crit_vals;
                    for (size_t k = 0; k < n_conds; ++k) {
                        const AstToken& rt = args[2 * k].getValue()->getValue();
                        CellCoord a, b;
                        if (rt.kind == AstNodeKind::CellRange) {
                            a = CellCoord::parse(rt.range_start);
                            b = CellCoord::parse(rt.range_end);
                        } else if (rt.kind == AstNodeKind::CellRef) {
                            a = b = CellCoord::parse(rt.text);
                        } else {
                            out_err = "#VALUE!"; out_rpn = rpn_ss.str(); return {};
                        }
                        TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                        TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                        TX cols = c_max - c_min + 1;
                        TX rows = r_max - r_min + 1;
                        if (k == 0) { base_cols = cols; base_rows = rows; }
                        else if (cols != base_cols || rows != base_rows) {
                            out_err = "#VALUE!"; out_rpn = rpn_ss.str(); return {};
                        }
                        starts.push_back(CellCoord(c_min, r_min, true));
                        crit_vals.push_back(args[2 * k + 1].getValue()->getValue().evaluated);
                    }

                    TX matched = 0;
                    for (TX dr = 0; dr < base_rows; ++dr) {
                        for (TX dc = 0; dc < base_cols; ++dc) {
                            bool all_match = true;
                            for (size_t k = 0; k < n_conds; ++k) {
                                CellCoord s = starts[k].getValue();
                                std::string cid = CellCoord(s.col + dc, s.row + dr, true).toId();
                                bool is_empty = (!m_raw_table.contains(cid) || m_raw_table[cid].empty());
                                CellEvalValue cv;
                                if (!is_empty) {
                                    cv = resolveCellValue(cid, visiting, out_err);
                                    if (!out_err.empty()) { out_rpn = rpn_ss.str(); return {}; }
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
                }
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
                } else if (tok.op == '=' || tok.op == '!' || tok.op == '<' || tok.op == '>' || tok.op == 'L' || tok.op == 'G') {
                    bool cmp_res = false;
                    if (!lhs.is_text && !rhs.is_text) {
                        double lnum = lhs.num, rnum = rhs.num;
                        if (tok.op == '=') cmp_res = std::abs(lnum - rnum) < 1e-9;
                        else if (tok.op == '!') cmp_res = std::abs(lnum - rnum) >= 1e-9;
                        else if (tok.op == '<') cmp_res = lnum < rnum - 1e-9;
                        else if (tok.op == '>') cmp_res = lnum > rnum + 1e-9;
                        else if (tok.op == 'L') cmp_res = lnum <= rnum + 1e-9;
                        else if (tok.op == 'G') cmp_res = lnum >= rnum - 1e-9;
                    } else {
                        std::string ls = lhs.asString(), rs = rhs.asString();
                        if (tok.op == '=') cmp_res = (ls == rs);
                        else if (tok.op == '!') cmp_res = (ls != rs);
                        else if (tok.op == '<') cmp_res = (ls < rs);
                        else if (tok.op == '>') cmp_res = (ls > rs);
                        else if (tok.op == 'L') cmp_res = (ls <= rs);
                        else if (tok.op == 'G') cmp_res = (ls >= rs);
                    }
                    tok.evaluated = CellEvalValue{false, cmp_res ? 1.0 : 0.0, ""};
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

    CellEvalValue evaluateRawExpression(const std::string& raw,
                                        StringVec& visiting,
                                        StringVec& out_refs,
                                        std::string& out_rpn,
                                        std::string& out_err,
                                        bool& out_is_formula,
                                        std::string* out_syntax_msg = nullptr) {
        out_is_formula = false;
        bool has_quote = (!raw.empty() && raw[0] == '\'');
        bool explicit_formula = (!raw.empty() && raw[0] == '=');
        if (has_quote || (!explicit_formula && !isPureNumber(raw))) {
            std::string txt = has_quote ? raw.substr(1) : raw;
            out_rpn = "\"" + txt + "\"";
            return CellEvalValue{true, 0.0, txt};
        }

        AstNode* ast = nullptr;
        try {
            FormulaParser parser(raw, out_refs);
            ast = parser.parse();
        } catch (const std::exception& e) {
            out_err = "#SYNTAX!";
            if (out_syntax_msg) *out_syntax_msg = e.what();
            return {};
        }

        CellEvalValue eval_res = evaluateAstPostOrder(ast, visiting, out_rpn, out_err);
        out_is_formula = explicit_formula || (out_refs.size() > 0) || (ast && ast->m_pChild[0] != nullptr);
        freeAst(ast);
        return eval_res;
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
        StringVec refs;
        std::string rpn, local_err;
        bool is_formula = false;
        CellEvalValue eval_res = evaluateRawExpression(raw, visiting, refs, rpn, local_err, is_formula);
        visiting.pop_back();

        SpreadsheetCell& cell = m_cells[cell_id];
        cell.id = cell_id;
        cell.raw_expression = raw;
        cell.is_formula = is_formula;
        cell.postorder_rpn = rpn;

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

        if (expr.empty() || expr[0] != '=') return;

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
     * @brief Propaga el recálculo en cascada recorriendo la `LinkedList` de dependientes en BFS O(1) por paso.
     */
    StringVec propagateFrom(const std::string& root_id) {
        StringVec updated;
        StringVec visited;
        visited.push_back(root_id);

        for (size_t qi = 0; qi < visited.size(); ++qi) {
            std::string curr_id = visited[qi].getValue();
            if (qi > 0) {
                if (!m_raw_table.contains(curr_id) || m_raw_table[curr_id].empty()) continue;
                std::string prev_disp = m_cells.contains(curr_id) ? m_cells[curr_id].displayValue() : "";

                StringVec eval_visiting;
                std::string err;
                resolveCellValue(curr_id, eval_visiting, err);

                std::string now_disp = m_cells[curr_id].displayValue();
                if (now_disp != prev_disp) {
                    addUniqueStr(updated, curr_id + "=" + now_disp);
                }
            }
            if (m_dependents.contains(curr_id)) {
                for (const auto& dep_node : m_dependents[curr_id]) {
                    addUniqueStr(visited, dep_node.getValue());
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
            m_cells[cid] = SpreadsheetCell{cid, "", 0.0, "", false, false, false, "", ""};
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

        ParsedAssignment pa = parseAssignment(s, fallback_cell);
        std::string target = pa.has_invalid_lhs ? fallback_cell : pa.target_cell;
        std::string expr   = pa.has_invalid_lhs ? s : (!pa.expression.empty() && s.find('=') == 0 ? s : pa.expression);
        if (out_assigned_cell) *out_assigned_cell = target;
        return setCell(target, expr);
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

    static StringVec collectGraphList(DepGraph& graph, const std::string& cell_id) {
        StringVec res;
        if (!graph.contains(cell_id)) return res;
        for (const auto& node : graph[cell_id]) {
            addUniqueStr(res, node.getValue());
        }
        return res;
    }

    /** @brief Obtiene la lista de celdas de las que depende directamente `cell_id` (sus entradas). */
    StringVec getCellInputs(const std::string& cell_id) {
        return collectGraphList(m_inputs, cell_id);
    }

    /** @brief Obtiene la lista de celdas que dependen de `cell_id` desde su `LinkedList` de observadores. */
    StringVec getCellDependents(const std::string& cell_id) {
        return collectGraphList(m_dependents, cell_id);
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
        ParsedAssignment pa = parseAssignment(s, active_cell_id);
        if (pa.has_invalid_lhs) {
            p.valid = false;
            p.error_msg = "Celda destino inválida: " + pa.invalid_lhs;
            return p;
        }
        p.target_cell = pa.target_cell;
        p.expression = pa.expression;

        if (p.expression.empty()) {
            p.valid = false;
            p.error_msg = "Esperando expresión...";
            return p;
        }

        StringVec visiting;
        visiting.push_back(p.target_cell);
        std::string rpn, err, syntax_msg;
        bool is_formula = false;
        CellEvalValue eval_res = evaluateRawExpression(
            p.expression, visiting, p.referenced_cells, rpn, err, is_formula, &syntax_msg);

        p.postorder_rpn = rpn;
        if (!err.empty()) {
            p.valid = false;
            p.error_msg = (!syntax_msg.empty()) ? syntax_msg : err;
            return p;
        }

        p.valid = true;
        p.is_text = eval_res.is_text;
        p.value = eval_res.num;
        p.formatted_value = eval_res.asString();
        return p;
    }

    /**
     * @brief Proyecta el AST completo de una expresión usando `VerticalTreeLayout` o `HorizontalTreeLayout`.
     */
    VisualGraph buildAstVisualGraph(const std::string& raw_expr, bool horizontal) const {
        std::string expr = texcel_detail::trim(raw_expr);
        if (expr.empty()) return {};
        if (!expr.empty() && expr[0] == '=') expr = texcel_detail::trim(expr.substr(1));
        if (expr.empty() || expr[0] == '"') return {};

        AstNode* ast = nullptr;
        try {
            StringVec refs;
            FormulaParser parser(expr, refs);
            ast = parser.parse();
        } catch (...) {
            return {};
        }
        if (!ast) return {};

        struct AstTreeView {
            AstNode* m_root;
            AstNode* getRoot() const { return m_root; }
        };

        auto fmt = [](const AstToken& tok, TreeRole role) -> std::string {
            if (role == TreeRole::Root) return styled("[" + tok.text + "]").bright_yellow().bold().str();
            if (role == TreeRole::Leaf) return styled(tok.text).bright_cyan().str();
            return styled(tok.text).bright_green().bold().str();
        };

        VisualGraph vg;
        if (horizontal) {
            HorizontalTreeLayout<decltype(fmt)> layout(fmt, true);
            vg = layout(AstTreeView{ast});
        } else {
            VerticalTreeLayout<decltype(fmt)> layout(fmt, 3, false);
            vg = layout(AstTreeView{ast});
        }
        freeAst(ast);
        return vg;
    }

    /** @brief Limpia un rango rectangular de celdas y propaga dependencias. */
    void clearRange(TX c0, TX r0, TX c1, TX r1) {
        texcel_detail::forEachCellInRange(CellCoord(c0, r0, true), CellCoord(c1, r1, true), [&](const std::string& cid) {
            if (m_raw_table.contains(cid) && !m_raw_table[cid].empty()) {
                setCell(cid, "");
            }
        });
    }

    /**
     * @brief Desplaza referencias relativas en una fórmula por `(dcol, drow)` respetando `$` absolutos y `"..."`.
     */
    static std::string shiftFormulaRefs(const std::string& expr, TX dcol, TX drow) {
        std::string out;
        out.reserve(expr.size() + 8);
        bool in_quotes = false;

        for (size_t i = 0; i < expr.size();) {
            char ch = expr[i];
            if (ch == '"') {
                in_quotes = !in_quotes;
                out.push_back(ch);
                ++i;
                continue;
            }
            if (in_quotes) {
                out.push_back(ch);
                ++i;
                continue;
            }

            // Detectar token de celda opcionalmente con `$` (`$A$1`, `$A1`, `A$1`, `A1`)
            bool prev_alnum = (i > 0 && texcel_detail::isChar<std::isalnum>(expr[i - 1]));
            if (!prev_alnum && (ch == '$' || texcel_detail::isChar<std::isalpha>(ch))) {
                size_t p = i;
                bool abs_col = false, abs_row = false;
                if (expr[p] == '$') { abs_col = true; ++p; }
                size_t col_start = p;
                while (p < expr.size() && texcel_detail::isChar<std::isalpha>(expr[p])) ++p;
                if (p > col_start) {
                    if (p < expr.size() && expr[p] == '$') { abs_row = true; ++p; }
                    size_t row_start = p;
                    while (p < expr.size() && texcel_detail::isChar<std::isdigit>(expr[p])) ++p;
                    bool next_is_paren = (p < expr.size() && expr[p] == '(');
                    bool next_is_alpha = (p < expr.size() && texcel_detail::isChar<std::isalpha>(expr[p]));
                    if (p > row_start && !next_is_paren && !next_is_alpha) {
                        std::string col_part = expr.substr(col_start, (expr[col_start + (p - col_start)] == '$') ? 0 : 0);
                        std::string raw_tok = expr.substr(i, p - i);
                        CellCoord coord = CellCoord::parse(raw_tok);
                        if (coord.valid) {
                            TX nc = coord.col + (abs_col ? 0 : dcol);
                            TX nr = coord.row + (abs_row ? 0 : drow);
                            if (nc < 0 || nc >= CellCoord::MAX_COLS || nr < 0 || nr >= CellCoord::MAX_ROWS) {
                                out += "#REF!";
                            } else {
                                if (abs_col) out.push_back('$');
                                out += CellCoord::colToLabel(nc);
                                if (abs_row) out.push_back('$');
                                out += std::to_string(nr + 1);
                            }
                            i = p;
                            continue;
                        }
                    }
                }
            }
            out.push_back(ch);
            ++i;
        }
        return out;
    }

    /**
     * @brief Rellena el rango `(c0, r0)..(c1, r1)` propagando fórmulas relativas o series numéricas desde `(c0, r0)`.
     */
    TX autoFillRange(TX c0, TX r0, TX c1, TX r1) {
        std::string src_id = CellCoord(c0, r0, true).toId();
        if (!m_raw_table.contains(src_id) || m_raw_table[src_id].empty()) return 0;

        SpreadsheetCell src_cell = getCell(src_id);
        std::string src_raw = m_raw_table[src_id];
        TX cmin = std::min(c0, c1), cmax = std::max(c0, c1);
        TX rmin = std::min(r0, r1), rmax = std::max(r0, r1);

        // Detectar serie aritmética si el origen y su vecino inmediato son números puros
        bool is_series = false;
        double series_base = 0.0, series_step = 0.0;
        bool fill_vertical = (rmax - rmin) >= (cmax - cmin);
        if (!src_cell.is_formula && !src_cell.is_text && isPureNumber(src_raw)) {
            series_base = std::stod(src_raw);
            TX nc = fill_vertical ? c0 : (c0 + (c1 >= c0 ? 1 : -1));
            TX nr = fill_vertical ? (r0 + (r1 >= r0 ? 1 : -1)) : r0;
            if (nc >= cmin && nc <= cmax && nr >= rmin && nr <= rmax) {
                std::string next_id = CellCoord(nc, nr, true).toId();
                if (m_raw_table.contains(next_id) && isPureNumber(m_raw_table[next_id])) {
                    series_step = std::stod(m_raw_table[next_id]) - series_base;
                    is_series = true;
                }
            }
        }

        TX filled = 0;
        for (TX r = rmin; r <= rmax; ++r) {
            for (TX c = cmin; c <= cmax; ++c) {
                if (c == c0 && r == r0) continue;
                std::string dst_id = CellCoord(c, r, true).toId();
                TX dcol = c - c0;
                TX drow = r - r0;

                if (src_cell.is_formula) {
                    std::string shifted = shiftFormulaRefs(src_raw, dcol, drow);
                    setCell(dst_id, (!shifted.empty() && shifted[0] == '=') ? shifted : ("= " + shifted));
                } else if (is_series) {
                    TX step_idx = fill_vertical ? drow : dcol;
                    setCell(dst_id, SpreadsheetCell::formatNumber(series_base + step_idx * series_step));
                } else {
                    setCell(dst_id, src_raw);
                }
                ++filled;
            }
        }
        return filled;
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
