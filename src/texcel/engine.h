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
    using DepList     = LinkedList<LinkedListAscTraits<std::string>>;
    using DepGraph    = BaseHashTable<HashTableTraits<std::string, DepList>, Callable>;
    using AstNode     = BinaryTreeNode<AstToken>;
    using PrePostIter = BinaryTreeIterator<AstToken, Order::PrePostOrder, Direction::Asc>;
    using RawTable    = HashTable<HashTableTraits<std::string, std::string>>;
    using CellTable   = BaseHashTable<HashTableTraits<std::string, SpreadsheetCell>, Callable>;

private:
    RawTable  m_raw_table;  /**< Tabla serializable `(celda -> fórmula)` con mixins `Printable` y `Readable`. */
    CellTable m_cells;      /**< Tabla de estado evaluado de cada celda activa. */
    DepGraph  m_dependents; /**< `celda -> LinkedList` de celdas observadoras que dependen de ella. */
    StringVec m_logs;       /**< Historial cronológico de operaciones y propagaciones. */

    static void listRemove(DepList& lst, const std::string& target) {
        DepList filtered;
        for (const auto& node : lst) {
            if (node.getValue() != target) filtered.push_back(node.getValue(), 0);
        }
        lst = std::move(filtered);
    }

    struct EvalError {
        std::string code;
    };

    bool hasRawCell(const std::string& cid) {
        return m_raw_table.contains(cid) && !m_raw_table[cid].empty();
    }

public:
    static AstNode* tryParseAst(const std::string& expr, StringVec& out_refs, std::string* out_err_msg = nullptr) {
        try {
            FormulaParser parser(expr, out_refs);
            return parser.parse();
        } catch (const std::exception& e) {
            if (out_err_msg) *out_err_msg = e.what();
            return nullptr;
        }
    }

