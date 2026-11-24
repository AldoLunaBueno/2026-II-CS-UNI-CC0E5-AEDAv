/**
 * @file engine.h
 * @ingroup texcel
 * @brief Motor de hoja de cálculo reactiva (`HashTable` + `BinaryTree` PrePostOrder + `LinkedList` + `Vector`).
 */

#ifndef __TEXCEL_ENGINE_H__
#define __TEXCEL_ENGINE_H__

#include "types.h"
#include "parser.h"
#include "../containers/hash_table.h"
#include "../containers/lists/linked_list.h"

/**
 * @class SpreadsheetEngine
 * @brief Motor reactivo basado en los contenedores propios del curso (`HashTable`, `BinaryTree`, `LinkedList`, `Vector`).
 *
 * - Persiste celdas dispersas en `HashTable` (`Printable` / `Readable`).
 * - Compila fórmulas en `BinaryTreeNode<AstToken>` y las evalúa in-place mediante `BinaryTreeIterator<Order::PrePostOrder>`
 *   con cortocircuito real en `IF` / `IFERROR` durante `Phase::Pre`.
 * - Suscribe dependencias puntuales en `DepGraph` (`LinkedList`) y rangos rectangulares en `RangeSubVec`.
 * - Mantiene un índice disperso por fila sobre `BaseVector` hacia nodos estables de `m_cells` (`BaseAVLTree`) para
 *   lectura en $O(1)$ durante agregaciones masivas (`COUNTIFS`, `SUM`, `AVG`).
 */
class SpreadsheetEngine {
public:
    using StringVec   = Vector<VectorAscTraits<std::string>>;
    using RangeVec    = Vector<VectorAscTraits<CellRangeRect>>;
    using DepList     = LinkedList<LinkedListAscTraits<std::string>>;
    using DepGraph    = BaseHashTable<HashTableTraits<std::string, DepList>, Callable>;
    using AstNode     = BinaryTreeNode<AstToken>;
    using PrePostIter = BinaryTreeIterator<AstToken, Order::PrePostOrder, Direction::Asc>;
    using RawTable    = HashTable<HashTableTraits<std::string, std::string>>;
    using CellTable   = BaseHashTable<HashTableTraits<std::string, SpreadsheetCell>, Callable>;

private:
    struct RangeSubscription {
        std::string   subscriber_id;
        CellRangeRect rect;

        auto operator<=>(const RangeSubscription&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const RangeSubscription& s) {
            return os << s.subscriber_id << "@" << s.rect.toId();
        }
    };

    using RangeSubVec = BaseVector<VectorAscTraits<RangeSubscription>, Callable>;
    using CellPtrRow  = BaseVector<VectorAscTraits<SpreadsheetCell*>, Callable>;
    using GridIndex   = BaseVector<VectorAscTraits<CellPtrRow>, Callable>;

    RawTable    m_raw_table;      /**< Tabla serializable `(celda -> fórmula)` con mixins `Printable` y `Readable`. */
    CellTable   m_cells;          /**< Tabla de estado evaluado de cada celda activa (direcciones AVL estables). */
    DepGraph    m_dependents;     /**< `celda -> LinkedList` de observadores puntuales. */
    RangeSubVec m_range_subs;     /**< Suscripciones compactas a rangos rectangulares. */
    GridIndex   m_grid_index;     /**< Índice disperso `m_grid_index[row][col] -> SpreadsheetCell*` para lectura $O(1)$. */
    TX          m_max_used_col = -1;
    TX          m_max_used_row = -1;
    StringVec   m_logs;           /**< Historial cronológico de operaciones y propagaciones. */

    static void listRemove(DepList& lst, const std::string& target) {
        DepList filtered;
        for (const auto& node : lst) {
            if (node.getValue() != target) filtered.push_back(node.getValue(), 0);
        }
        lst = std::move(filtered);
    }

    void setGridPtr(TX col, TX row, SpreadsheetCell* ptr) {
        if (col < 0 || row < 0 || col >= CellCoord::MAX_COLS || row >= CellCoord::MAX_ROWS) return;
        while (static_cast<TX>(m_grid_index.size()) <= row) {
            m_grid_index.push_back(CellPtrRow{}, 0);
        }
        CellPtrRow& rvec = m_grid_index[static_cast<size_t>(row)].value();
        while (static_cast<TX>(rvec.size()) <= col) {
            rvec.push_back(nullptr, 0);
        }
        rvec[static_cast<size_t>(col)].value() = ptr;
        if (ptr && !ptr->raw_expression.empty()) {
            m_max_used_col = std::max(m_max_used_col, col);
            m_max_used_row = std::max(m_max_used_row, row);
        }
    }

    SpreadsheetCell* fastGridPtr(TX col, TX row) {
        if (row < 0 || col < 0 || static_cast<size_t>(row) >= m_grid_index.size()) return nullptr;
        CellPtrRow& rvec = m_grid_index[static_cast<size_t>(row)].value();
        if (static_cast<size_t>(col) >= rvec.size()) return nullptr;
        return rvec[static_cast<size_t>(col)].getValue();
    }

    const SpreadsheetCell* fastGridPtr(TX col, TX row) const {
        if (row < 0 || col < 0 || static_cast<size_t>(row) >= m_grid_index.size()) return nullptr;
        const CellPtrRow& rvec = m_grid_index[static_cast<size_t>(row)].getValue();
        if (static_cast<size_t>(col) >= rvec.size()) return nullptr;
        return rvec[static_cast<size_t>(col)].getValue();
    }

    bool hasRawCell(const std::string& cid) {
        const std::string* raw = m_raw_table.find(cid);
        return raw && !raw->empty();
    }

public:
    static AstNode* tryParseAst(const std::string& expr,
                                StringVec* out_refs = nullptr,
                                std::string* out_err_msg = nullptr,
                                RangeVec* out_ranges = nullptr) {
        try {
            FormulaParser parser(expr, out_refs, out_ranges);
            return parser.parse();
        } catch (const std::exception& e) {
            if (out_err_msg) *out_err_msg = e.what();
            return nullptr;
        }
    }