private:

    static bool canParseFormula(const std::string& expr) {
        StringVec refs;
        AstNode* ast = tryParseAst(expr, refs);
        bool ok = (ast != nullptr);
        texcel_detail::freeAst(ast);
        return ok;
    }

    struct ParsedAssignment {
        std::string target_cell;
        std::string expression;
        bool        has_invalid_lhs = false;
        std::string invalid_lhs;
    };

    static ParsedAssignment parseAssignment(const std::string& s, const std::string& fallback_cell) {
        ParsedAssignment res{fallback_cell, s};
        if (s.empty() || s[0] == '\'' || s[0] == '"') return res;
        size_t eq_pos = s.find('=');
        if (eq_pos == std::string::npos) return res;

        std::string lhs = texcel_detail::trim(s.substr(0, eq_pos));
        std::string rhs = texcel_detail::trim(s.substr(eq_pos + 1));
        if (lhs.empty()) {
            res.expression = rhs.empty() ? "" : s;
            return res;
        }
        CellCoord c = CellCoord::parse(lhs);
        if (!c.valid) {
            res.has_invalid_lhs = true;
            res.invalid_lhs = lhs;
            return res;
        }
        res.target_cell = c.toId();
        if (rhs.empty() || rhs[0] == '=' || rhs[0] == '\'' || texcel_detail::isPureNumber(rhs)) {
            res.expression = rhs;
        } else {
            res.expression = canParseFormula(rhs) ? ("= " + rhs) : rhs;
        }
        return res;
    }

    /** @brief Evalúa si una celda cumple un criterio estilo Excel (`">5"`, `"<=10"`, `"<>0"`, `"Total"`, `5`). */
    static bool matchesCriteria(const CellEvalValue& cell, bool cell_empty, const CellEvalValue& crit) {
        if (!crit.is_text) {
            return !cell_empty && !cell.is_text && texcel_detail::compareValues('=', cell, crit);
        }

        const std::string& raw_c = crit.text;
        char op_code = 0;
        std::string rhs;
        for (const auto& info : texcel_detail::BINARY_OPS) {
            if (info.prec != 1) continue;
            std::string_view lex(info.lexeme);
            if (raw_c.starts_with(lex)) {
                op_code = info.code;
                rhs = texcel_detail::trim(raw_c.substr(lex.size()));
                break;
            }
        }
        if (op_code == 0) {
            return raw_c.empty() ? cell_empty : (!cell_empty && cell.asString() == raw_c);
        }

        if (rhs.empty()) {
            if (op_code == '!') return !cell_empty;
            if (op_code == '=') return cell_empty;
            return false;
        }

        if (texcel_detail::isPureNumber(rhs)) {
            if (cell_empty || cell.is_text) return op_code == '!';
            return texcel_detail::compareValues(op_code, cell, CellEvalValue{false, std::stod(rhs), ""});
        }

        if (op_code == '=') return !cell_empty && cell.asString() == rhs;
        if (op_code == '!') return cell_empty || cell.asString() != rhs;
        if (cell_empty) return false;
        return texcel_detail::compareValues(op_code, CellEvalValue{true, 0.0, cell.asString()}, CellEvalValue{true, 0.0, rhs});
    }

    CellEvalValue resolveOrThrow(const std::string& cid, StringVec& visiting) {
        std::string err;
        CellEvalValue val = resolveCellValue(cid, visiting, err);
        if (!err.empty()) throw EvalError{err};
        return val;
    }

    CellEvalValue readCellForEval(const std::string& cid, StringVec& visiting, bool& out_is_empty) {
        if (texcel_detail::containsStr(visiting, cid)) throw EvalError{"#CYCLE!"};
        if (m_cells.contains(cid)) {
            const SpreadsheetCell& cached = m_cells[cid];
            if (!cached.dirty) {
                out_is_empty = cached.raw_expression.empty();
                if (out_is_empty) return {};
                if (cached.has_error) throw EvalError{cached.error_msg};
                return cached.result;
            }
        }
        out_is_empty = !hasRawCell(cid);
        return out_is_empty ? CellEvalValue{} : resolveOrThrow(cid, visiting);
    }

    static Vector<VectorAscTraits<AstNode*>> collectFuncArgs(const AstNode& node) {
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
        return args;
    }

    template <typename Fn>
    void forEachArgValue(const Vector<VectorAscTraits<AstNode*>>& args, StringVec& visiting, Fn&& fn) {
        for (size_t i = 0; i < args.size(); ++i) {
            const AstToken& at = args[i].getValue()->getValue();
            if (at.kind == AstNodeKind::CellRange) {
                texcel_detail::forEachCellInRange(at.range_a, at.range_b, [&](const std::string& cid) {
                    fn(resolveOrThrow(cid, visiting), true);
                });
            } else {
                fn(at.evaluated, false);
            }
        }
    }

    CellEvalValue evalCountIf(const Vector<VectorAscTraits<AstNode*>>& args, StringVec& visiting) {
        const size_t n_conds = args.size() / 2;
        if (n_conds == 0 || args.size() % 2 != 0) throw EvalError{"#VALUE!"};

        TX base_cols = 0, base_rows = 0;
        Vector<VectorAscTraits<CellCoord>> starts;
        Vector<VectorAscTraits<CellEvalValue>> crit_vals;
        for (size_t k = 0; k < n_conds; ++k) {
            const AstToken& rt = args[2 * k].getValue()->getValue();
            CellCoord a, b;
            if (rt.kind == AstNodeKind::CellRange) {
                a = rt.range_a;
                b = rt.range_b;
            } else if (rt.kind == AstNodeKind::CellRef) {
                a = b = CellCoord::parse(rt.text);
            } else {
                throw EvalError{"#VALUE!"};
            }
            TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
            TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
            TX cols = c_max - c_min + 1;
            TX rows = r_max - r_min + 1;
            if (k == 0) {
                base_cols = cols;
                base_rows = rows;
            } else if (cols != base_cols || rows != base_rows) {
                throw EvalError{"#VALUE!"};
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
                    bool is_empty = false;
                    CellEvalValue cv = readCellForEval(cid, visiting, is_empty);
                    if (!matchesCriteria(cv, is_empty, crit_vals[k].getValue())) {
                        all_match = false;
                        break;
                    }
                }
                if (all_match) ++matched;
            }
        }
        return CellEvalValue{false, static_cast<double>(matched), ""};
    }

    CellEvalValue evalFunction(FuncId fn,
                               const Vector<VectorAscTraits<AstNode*>>& args,
                               StringVec& visiting) {
        switch (fn) {
            case FuncId::If: {
                if (args.size() != 3) throw EvalError{"#SYNTAX!"};
                const CellEvalValue& cond = args[0].getValue()->getValue().evaluated;
                bool truthy = (!cond.is_text && std::abs(cond.num) > 1e-9) ||
                              (cond.is_text && texcel_detail::toUpper(cond.text) == "TRUE");
                return truthy ? args[1].getValue()->getValue().evaluated
                              : args[2].getValue()->getValue().evaluated;
            }
            case FuncId::Concat: {
                std::string out_s;
                forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool) {
                    out_s += ev.asString();
                });
                return CellEvalValue{true, 0.0, out_s};
            }
            case FuncId::CountIf:
            case FuncId::CountIfs:
                return evalCountIf(args, visiting);
            case FuncId::Sum:
            case FuncId::Avg:
            case FuncId::Min:
            case FuncId::Max: {
                double acc = 0.0, mn = 0.0, mx = 0.0;
                TX count = 0;
                auto foldNum = [&](double v) {
                    if (count == 0) { mn = mx = v; }
                    else { mn = std::min(mn, v); mx = std::max(mx, v); }
                    acc += v;
                    ++count;
                };
                forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool from_range) {
                    if (!ev.is_text) foldNum(ev.num);
                    else if (from_range) return;
                    else if (ev.text.empty()) foldNum(0.0);
                    else if (texcel_detail::isPureNumber(ev.text)) foldNum(std::stod(ev.text));
                    else throw EvalError{"#VALUE!"};
                });
                if (fn == FuncId::Sum) return CellEvalValue{false, acc, ""};
                if (fn == FuncId::Avg) return CellEvalValue{false, count > 0 ? acc / count : 0.0, ""};
                if (fn == FuncId::Min) return CellEvalValue{false, mn, ""};
                return CellEvalValue{false, mx, ""};
            }
            default:
                throw EvalError{"#SYNTAX!"};
        }
    }

    static CellEvalValue evalBinaryOp(char op, const CellEvalValue& lhs, const CellEvalValue& rhs) {
        double lnum = lhs.asNumber(), rnum = rhs.asNumber();
        switch (op) {
            case '&': return CellEvalValue{true, 0.0, lhs.asString() + rhs.asString()};
            case '=': case '!': case '<': case '>': case 'L': case 'G':
                return CellEvalValue{false, texcel_detail::compareValues(op, lhs, rhs) ? 1.0 : 0.0, ""};
            case '+': return CellEvalValue{false, lnum + rnum, ""};
            case '-': return CellEvalValue{false, lnum - rnum, ""};
            case '*': return CellEvalValue{false, lnum * rnum, ""};
            case '/':
                if (std::abs(rnum) < 1e-12) throw EvalError{"#DIV/0!"};
                return CellEvalValue{false, lnum / rnum, ""};
            case '^': return CellEvalValue{false, std::pow(lnum, rnum), ""};
            default:  throw EvalError{"#OP!"};
        }
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

        try {
            for (PrePostIter it(root), end(nullptr); it != end; ++it) {
                AstNode& node = *it;
                AstToken& tok = node.value();

                if (it.phase() == Phase::Pre) {
                    if (tok.kind == AstNodeKind::BinaryOp) {
                        bool child_num = texcel_detail::binaryOpIsNumeric(tok.op);
                        if (node.m_pChild[0]) node.m_pChild[0]->value().expect_numeric = child_num;
                        if (node.m_pChild[1]) node.m_pChild[1]->value().expect_numeric = child_num;
                    }
                    continue;
                }

                if (!first) rpn_ss << " ";
                rpn_ss << "[" << tok.text << "]";
                first = false;

                if (tok.kind == AstNodeKind::CellRef) {
                    tok.evaluated = resolveOrThrow(tok.text, visiting);
                } else if (tok.kind == AstNodeKind::FuncCall) {
                    tok.evaluated = evalFunction(tok.func_id, collectFuncArgs(node), visiting);
                } else if (tok.kind == AstNodeKind::BinaryOp) {
                    if (!node.m_pChild[0] || !node.m_pChild[1]) throw EvalError{"#SYNTAX!"};
                    tok.evaluated = evalBinaryOp(
                        tok.op, node.m_pChild[0]->getValue().evaluated, node.m_pChild[1]->getValue().evaluated);
                }

                // Validación de tipo propagado en Phase::Pre (ej. `"Hola" + 5` -> `#VALUE!`)
                if (tok.expect_numeric && tok.evaluated.is_text) {
                    if (tok.evaluated.text.empty()) {
                        tok.evaluated = CellEvalValue{false, 0.0, ""};
                    } else if (texcel_detail::isPureNumber(tok.evaluated.text)) {
                        tok.evaluated = CellEvalValue{false, std::stod(tok.evaluated.text), ""};
                    } else {
                        throw EvalError{"#VALUE!"};
                    }
                }
            }
        } catch (const EvalError& e) {
            out_err = e.code;
            out_rpn = rpn_ss.str();
            return {};
        }

        out_rpn = rpn_ss.str();
        return root->getValue().evaluated;
    }

    CellEvalValue evaluateRawExpression(const std::string& raw,
                                        StringVec& visiting,
                                        StringVec& out_refs,
                                        std::string& out_rpn,
                                        std::string& out_err,
                                        std::string* out_syntax_msg = nullptr) {
        bool has_quote = (!raw.empty() && raw[0] == '\'');
        bool explicit_formula = (!raw.empty() && raw[0] == '=');
        if (!explicit_formula) {
            if (!has_quote && texcel_detail::isPureNumber(raw)) {
                double val = std::stod(raw);
                out_rpn = "[" + formatCellNumber(val) + "]";
                return CellEvalValue{false, val, ""};
            }
            std::string txt = has_quote ? raw.substr(1) : raw;
            out_rpn = "\"" + txt + "\"";
            return CellEvalValue{true, 0.0, txt};
        }

        AstNode* ast = tryParseAst(raw, out_refs, out_syntax_msg);
        if (!ast) {
            out_err = "#SYNTAX!";
            return {};
        }

        CellEvalValue eval_res = evaluateAstPostOrder(ast, visiting, out_rpn, out_err);
        texcel_detail::freeAst(ast);
        return eval_res;
    }

    void unsubscribeInputs(const std::string& cid) {
        if (!m_cells.contains(cid)) return;
        StringVec& old_inputs = m_cells[cid].inputs;
        for (size_t i = 0; i < old_inputs.size(); ++i) {
            const std::string& in_id = old_inputs[i].getValue();
            if (m_dependents.contains(in_id)) listRemove(m_dependents[in_id], cid);
        }
        old_inputs.clear();
    }

    CellEvalValue resolveCellValue(const std::string& cell_id, StringVec& visiting, std::string& out_err) {
        if (texcel_detail::containsStr(visiting, cell_id)) {
            out_err = "#CYCLE!";
            return {};
        }
        if (m_cells.contains(cell_id)) {
            const SpreadsheetCell& cached = m_cells[cell_id];
            if (!cached.dirty) {
                if (cached.has_error) {
                    out_err = cached.error_msg;
                    return {};
                }
                return cached.result;
            }
        }
        if (!hasRawCell(cell_id)) return {};

        visiting.push_back(cell_id);
        std::string raw = m_raw_table[cell_id];
        StringVec refs;
        std::string rpn, local_err;
        CellEvalValue eval_res = evaluateRawExpression(raw, visiting, refs, rpn, local_err);
        visiting.pop_back();

        unsubscribeInputs(cell_id);
        SpreadsheetCell& cell = m_cells[cell_id];
        cell.id = cell_id;
        cell.raw_expression = raw;
        cell.is_formula = (!raw.empty() && raw[0] == '=');
        cell.postorder_rpn = rpn;
        cell.inputs = refs;
        for (size_t i = 0; i < refs.size(); ++i) {
            const std::string& ref_id = refs[i].getValue();
            if (ref_id != cell_id) texcel_detail::addUniqueStr(m_dependents[ref_id], cell_id);
        }

        if (!local_err.empty()) {
            cell.has_error = true;
            cell.error_msg = local_err;
            cell.result = {};
            cell.dirty = false;
            out_err = local_err;
            return {};
        }

        cell.has_error = false;
        cell.error_msg.clear();
        cell.result = eval_res;
        cell.dirty = false;
        return eval_res;
    }

    StringVec collectAndMarkDirtySubgraph(const std::string& root_id) {
        StringVec affected;
        texcel_detail::StringSet seen;
        seen[root_id] = true;
        affected.push_back(root_id);
        for (size_t qi = 0; qi < affected.size(); ++qi) {
            std::string curr_id = affected[qi].getValue();
            if (m_cells.contains(curr_id)) m_cells[curr_id].dirty = true;
            if (m_dependents.contains(curr_id)) {
                for (const auto& dep_node : m_dependents[curr_id]) {
                    const std::string& dep_id = dep_node.getValue();
                    if (!seen.contains(dep_id)) {
                        seen[dep_id] = true;
                        affected.push_back(dep_id);
                    }
                }
            }
        }
        return affected;
    }

    bool recomputeCell(const std::string& cid, std::string* out_disp = nullptr) {
        std::string prev = m_cells.contains(cid) ? m_cells[cid].displayValue() : "";
        StringVec visiting;
        std::string err;
        resolveCellValue(cid, visiting, err);
        std::string now = m_cells.contains(cid) ? m_cells[cid].displayValue() : "";
        if (out_disp) *out_disp = now;
        return now != prev;
    }

    /**
     * @brief Propaga el recálculo en cascada sobre los descendientes marcados como `dirty`.
     */
    StringVec propagateAffected(const StringVec& affected) {
        StringVec updated;
        for (size_t qi = 1; qi < affected.size(); ++qi) {
            std::string curr_id = affected[qi].getValue();
            if (!hasRawCell(curr_id)) continue;
            std::string now_disp;
            if (recomputeCell(curr_id, &now_disp)) {
                updated.push_back(curr_id + "=" + now_disp);
            }
        }
        return updated;
    }

    StringVec propagateFrom(const std::string& root_id) {
        return propagateAffected(collectAndMarkDirtySubgraph(root_id));
    }