    static AstNode* tryParseAst(const std::string& expr, StringVec& out_refs, std::string* out_err_msg = nullptr) {
        return tryParseAst(expr, &out_refs, out_err_msg, nullptr);
    }

private:
    static bool canParseFormula(const std::string& expr) {
        AstNode* ast = tryParseAst(expr, nullptr, nullptr, nullptr);
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

        std::string lhs = texcel_detail::trim(std::string_view(s).substr(0, eq_pos));
        std::string rhs = texcel_detail::trim(std::string_view(s).substr(eq_pos + 1));
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

    struct ParsedCriterion {
        bool          is_numeric_eq = false;
        double        num_val = 0.0;
        char          op_code = 0; // 0 = igualdad textual directa
        bool          rhs_is_num = false;
        double        rhs_num = 0.0;
        std::string   rhs_str;
        bool          matches_empty = false;
    };

    static ParsedCriterion compileCriterion(const CellEvalValue& crit) {
        ParsedCriterion pc;
        if (!crit.is_text) {
            pc.is_numeric_eq = true;
            pc.num_val = crit.num;
            pc.matches_empty = false;
            return pc;
        }

        const std::string& raw_c = crit.text;
        for (const auto& info : texcel_detail::BINARY_OPS) {
            if (info.prec != 1) continue;
            std::string_view lex(info.lexeme);
            if (raw_c.starts_with(lex)) {
                pc.op_code = info.code;
                pc.rhs_str = texcel_detail::trim(std::string_view(raw_c).substr(lex.size()));
                break;
            }
        }
        if (pc.op_code == 0) {
            pc.rhs_str = raw_c;
            pc.matches_empty = raw_c.empty();
            return pc;
        }
        if (pc.rhs_str.empty()) {
            pc.matches_empty = (pc.op_code == '=');
            return pc;
        }
        if (texcel_detail::isPureNumber(pc.rhs_str)) {
            pc.rhs_is_num = true;
            pc.rhs_num = std::stod(pc.rhs_str);
            pc.matches_empty = (pc.op_code == '!');
            return pc;
        }
        pc.matches_empty = (pc.op_code == '!');
        return pc;
    }

    static bool matchesCompiledCriterion(const CellEvalValue& cell, bool cell_empty, const ParsedCriterion& pc) {
        if (cell_empty) return pc.matches_empty;
        if (pc.is_numeric_eq) {
            return !cell.is_text && std::abs(cell.num - pc.num_val) < 1e-9;
        }
        if (pc.op_code == 0) {
            return !pc.rhs_str.empty() && cell.asString() == pc.rhs_str;
        }
        if (pc.rhs_str.empty()) {
            if (pc.op_code == '!') return true;
            return false;
        }
        if (pc.rhs_is_num) {
            if (cell.is_text) return pc.op_code == '!';
            return texcel_detail::compareValues(pc.op_code, cell, CellEvalValue{false, pc.rhs_num, "", ""});
        }
        std::string cs = cell.asString();
        if (pc.op_code == '=') return cs == pc.rhs_str;
        if (pc.op_code == '!') return cs != pc.rhs_str;
        return texcel_detail::compareValues(pc.op_code, CellEvalValue{true, 0.0, cs, ""}, CellEvalValue{true, 0.0, pc.rhs_str, ""});
    }

    CellEvalValue resolveById(const std::string& cid, StringVec& visiting) {
        std::string err;
        CellEvalValue val = resolveCellValue(cid, visiting, err);
        if (!err.empty()) return CellEvalValue::makeError(err);
        return val;
    }

    CellEvalValue readCellByCoord(TX col, TX row, StringVec& visiting, bool& out_is_empty) {
        SpreadsheetCell* ptr = fastGridPtr(col, row);
        if (ptr) {
            if (visiting.size() > 0 && texcel_detail::containsStr(visiting, ptr->id)) {
                out_is_empty = false;
                return CellEvalValue::makeError("#CYCLE!");
            }
            if (!ptr->dirty) {
                out_is_empty = ptr->raw_expression.empty();
                if (out_is_empty) return {};
                if (ptr->has_error) return CellEvalValue::makeError(ptr->error_msg);
                return ptr->result;
            }
            out_is_empty = ptr->raw_expression.empty();
            if (out_is_empty) return {};
            std::string err;
            CellEvalValue val = resolveCellValue(ptr->id, visiting, err, nullptr, ptr);
            return err.empty() ? val : CellEvalValue::makeError(err);
        }

        // Si la fila/columna supera el bounding box conocido y m_grid_index está sincronizado, verificar tabla cruda solo si aplica
        if (col > m_max_used_col || row > m_max_used_row) {
            out_is_empty = true;
            return {};
        }

        std::string cid = CellCoord(col, row, true).toId();
        if (visiting.size() > 0 && texcel_detail::containsStr(visiting, cid)) {
            out_is_empty = false;
            return CellEvalValue::makeError("#CYCLE!");
        }
        if (SpreadsheetCell* found = m_cells.find(cid)) {
            setGridPtr(col, row, found);
            if (!found->dirty) {
                out_is_empty = found->raw_expression.empty();
                if (out_is_empty) return {};
                if (found->has_error) return CellEvalValue::makeError(found->error_msg);
                return found->result;
            }
        }
        const std::string* raw_ptr = m_raw_table.find(cid);
        if (!raw_ptr || raw_ptr->empty()) {
            out_is_empty = true;
            return {};
        }
        out_is_empty = false;
        std::string err;
        CellEvalValue val = resolveCellValue(cid, visiting, err, raw_ptr, nullptr);
        return err.empty() ? val : CellEvalValue::makeError(err);
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
    CellEvalValue forEachArgValue(const Vector<VectorAscTraits<AstNode*>>& args, StringVec& visiting, Fn&& fn) {
        for (size_t i = 0; i < args.size(); ++i) {
            const AstToken& at = args[i].getValue()->getValue();
            if (at.kind == AstNodeKind::CellRange) {
                TX c0 = std::min(at.range_a.col, at.range_b.col);
                TX c1 = std::max(at.range_a.col, at.range_b.col);
                TX r0 = std::min(at.range_a.row, at.range_b.row);
                TX r1 = std::max(at.range_a.row, at.range_b.row);
                for (TX r = r0; r <= r1; ++r) {
                    for (TX c = c0; c <= c1; ++c) {
                        bool is_empty = false;
                        CellEvalValue cv = readCellByCoord(c, r, visiting, is_empty);
                        if (cv.hasError()) return cv;
                        fn(cv, true, is_empty);
                    }
                }
            } else {
                if (at.evaluated.hasError()) return at.evaluated;
                fn(at.evaluated, false, at.evaluated.is_text && at.evaluated.text.empty());
            }
        }
        return {};
    }

    static bool coerceStrictNumber(const CellEvalValue& ev, double& out_num) {
        if (ev.hasError()) return false;
        if (!ev.is_text) { out_num = ev.num; return true; }
        if (ev.text.empty()) { out_num = 0.0; return true; }
        if (texcel_detail::isPureNumber(ev.text)) { out_num = std::stod(ev.text); return true; }
        return false;
    }

    CellEvalValue evalCountIf(const Vector<VectorAscTraits<AstNode*>>& args, StringVec& visiting) {
        const size_t n_conds = args.size() / 2;
        if (n_conds == 0 || args.size() % 2 != 0) return CellEvalValue::makeError("#VALUE!");

        TX base_cols = 0, base_rows = 0;
        Vector<VectorAscTraits<CellCoord>> starts;
        std::vector<ParsedCriterion> crits;
        crits.reserve(n_conds);
        bool all_match_empty = true;

        for (size_t k = 0; k < n_conds; ++k) {
            const AstToken& rt = args[2 * k].getValue()->getValue();
            CellCoord a, b;
            if (rt.kind == AstNodeKind::CellRange) {
                a = rt.range_a;
                b = rt.range_b;
            } else if (rt.kind == AstNodeKind::CellRef) {
                a = b = CellCoord::parse(rt.text);
            } else {
                return CellEvalValue::makeError("#VALUE!");
            }
            const CellEvalValue& crit_ev = args[2 * k + 1].getValue()->getValue().evaluated;
            if (crit_ev.hasError()) return crit_ev;

            TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
            TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
            TX cols = c_max - c_min + 1;
            TX rows = r_max - r_min + 1;
            if (k == 0) {
                base_cols = cols;
                base_rows = rows;
            } else if (cols != base_cols || rows != base_rows) {
                return CellEvalValue::makeError("#VALUE!");
            }
            starts.push_back(CellCoord(c_min, r_min, true));
            ParsedCriterion pc = compileCriterion(crit_ev);
            if (!pc.matches_empty) all_match_empty = false;
            crits.push_back(std::move(pc));
        }

        // Si al menos un criterio NO coincide con celdas vacías (caso habitual en COUNTIF/COUNTIFS),
        // podemos acotar dr y dc al máximo desplazamiento donde algún rango aún intersecta m_max_used_col/row.
        TX max_dr = base_rows - 1;
        TX max_dc = base_cols - 1;
        if (!all_match_empty) {
            if (m_max_used_col < 0 || m_max_used_row < 0) {
                return CellEvalValue{false, 0.0, "", ""};
            }
            for (size_t k = 0; k < n_conds; ++k) {
                if (!crits[k].matches_empty) {
                    CellCoord s = starts[k].getValue();
                    max_dc = std::min(max_dc, m_max_used_col - s.col);
                    max_dr = std::min(max_dr, m_max_used_row - s.row);
                }
            }
            if (max_dc < 0 || max_dr < 0) {
                return CellEvalValue{false, 0.0, "", ""};
            }
        }

        TX matched = 0;
        for (TX dr = 0; dr <= max_dr; ++dr) {
            for (TX dc = 0; dc <= max_dc; ++dc) {
                bool all_match = true;
                for (size_t k = 0; k < n_conds; ++k) {
                    CellCoord s = starts[k].getValue();
                    bool is_empty = false;
                    CellEvalValue cv = readCellByCoord(s.col + dc, s.row + dr, visiting, is_empty);
                    if (cv.hasError()) return cv;
                    if (!matchesCompiledCriterion(cv, is_empty, crits[k])) {
                        all_match = false;
                        break;
                    }
                }
                if (all_match) ++matched;
            }
        }
        return CellEvalValue{false, static_cast<double>(matched), "", ""};
    }

    CellEvalValue evalFunction(FuncId fn,
                               const Vector<VectorAscTraits<AstNode*>>& args,
                               StringVec& visiting) {
        switch (fn) {
            case FuncId::If: {
                if (args.size() < 2 || args.size() > 3) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& cond = args[0].getValue()->getValue().evaluated;
                if (cond.hasError()) return cond;
                if (texcel_detail::isTruthyValue(cond)) {
                    return args[1].getValue()->getValue().evaluated;
                }
                if (args.size() == 3) {
                    return args[2].getValue()->getValue().evaluated;
                }
                return CellEvalValue{true, 0.0, "False", ""};
            }
            case FuncId::IfError: {
                if (args.size() != 2) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& v = args[0].getValue()->getValue().evaluated;
                return v.hasError() ? args[1].getValue()->getValue().evaluated : v;
            }
            case FuncId::Not: {
                if (args.size() != 1) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& v = args[0].getValue()->getValue().evaluated;
                if (v.hasError()) return v;
                return CellEvalValue{true, 0.0, texcel_detail::isTruthyValue(v) ? "False" : "True", ""};
            }
            case FuncId::And:
            case FuncId::Or: {
                bool is_and = (fn == FuncId::And);
                bool acc = is_and;
                bool any_seen = false;
                CellEvalValue err = forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool from_range, bool is_empty) {
                    if (from_range && is_empty) return;
                    any_seen = true;
                    bool b = texcel_detail::isTruthyValue(ev);
                    if (is_and) acc = acc && b;
                    else acc = acc || b;
                });
                if (err.hasError()) return err;
                if (!any_seen) return CellEvalValue::makeError("#VALUE!");
                return CellEvalValue{true, 0.0, acc ? "True" : "False", ""};
            }
            case FuncId::Round: {
                if (args.size() < 1 || args.size() > 2) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& a0 = args[0].getValue()->getValue().evaluated;
                if (a0.hasError()) return a0;
                double val = 0.0;
                if (!coerceStrictNumber(a0, val)) return CellEvalValue::makeError("#VALUE!");
                long long dec = 0;
                if (args.size() == 2) {
                    const CellEvalValue& a1 = args[1].getValue()->getValue().evaluated;
                    if (a1.hasError()) return a1;
                    double dval = 0.0;
                    if (!coerceStrictNumber(a1, dval)) return CellEvalValue::makeError("#VALUE!");
                    dec = std::clamp<long long>(std::llround(dval), -12, 12);
                }
                double factor = std::pow(10.0, static_cast<double>(dec));
                double res = (factor != 0.0) ? (std::round(val * factor) / factor) : val;
                return CellEvalValue{false, res, "", ""};
            }
            case FuncId::Abs: {
                if (args.size() != 1) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& a0 = args[0].getValue()->getValue().evaluated;
                if (a0.hasError()) return a0;
                double val = 0.0;
                if (!coerceStrictNumber(a0, val)) return CellEvalValue::makeError("#VALUE!");
                return CellEvalValue{false, std::abs(val), "", ""};
            }
            case FuncId::Sqrt: {
                if (args.size() != 1) return CellEvalValue::makeError("#SYNTAX!");
                const CellEvalValue& a0 = args[0].getValue()->getValue().evaluated;
                if (a0.hasError()) return a0;
                double val = 0.0;
                if (!coerceStrictNumber(a0, val)) return CellEvalValue::makeError("#VALUE!");
                if (val < -1e-12) return CellEvalValue::makeError("#NUM!");
                return CellEvalValue{false, std::sqrt(std::max(0.0, val)), "", ""};
            }
            case FuncId::Count:
            case FuncId::CountA: {
                bool count_all = (fn == FuncId::CountA);
                TX cnt = 0;
                CellEvalValue err = forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool from_range, bool is_empty) {
                    if (is_empty) return;
                    if (count_all) {
                        ++cnt;
                    } else if (!ev.is_text || (!from_range && texcel_detail::isPureNumber(ev.text))) {
                        ++cnt;
                    }
                });
                if (err.hasError()) return err;
                return CellEvalValue{false, static_cast<double>(cnt), "", ""};
            }
            case FuncId::Concat: {
                std::string out_s;
                CellEvalValue err = forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool, bool is_empty) {
                    if (!is_empty) out_s += ev.asString();
                });
                if (err.hasError()) return err;
                return CellEvalValue{true, 0.0, out_s, ""};
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
                bool bad_val = false;
                auto foldNum = [&](double v) {
                    if (count == 0) { mn = mx = v; }
                    else { mn = std::min(mn, v); mx = std::max(mx, v); }
                    acc += v;
                    ++count;
                };
                CellEvalValue err = forEachArgValue(args, visiting, [&](const CellEvalValue& ev, bool from_range, bool) {
                    if (bad_val) return;
                    if (!ev.is_text) foldNum(ev.num);
                    else if (from_range) return;
                    else if (ev.text.empty()) foldNum(0.0);
                    else if (texcel_detail::isPureNumber(ev.text)) foldNum(std::stod(ev.text));
                    else bad_val = true;
                });
                if (err.hasError()) return err;
                if (bad_val) return CellEvalValue::makeError("#VALUE!");
                if (fn == FuncId::Sum) return CellEvalValue{false, acc, "", ""};
                if (fn == FuncId::Avg) return CellEvalValue{false, count > 0 ? acc / count : 0.0, "", ""};
                if (fn == FuncId::Min) return CellEvalValue{false, mn, "", ""};
                return CellEvalValue{false, mx, "", ""};
            }
            default:
                return CellEvalValue::makeError("#SYNTAX!");
        }
    }

    static CellEvalValue evalBinaryOp(char op, const CellEvalValue& lhs, const CellEvalValue& rhs) {
        if (lhs.hasError()) return lhs;
        if (rhs.hasError()) return rhs;
        double lnum = lhs.asNumber(), rnum = rhs.asNumber();
        switch (op) {
            case '&': return CellEvalValue{true, 0.0, lhs.asString() + rhs.asString(), ""};
            case '=': case '!': case '<': case '>': case 'L': case 'G':
                return CellEvalValue{false, texcel_detail::compareValues(op, lhs, rhs) ? 1.0 : 0.0, "", ""};
            case '+': return CellEvalValue{false, lnum + rnum, "", ""};
            case '-': return CellEvalValue{false, lnum - rnum, "", ""};
            case '*': return CellEvalValue{false, lnum * rnum, "", ""};
            case '/':
                if (std::abs(rnum) < 1e-12) return CellEvalValue::makeError("#DIV/0!");
                return CellEvalValue{false, lnum / rnum, "", ""};
            case '^': {
                double res = std::pow(lnum, rnum);
                if (std::isnan(res) || std::isinf(res)) return CellEvalValue::makeError("#NUM!");
                return CellEvalValue{false, res, "", ""};
            }
            default: return CellEvalValue::makeError("#OP!");
        }
    }

    /**
     * @brief Evalúa un árbol `BinaryTreeNode<AstToken>` in-place sin pila externa mediante `Order::PrePostOrder`.
     *
     * - `Phase::Pre` (bajando): propaga `expect_numeric` y ejecuta cortocircuito real (`skip_eval`) sobre ramas no
     *   tomadas de `IF` e `IFERROR` (ya que `m_pChild[0]` completó su `Phase::Post` antes de entrar a `m_pChild[1]`).
     * - `Phase::Post` (subiendo): registra RPN, evalúa nodos activos y propaga `CellEvalValue` hacia la raíz.
     */
    CellEvalValue evaluateAstPostOrder(AstNode* root,
                                       StringVec& visiting,
                                       std::string* out_rpn,
                                       std::string& out_err) {
        if (!root) return {};
        if (out_rpn) out_rpn->clear();
        bool first = true;

        for (PrePostIter it(root), end(nullptr); it != end; ++it) {
            AstNode& node = *it;
            AstToken& tok = node.value();

            if (it.phase() == Phase::Pre) {
                // Cortocircuito en Phase::Pre al entrar al hijo derecho de IF o IFERROR
                if (node.m_parent && &node == node.m_parent->m_pChild[1]) {
                    const AstToken& ptok = node.m_parent->getValue();
                    if (ptok.kind == AstNodeKind::FuncCall && !ptok.skip_eval && node.m_parent->m_pChild[0]) {
                        const CellEvalValue& first_arg = node.m_parent->m_pChild[0]->getValue().evaluated;
                        if (ptok.func_id == FuncId::If) {
                            if (tok.kind == AstNodeKind::ArgPair) {
                                // IF de 3 argumentos: ArgPair(si_ok, si_no)
                                if (first_arg.hasError()) {
                                    tok.skip_eval = true;
                                } else if (texcel_detail::isTruthyValue(first_arg)) {
                                    if (node.m_pChild[1]) node.m_pChild[1]->value().skip_eval = true;
                                } else {
                                    if (node.m_pChild[0]) node.m_pChild[0]->value().skip_eval = true;
                                }
                            } else {
                                // IF de 2 argumentos: si_ok directo en m_pChild[1]
                                if (first_arg.hasError() || !texcel_detail::isTruthyValue(first_arg)) {
                                    tok.skip_eval = true;
                                }
                            }
                        } else if (ptok.func_id == FuncId::IfError) {
                            if (!first_arg.hasError()) {
                                tok.skip_eval = true;
                            }
                        }
                    }
                }

                if (tok.skip_eval) {
                    if (node.m_pChild[0]) node.m_pChild[0]->value().skip_eval = true;
                    if (node.m_pChild[1]) node.m_pChild[1]->value().skip_eval = true;
                    continue;
                }

                if (tok.kind == AstNodeKind::BinaryOp) {
                    bool child_num = texcel_detail::binaryOpIsNumeric(tok.op);
                    if (node.m_pChild[0]) node.m_pChild[0]->value().expect_numeric = child_num;
                    if (node.m_pChild[1]) node.m_pChild[1]->value().expect_numeric = child_num;
                }
                continue;
            }

            if (out_rpn) {
                if (!first) out_rpn->push_back(' ');
                out_rpn->push_back('[');
                out_rpn->append(tok.text);
                out_rpn->push_back(']');
                first = false;
            }

            if (tok.skip_eval) continue;

            if (tok.kind == AstNodeKind::CellRef) {
                CellCoord c = CellCoord::parse(tok.text);
                if (c.valid) {
                    bool is_empty = false;
                    tok.evaluated = readCellByCoord(c.col, c.row, visiting, is_empty);
                } else {
                    tok.evaluated = resolveById(tok.text, visiting);
                }
            } else if (tok.kind == AstNodeKind::FuncCall) {
                tok.evaluated = evalFunction(tok.func_id, collectFuncArgs(node), visiting);
            } else if (tok.kind == AstNodeKind::BinaryOp) {
                if (!node.m_pChild[0] || !node.m_pChild[1]) {
                    tok.evaluated = CellEvalValue::makeError("#SYNTAX!");
                } else {
                    tok.evaluated = evalBinaryOp(
                        tok.op, node.m_pChild[0]->getValue().evaluated, node.m_pChild[1]->getValue().evaluated);
                }
            }

            // Validación de tipo propagado en Phase::Pre (ej. `"Hola" + 5` -> `#VALUE!`)
            if (!tok.evaluated.hasError() && tok.expect_numeric && tok.evaluated.is_text) {
                if (tok.evaluated.text.empty()) {
                    tok.evaluated = CellEvalValue{false, 0.0, "", ""};
                } else if (texcel_detail::isPureNumber(tok.evaluated.text)) {
                    tok.evaluated = CellEvalValue{false, std::stod(tok.evaluated.text), "", ""};
                } else {
                    tok.evaluated = CellEvalValue::makeError("#VALUE!");
                }
            }
        }

        const CellEvalValue& root_val = root->getValue().evaluated;
        if (root_val.hasError()) {
            out_err = root_val.err_code;
            return {};
        }
        return root_val;
    }

    static CellEvalValue evaluateLiteralFast(const std::string& raw, std::string* out_rpn = nullptr) {
        bool has_quote = (!raw.empty() && raw[0] == '\'');
        if (!has_quote && texcel_detail::isPureNumber(raw)) {
            double val = std::stod(raw);
            if (out_rpn) *out_rpn = "[" + formatCellNumber(val) + "]";
            return CellEvalValue{false, val, "", ""};
        }
        std::string txt = has_quote ? raw.substr(1) : raw;
        if (out_rpn) *out_rpn = "\"" + txt + "\"";
        return CellEvalValue{true, 0.0, std::move(txt), ""};
    }

    CellEvalValue evaluateRawExpression(const std::string& raw,
                                        StringVec& visiting,
                                        StringVec* out_refs,
                                        RangeVec* out_ranges,
                                        std::string* out_rpn,
                                        std::string& out_err,
                                        std::string* out_syntax_msg = nullptr) {
        bool explicit_formula = (!raw.empty() && raw[0] == '=');
        if (!explicit_formula) {
            return evaluateLiteralFast(raw, out_rpn);
        }

        AstNode* ast = tryParseAst(raw, out_refs, out_syntax_msg, out_ranges);
        if (!ast) {
            out_err = "#SYNTAX!";
            return {};
        }

        CellEvalValue eval_res = evaluateAstPostOrder(ast, visiting, out_rpn, out_err);
        texcel_detail::freeAst(ast);
        return eval_res;
    }

    void unsubscribeInputs(const std::string& cid) {
        SpreadsheetCell* cell_ptr = m_cells.find(cid);
        if (!cell_ptr) return;
        StringVec& old_inputs = cell_ptr->inputs;
        for (size_t i = 0; i < old_inputs.size(); ++i) {
            const std::string& in_id = old_inputs[i].getValue();
            if (DepList* deps = m_dependents.find(in_id)) listRemove(*deps, cid);
        }
        old_inputs.clear();

        if (cell_ptr->range_inputs.size() > 0) {
            RangeSubVec kept;
            for (size_t i = 0; i < m_range_subs.size(); ++i) {
                const RangeSubscription& sub = m_range_subs[i].getValue();
                if (sub.subscriber_id != cid) kept.push_back(sub, 0);
            }
            m_range_subs = std::move(kept);
            cell_ptr->range_inputs.clear();
        }
    }

    CellEvalValue resolveCellValue(const std::string& cell_id,
                                   StringVec& visiting,
                                   std::string& out_err,
                                   const std::string* known_raw = nullptr,
                                   SpreadsheetCell* known_cell = nullptr) {
        if (visiting.size() > 0 && texcel_detail::containsStr(visiting, cell_id)) {
            out_err = "#CYCLE!";
            return {};
        }
        SpreadsheetCell* existing = known_cell ? known_cell : m_cells.find(cell_id);
        if (existing && !existing->dirty) {
            if (existing->has_error) {
                out_err = existing->error_msg;
                return {};
            }
            return existing->result;
        }
        const std::string* raw_ptr = known_raw ? known_raw : (existing && !existing->raw_expression.empty() ? &existing->raw_expression : m_raw_table.find(cell_id));
        if (!raw_ptr || raw_ptr->empty()) return {};

        const std::string& raw = *raw_ptr;
        bool has_subs_loaded = existing && (!existing->is_formula || existing->inputs.size() > 0 || existing->range_inputs.size() > 0 || !existing->postorder_rpn.empty());
        bool same_expr = (existing != nullptr && existing->raw_expression == raw && has_subs_loaded);

        visiting.push_back(cell_id);
        StringVec refs;
        RangeVec  ranges;
        std::string rpn, local_err;
        CellEvalValue eval_res = evaluateRawExpression(
            raw, visiting,
            same_expr ? nullptr : &refs,
            same_expr ? nullptr : &ranges,
            same_expr ? nullptr : &rpn,
            local_err);
        visiting.pop_back();

        if (!same_expr && existing && has_subs_loaded) {
            unsubscribeInputs(cell_id);
        }
        SpreadsheetCell& cell = existing ? *existing : m_cells[cell_id];
        if (!existing) {
            CellCoord cc = CellCoord::parse(cell_id);
            if (cc.valid) setGridPtr(cc.col, cc.row, &cell);
        }
        if (!same_expr) {
            cell.id = cell_id;
            cell.raw_expression = raw;
            cell.is_formula = (!raw.empty() && raw[0] == '=');
            cell.postorder_rpn = std::move(rpn);
            cell.inputs = std::move(refs);
            cell.range_inputs = std::move(ranges);
            for (size_t i = 0; i < cell.inputs.size(); ++i) {
                const std::string& ref_id = cell.inputs[i].getValue();
                if (ref_id != cell_id) m_dependents[ref_id].push_back(cell_id, 0);
            }
            for (size_t i = 0; i < cell.range_inputs.size(); ++i) {
                m_range_subs.push_back(RangeSubscription{cell_id, cell.range_inputs[i].getValue()}, 0);
            }
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
            if (SpreadsheetCell* c = m_cells.find(curr_id)) c->dirty = true;
            if (DepList* deps = m_dependents.find(curr_id)) {
                for (const auto& dep_node : *deps) {
                    const std::string& dep_id = dep_node.getValue();
                    if (!seen.contains(dep_id)) {
                        seen[dep_id] = true;
                        affected.push_back(dep_id);
                    }
                }
            }
            CellCoord cc = CellCoord::parse(curr_id);
            if (cc.valid) {
                for (size_t ri = 0; ri < m_range_subs.size(); ++ri) {
                    const RangeSubscription& sub = m_range_subs[ri].getValue();
                    if (sub.rect.contains(cc.col, cc.row) && !seen.contains(sub.subscriber_id)) {
                        seen[sub.subscriber_id] = true;
                        affected.push_back(sub.subscriber_id);
                    }
                }
            }
        }
        return affected;
    }

    void evalCellQuiet(const std::string& cid,
                       const std::string* known_raw = nullptr,
                       SpreadsheetCell* known_cell = nullptr) {
        StringVec visiting;
        std::string err;
        resolveCellValue(cid, visiting, err, known_raw, known_cell);
    }

    bool recomputeCell(const std::string& cid,
                       std::string* out_disp = nullptr,
                       const std::string* known_raw = nullptr) {
        SpreadsheetCell* cell_ptr = m_cells.find(cid);
        bool had_before = (cell_ptr != nullptr);
        bool prev_empty = true;
        bool prev_err = false;
        std::string prev_err_msg;
        CellEvalValue prev_res;
        if (had_before) {
            prev_empty   = cell_ptr->raw_expression.empty();
            prev_err     = cell_ptr->has_error;
            prev_err_msg = cell_ptr->error_msg;
            prev_res     = cell_ptr->result;
        }
        evalCellQuiet(cid, known_raw, cell_ptr);
        SpreadsheetCell* after_ptr = cell_ptr ? cell_ptr : m_cells.find(cid);
        if (!had_before && !after_ptr) return false;
        if (had_before && after_ptr) {
            bool now_empty = after_ptr->raw_expression.empty();
            bool same = (prev_empty && now_empty) ||
                        (!prev_empty && !now_empty &&
                         prev_err == after_ptr->has_error &&
                         (prev_err ? (prev_err_msg == after_ptr->error_msg)
                                   : (prev_res.is_text == after_ptr->result.is_text &&
                                      (prev_res.is_text ? (prev_res.text == after_ptr->result.text)
                                                        : (prev_res.num == after_ptr->result.num)))));
            if (same) return false;
        }
        std::string prev = (!had_before || prev_empty) ? "" : (prev_err ? prev_err_msg : prev_res.asString());
        std::string now  = after_ptr ? after_ptr->displayValue() : "";
        if (out_disp) *out_disp = now;
        return now != prev;
    }

    StringVec propagateAffected(const StringVec& affected) {
        StringVec updated;
        for (size_t qi = 1; qi < affected.size(); ++qi) {
            const std::string& curr_id = affected[qi].getValue();
            const std::string* raw_ptr = m_raw_table.find(curr_id);
            if (!raw_ptr || raw_ptr->empty()) continue;
            std::string now_disp;
            if (recomputeCell(curr_id, &now_disp, raw_ptr)) {
                updated.push_back(curr_id + "=" + now_disp);
            }
        }
        return updated;
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

    TX maxUsedCol() const { return m_max_used_col; }
    TX maxUsedRow() const { return m_max_used_row; }

    /** @brief Reconstruye el grafo y recalcula todas las celdas activas en dos pasadas (literales $O(1)$ + fórmulas). */
    StringVec recalculateAll(const std::string& = "") {
        m_dependents.clear();
        m_range_subs.clear();
        m_grid_index.clear();
        m_cells.clear();
        m_max_used_col = -1;
        m_max_used_row = -1;

        BaseVector<VectorAscTraits<SpreadsheetCell*>, Callable> formula_cells;

        // Pasada 1: indexar todas las celdas no vacías y resolver literales (números/textos) sin AST
        for (const auto& entry_node : m_raw_table) {
            const auto& kv = entry_node.getValue();
            const std::string& cid = kv.getKey();
            const std::string& raw = kv.getValue();
            if (raw.empty()) continue;

            CellCoord cc = CellCoord::parse(cid);
            std::string canon = cc.valid ? cc.toId() : cid;
            SpreadsheetCell& cell = m_cells[canon];
            cell.id = canon;
            cell.raw_expression = raw;
            cell.is_formula = (raw[0] == '=');
            if (cc.valid) setGridPtr(cc.col, cc.row, &cell);

            if (!cell.is_formula) {
                cell.result = evaluateLiteralFast(raw, &cell.postorder_rpn);
                cell.has_error = false;
                cell.dirty = false;
            } else {
                cell.dirty = true;
                formula_cells.push_back(&cell, 0);
            }
        }

        // Pasada 2: evaluar únicamente las celdas con fórmula usando lecturas O(1) sobre m_grid_index
        for (size_t i = 0; i < formula_cells.size(); ++i) {
            SpreadsheetCell* fc = formula_cells[i].getValue();
            if (fc && fc->dirty) {
                evalCellQuiet(fc->id, &fc->raw_expression, fc);
            }
        }
        return {};
    }

    /**
     * @brief Asigna un valor o fórmula a una celda (ej. `setCell("A1", "5")`, `setCell("B1", "= A1 + 4")`).
     */
    bool setCell(const std::string& cell_id_raw, const std::string& expression_raw, bool quiet_log = false) {
        CellCoord coord = CellCoord::parse(cell_id_raw);
        if (!coord.valid) {
            if (!quiet_log) addLog("✖ Coordenada inválida: " + cell_id_raw);
            return false;
        }
        std::string cid = coord.toId();
        std::string expr = texcel_detail::trim(expression_raw);

        unsubscribeInputs(cid);
        if (SpreadsheetCell* c = m_cells.find(cid)) c->raw_expression.clear();
        StringVec affected = collectAndMarkDirtySubgraph(cid);

        if (expr.empty()) {
            m_raw_table[cid] = "";
            SpreadsheetCell& cleared = m_cells[cid];
            cleared = SpreadsheetCell{.id = cid, .dirty = false};
            setGridPtr(coord.col, coord.row, &cleared);
            propagateAffected(affected);
            if (!quiet_log) addLog("⌫ " + cid + " limpiada");
            return true;
        }

        m_raw_table[cid] = expr;
        SpreadsheetCell& target = m_cells[cid];
        target.id = cid;
        target.raw_expression = expr;
        target.is_formula = (expr[0] == '=');
        target.dirty = true;
        setGridPtr(coord.col, coord.row, &target);

        evalCellQuiet(cid, &m_raw_table[cid], &target);
        StringVec cascaded = propagateAffected(affected);

        if (!quiet_log) {
            std::string log_expr = (expr[0] == '=') ? texcel_detail::trim(std::string_view(expr).substr(1)) : expr;
            std::ostringstream log_ss;
            log_ss << (target.has_error ? "⚠ " : "✔ ") << cid << " = " << log_expr << " ➔ " << target.displayValue();
            if (cascaded.size() > 0) {
                log_ss << " (↻ ";
                size_t show_max = std::min<size_t>(cascaded.size(), 6);
                for (size_t i = 0; i < show_max; ++i) {
                    if (i > 0) log_ss << ", ";
                    log_ss << cascaded[i].getValue();
                }
                if (cascaded.size() > show_max) {
                    log_ss << ", +" << (cascaded.size() - show_max);
                }
                log_ss << ")";
            }
            addLog(log_ss.str());
        }
        return !target.has_error;
    }

    /**
     * @brief Ejecuta un comando completo estilo `A1 = 5` o `B1 = A1 + 4` con cierre automático de paréntesis finales.
     */
    bool executeCommand(const std::string& cmd, const std::string& fallback_cell, std::string* out_assigned_cell = nullptr) {
        std::string s = FormulaParser::balanceTrailingParens(texcel_detail::trim(cmd));
        if (s.empty()) return false;

        ParsedAssignment pa = parseAssignment(s, fallback_cell);
        std::string target = pa.has_invalid_lhs ? fallback_cell : pa.target_cell;
        if (out_assigned_cell) *out_assigned_cell = target;
        return setCell(target, pa.has_invalid_lhs ? s : pa.expression);
    }

    /** @brief Consulta sin copias por coordenada 0-indexada (`nullptr` si está vacía). */
    const SpreadsheetCell* findCellAt(TX col, TX row) const {
        if (const SpreadsheetCell* p = fastGridPtr(col, row)) {
            return p->raw_expression.empty() ? nullptr : p;
        }
        return nullptr;
    }

    /** @brief Consulta sin copias por ID canónico (`"A1"`), retornando `nullptr` si no existe. */
    const SpreadsheetCell* findCell(const std::string& cell_id) {
        CellCoord c = CellCoord::parse(cell_id);
        if (c.valid) {
            if (SpreadsheetCell* p = fastGridPtr(c.col, c.row)) return p;
            std::string cid = c.toId();
            if (SpreadsheetCell* found = m_cells.find(cid)) {
                setGridPtr(c.col, c.row, found);
                return found;
            }
            return nullptr;
        }
        return m_cells.find(cell_id);
    }

    /** @brief Consulta el estado de una celda por valor (compatibilidad con tests y llamadas puntuales). */
    SpreadsheetCell getCell(const std::string& cell_id) {
        if (const SpreadsheetCell* found = findCell(cell_id)) return *found;
        CellCoord c = CellCoord::parse(cell_id);
        return SpreadsheetCell{.id = c.valid ? c.toId() : cell_id};
    }

    /** @brief Retorna el texto crudo de una celda (`""` si está vacía) sin copiar vectores de entradas. */
    std::string getRawExpression(const std::string& cell_id) {
        if (const SpreadsheetCell* found = findCell(cell_id)) return found->raw_expression;
        if (const std::string* raw = m_raw_table.find(cell_id)) return *raw;
        return "";
    }

    /** @brief Obtiene las entradas directas de `cell_id` (expande rangos pequeños `<= 64` celdas para compatibilidad). */
    StringVec getCellInputs(const std::string& cell_id) {
        const SpreadsheetCell* found = findCell(cell_id);
        if (!found) return {};
        StringVec out = found->inputs;
        for (size_t i = 0; i < found->range_inputs.size(); ++i) {
            const CellRangeRect& r = found->range_inputs[i].getValue();
            if (r.cellCount() <= 64) {
                for (TX row = r.r0; row <= r.r1; ++row) {
                    for (TX col = r.c0; col <= r.c1; ++col) {
                        texcel_detail::addUniqueStr(out, CellCoord(col, row, true).toId());
                    }
                }
            }
        }
        return out;
    }

    /** @brief Obtiene la lista de celdas que dependen de `cell_id` (observadores puntuales + rangos que la contienen). */
    StringVec getCellDependents(const std::string& cell_id) {
        StringVec res;
        CellCoord cc = CellCoord::parse(cell_id);
        std::string cid = cc.valid ? cc.toId() : cell_id;
        if (const DepList* deps = m_dependents.find(cid)) {
            for (const auto& node : *deps) {
                texcel_detail::addUniqueStr(res, node.getValue());
            }
        }
        if (cc.valid) {
            for (size_t i = 0; i < m_range_subs.size(); ++i) {
                const RangeSubscription& sub = m_range_subs[i].getValue();
                if (sub.rect.contains(cc.col, cc.row) && sub.subscriber_id != cid) {
                    texcel_detail::addUniqueStr(res, sub.subscriber_id);
                }
            }
        }
        return res;
    }

    /** @brief Indica si una cadena en mayúsculas corresponde a un comando administrativo de la consola. */
    static bool isAdminCommand(const std::string& upper_cmd) {
        return upper_cmd == "CLEAR" || upper_cmd == "QUIT" || upper_cmd == "EXIT" ||
               upper_cmd == "Q" || upper_cmd == "SAVE" || upper_cmd == "LOAD" ||
               upper_cmd == "UNDO" || upper_cmd == "REDO" || upper_cmd == "AUTOFIT";
    }

    /**
     * @brief Evalúa en vivo una entrada parcial de consola sin mutar la hoja de cálculo.
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
            p.expression, visiting, &p.referenced_cells, &p.referenced_ranges, &rpn, err, &syntax_msg);

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
        texcel_detail::forEachCoordInRange(CellCoord(c0, r0, true), CellCoord(c1, r1, true), [&](TX col, TX row) {
            const SpreadsheetCell* p = findCellAt(col, row);
            if (p) {
                setCell(p->id, "", true);
            } else {
                std::string cid = CellCoord(col, row, true).toId();
                if (hasRawCell(cid)) setCell(cid, "", true);
            }
        });
        addLog("⌫ Rango " + CellRangeRect(c0, r0, c1, r1).toId() + " limpiado");
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
            if (ch == '\\' && in_quotes && i + 1 < expr.size()) {
                out.push_back(ch);
                out.push_back(expr[i + 1]);
                i += 2;
                continue;
            }
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
                    setCell(dst_id, shiftFormulaRefs(src_raw, dcol, drow), true);
                } else if (is_series) {
                    TX step_idx = fill_vertical ? drow : dcol;
                    setCell(dst_id, formatCellNumber(series_base + step_idx * series_step), true);
                } else {
                    setCell(dst_id, src_raw, true);
                }
                ++filled;
            }
        }
        addLog("✔ Auto-Fill (" + std::to_string(filled) + " celdas) desde " + src_id);
        return filled;
    }

    /** @brief Limpia toda la hoja de cálculo y los índices reactivos. */
    void clear() {
        m_raw_table.clear();
        m_cells.clear();
        m_dependents.clear();
        m_range_subs.clear();
        m_grid_index.clear();
        m_max_used_col = -1;
        m_max_used_row = -1;
        addLog("Hoja de cálculo reiniciada");
    }
};

#endif // __TEXCEL_ENGINE_H__