public:
    SpreadsheetEngine() {
        addLog("Sesión iniciada.");
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
        m_dependents.clear();

        BaseHashTable<HashTableTraits<std::string, std::string>, Callable> prev_display;
        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            prev_display[cid] = m_cells.contains(cid) ? m_cells[cid].displayValue() : "";
        }
        m_cells.clear();

        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            if (!entry_node.getValue().getValue().empty()) recomputeCell(cid);
        }

        StringVec updated;
        for (const auto& entry_node : m_raw_table) {
            std::string cid = entry_node.getValue().getKey();
            if (entry_node.getValue().getValue().empty() || cid == changed_id) continue;

            std::string now_disp = m_cells[cid].displayValue();
            if (now_disp != prev_display[cid]) updated.push_back(cid + "=" + now_disp);
        }
        return updated;
    }

    /**
     * @brief Asigna un valor o fórmula a una celda (ej. `setCell("A1", "5")`, `setCell("B1", "= A1 + 4")`).
     */
    bool setCell(const std::string& cell_id_raw, const std::string& expression_raw) {
        CellCoord coord = CellCoord::parse(cell_id_raw);
        if (!coord.valid) {
            addLog("✖ Coordenada inválida: " + cell_id_raw);
            return false;
        }
        std::string cid = coord.toId();
        std::string expr = texcel_detail::trim(expression_raw);

        unsubscribeInputs(cid);
        StringVec affected = collectAndMarkDirtySubgraph(cid);

        if (expr.empty()) {
            m_raw_table[cid] = "";
            m_cells[cid] = SpreadsheetCell{.id = cid, .dirty = false};
            propagateAffected(affected);
            addLog("⌫ " + cid + " limpiada");
            return true;
        }

        m_raw_table[cid] = expr;
        recomputeCell(cid);
        StringVec cascaded = propagateAffected(affected);

        const SpreadsheetCell& cell = m_cells[cid];
        std::string log_expr = (!expr.empty() && expr[0] == '=') ? texcel_detail::trim(expr.substr(1)) : expr;
        std::ostringstream log_ss;
        log_ss << (cell.has_error ? "⚠ " : "✔ ") << cid << " = " << log_expr << " ➔ " << cell.displayValue();
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
        std::string s = texcel_detail::trim(cmd);
        if (s.empty()) return false;

        ParsedAssignment pa = parseAssignment(s, fallback_cell);
        std::string target = pa.has_invalid_lhs ? fallback_cell : pa.target_cell;
        if (out_assigned_cell) *out_assigned_cell = target;
        return setCell(target, pa.has_invalid_lhs ? s : pa.expression);
    }

    /** @brief Consulta el estado de una celda (retorna celda vacía por defecto si no existe). */
    SpreadsheetCell getCell(const std::string& cell_id) {
        CellCoord c = CellCoord::parse(cell_id);
        std::string cid = c.valid ? c.toId() : cell_id;
        if (m_cells.contains(cid)) return m_cells[cid];
        return SpreadsheetCell{.id = cid};
    }

    /** @brief Obtiene la lista de celdas de las que depende directamente `cell_id` (sus entradas). */
    StringVec getCellInputs(const std::string& cell_id) {
        return getCell(cell_id).inputs;
    }

    /** @brief Obtiene la lista de celdas que dependen de `cell_id` desde su `LinkedList` de observadores. */
    StringVec getCellDependents(const std::string& cell_id) {
        StringVec res;
        if (!m_dependents.contains(cell_id)) return res;
        for (const auto& node : m_dependents[cell_id]) {
            texcel_detail::addUniqueStr(res, node.getValue());
        }
        return res;
    }

    /** @brief Indica si una cadena en mayúsculas corresponde a un comando administrativo de la consola. */
    static bool isAdminCommand(const std::string& upper_cmd) {
        return upper_cmd == "CLEAR" || upper_cmd == "QUIT" || upper_cmd == "EXIT" ||
               upper_cmd == "Q" || upper_cmd == "SAVE" || upper_cmd == "LOAD";
    }

    /**
     * @brief Evalúa en vivo una entrada parcial de consola sin mutar la hoja de cálculo.
     *
     * Permite que las 4 ventanas (Matriz, Inspector AST y Preview) se actualicen mientras el usuario tipea.
     */
    EvalPreview previewCommand(const std::string& raw_input, const std::string& active_cell_id) {
        EvalPreview p;
        std::string s = texcel_detail::trim(raw_input);
        if (s.empty() || isAdminCommand(texcel_detail::toUpper(s))) return p;

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
        CellEvalValue eval_res = evaluateRawExpression(
            p.expression, visiting, p.referenced_cells, rpn, err, &syntax_msg);

        p.postorder_rpn = rpn;
        if (!err.empty()) {
            p.valid = false;
            p.error_msg = (!syntax_msg.empty()) ? syntax_msg : err;
            return p;
        }

        p.valid = true;
        p.result = eval_res;
        return p;
    }

    /** @brief Limpia un rango rectangular de celdas y propaga dependencias. */
    void clearRange(TX c0, TX r0, TX c1, TX r1) {
        texcel_detail::forEachCellInRange(CellCoord(c0, r0, true), CellCoord(c1, r1, true), [&](const std::string& cid) {
            if (hasRawCell(cid)) {
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
        if (!hasRawCell(src_id)) return 0;

        SpreadsheetCell src_cell = getCell(src_id);
        std::string src_raw = m_raw_table[src_id];
        TX cmin = std::min(c0, c1), cmax = std::max(c0, c1);
        TX rmin = std::min(r0, r1), rmax = std::max(r0, r1);

        // Detectar serie aritmética si el origen y su vecino inmediato son números puros
        bool is_series = false;
        double series_base = 0.0, series_step = 0.0;
        bool fill_vertical = (rmax - rmin) >= (cmax - cmin);
        if (!src_cell.is_formula && !src_cell.result.is_text && texcel_detail::isPureNumber(src_raw)) {
            series_base = std::stod(src_raw);
            TX nc = fill_vertical ? c0 : (c0 + (c1 >= c0 ? 1 : -1));
            TX nr = fill_vertical ? (r0 + (r1 >= r0 ? 1 : -1)) : r0;
            if (nc >= cmin && nc <= cmax && nr >= rmin && nr <= rmax) {
                std::string next_id = CellCoord(nc, nr, true).toId();
                if (m_raw_table.contains(next_id) && texcel_detail::isPureNumber(m_raw_table[next_id])) {
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
                    setCell(dst_id, shiftFormulaRefs(src_raw, dcol, drow));
                } else if (is_series) {
                    TX step_idx = fill_vertical ? drow : dcol;
                    setCell(dst_id, formatCellNumber(series_base + step_idx * series_step));
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
        m_dependents.clear();
        addLog("Hoja de cálculo reiniciada");
    }
};

#endif // __TEXCEL_ENGINE_H__
