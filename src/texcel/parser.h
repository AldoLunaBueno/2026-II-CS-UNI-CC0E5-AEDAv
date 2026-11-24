/**
 * @file parser.h
 * @ingroup texcel
 * @brief Parser de fórmulas por Precedence Climbing hacia `BinaryTreeNode<AstToken>` con referencias de rango compactas.
 */

#ifndef __TEXCEL_PARSER_H__
#define __TEXCEL_PARSER_H__

#include "types.h"
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

/**
 * @class FormulaParser
 * @brief Parser compacto por Precedence Climbing con gestión RAII, catálogo unificado y rangos `O(1)`.
 *
 * @image html texcel_precedence_climbing.svg "Ascenso de precedencia (Precedence Climbing) en FormulaParser::parseBinary"
 */
class FormulaParser {
    struct AstDeleter {
        void operator()(texcel_detail::AstNode* n) const { texcel_detail::freeAst(n); }
    };
    using AstPtr = std::unique_ptr<texcel_detail::AstNode, AstDeleter>;

    std::string               m_src;
    size_t                    m_pos = 0;
    texcel_detail::StringVec* m_refs = nullptr;
    texcel_detail::RangeVec*  m_ranges = nullptr;
    texcel_detail::StringSet  m_seen_refs;

    void addRef(const std::string& cid) {
        if (!m_refs) return;
        if (!m_seen_refs.contains(cid)) {
            m_seen_refs[cid] = true;
            m_refs->push_back(cid);
        }
    }

    void addRangeRef(const CellCoord& a, const CellCoord& b) {
        if (m_ranges) {
            texcel_detail::addUniqueRange(*m_ranges, CellRangeRect(a, b));
        } else if (m_refs) {
            texcel_detail::forEachCellInRange(a, b, [&](const std::string& cid) {
                addRef(cid);
            });
        }
    }

    static bool isIdentChar(char ch) {
        return texcel_detail::isChar<std::isalnum>(ch) || ch == '$';
    }

    static AstPtr makeNode(const AstToken& tok, AstPtr left = nullptr, AstPtr right = nullptr) {
        return AstPtr(texcel_detail::makeBinaryNode(tok, left.release(), right.release()));
    }

    void skipSpaces() {
        while (m_pos < m_src.size() && texcel_detail::isChar<std::isspace>(m_src[m_pos]))
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

    std::string readIdent() {
        skipSpaces();
        size_t start = m_pos;
        while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
        return texcel_detail::toUpper(m_src.substr(start, m_pos - start));
    }

    bool peekBinaryOp(texcel_detail::OpInfo& out, size_t& op_len) {
        skipSpaces();
        if (m_pos >= m_src.size()) return false;
        for (const auto& op : texcel_detail::BINARY_OPS) {
            size_t len = std::strlen(op.lexeme);
            if (m_src.compare(m_pos, len, op.lexeme) == 0) {
                out = op;
                op_len = len;
                return true;
            }
        }
        return false;
    }

public:
    FormulaParser(const std::string& src,
                  texcel_detail::StringVec* refs = nullptr,
                  texcel_detail::RangeVec* ranges = nullptr)
        : m_src(src), m_refs(refs), m_ranges(ranges) {}

    FormulaParser(const std::string& src, texcel_detail::StringVec& refs)
        : m_src(src), m_refs(&refs), m_ranges(nullptr) {}

    /**
     * @brief Completa automáticamente paréntesis de cierre faltantes al final de una fórmula (`=SUM(A1:A5` -> `=SUM(A1:A5)`).
     */
    static std::string balanceTrailingParens(const std::string& src) {
        std::string trimmed = texcel_detail::trim(src);
        if (trimmed.empty() || trimmed[0] == '\'') return src;
        bool is_formula = (trimmed[0] == '=');
        if (!is_formula) {
            size_t eq = trimmed.find('=');
            if (eq == std::string::npos || !CellCoord::parse(trimmed.substr(0, eq)).valid) return src;
        }
        bool in_quotes = false;
        int depth = 0;
        for (size_t i = 0; i < src.size(); ++i) {
            char c = src[i];
            if (c == '\\' && in_quotes && i + 1 < src.size()) { ++i; continue; }
            if (c == '"') { in_quotes = !in_quotes; continue; }
            if (!in_quotes) {
                if (c == '(') ++depth;
                else if (c == ')' && depth > 0) --depth;
            }
        }
        if (depth > 0 && !in_quotes) {
            return src + std::string(static_cast<size_t>(depth), ')');
        }
        return src;
    }

    /**
     * @brief Detecta la función activa y el índice de argumento (0-based) en la posición `cursor_pos` para *calltips* TUI.
     */
    static const TexcelFuncInfo* detectActiveCalltip(std::string_view buf, size_t cursor_pos, size_t& out_arg_idx) {
        out_arg_idx = 0;
        size_t lim = std::min(cursor_pos, buf.size());
        struct CallFrame { std::string fn_name; size_t arg_idx = 0; };
        std::vector<CallFrame> stack;
        bool in_quotes = false;

        for (size_t i = 0; i < lim; ++i) {
            char c = buf[i];
            if (c == '\\' && in_quotes && i + 1 < lim) { ++i; continue; }
            if (c == '"') { in_quotes = !in_quotes; continue; }
            if (in_quotes) continue;

            if (c == '(') {
                size_t end_id = i;
                while (end_id > 0 && texcel_detail::isChar<std::isspace>(buf[end_id - 1])) --end_id;
                size_t start_id = end_id;
                while (start_id > 0 && isIdentChar(buf[start_id - 1])) --start_id;
                std::string ident = texcel_detail::toUpper(std::string(buf.substr(start_id, end_id - start_id)));
                stack.push_back(CallFrame{std::move(ident), 0});
            } else if (c == ')') {
                if (!stack.empty()) stack.pop_back();
            } else if (c == ',' && !stack.empty()) {
                ++stack.back().arg_idx;
            }
        }

        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (!it->fn_name.empty()) {
                if (const TexcelFuncInfo* info = findTexcelFunc(it->fn_name)) {
                    out_arg_idx = it->arg_idx;
                    return info;
                }
            }
        }
        return nullptr;
    }

    texcel_detail::AstNode* parse() {
        skipSpaces();
        if (m_pos < m_src.size() && m_src[m_pos] == '=') ++m_pos;
        skipSpaces();
        if (m_pos >= m_src.size()) throw std::runtime_error("Expresión vacía");
        AstPtr root = parseBinary(1);
        skipSpaces();
        if (m_pos < m_src.size()) throw std::runtime_error("Token inesperado");
        return root.release();
    }

private:
    AstPtr parseBinary(TX min_prec) {
        AstPtr left = parseUnary();
        while (true) {
            texcel_detail::OpInfo op{};
            size_t op_len = 0;
            if (!peekBinaryOp(op, op_len) || op.prec < min_prec) break;
            m_pos += op_len;

            AstPtr right = parseBinary(op.right_assoc ? op.prec : op.prec + 1);
            AstToken tok;
            tok.kind = AstNodeKind::BinaryOp;
            tok.op   = op.code;
            tok.text = (op.code == '!') ? "<>" : op.lexeme;
            left = makeNode(tok, std::move(left), std::move(right));
        }
        return left;
    }

    AstPtr parseUnary() {
        char c = peek();
        if (c == '+') { get(); return parseUnary(); }
        if (c == '-') {
            get();
            AstToken zero{AstNodeKind::Number, "0"};
            AstToken neg{AstNodeKind::BinaryOp, "-", '-'};
            return makeNode(neg, makeNode(zero), parseUnary());
        }
        return parsePrimary();
    }

    AstPtr parseFunctionCall(const std::string& ident) {
        const TexcelFuncInfo* spec = findTexcelFunc(ident);
        if (!spec) throw std::runtime_error("Función desconocida: " + ident);

        std::vector<AstPtr> args;
        if (peek() != ')') {
            while (true) {
                args.push_back(parseBinary(1));
                char sep = peek();
                if (sep == ',') { get(); continue; }
                if (sep == ')') { get(); break; }
                throw std::runtime_error("Se esperaba ',' o ')' en " + ident);
            }
        } else {
            get();
        }

        size_t argc = args.size();
        if (argc < spec->min_args || (spec->max_args > 0 && argc > spec->max_args)) {
            throw std::runtime_error(ident + ": número inválido de argumentos");
        }
        if (spec->even_args && (argc % 2 != 0)) {
            throw std::runtime_error(ident + " requiere pares (rango, criterio)");
        }

        AstPtr right_spine = nullptr;
        if (argc >= 2) {
            right_spine = std::move(args.back());
            for (size_t i = argc - 1; i > 1; --i) {
                AstToken pair_tok{AstNodeKind::ArgPair, ","};
                right_spine = makeNode(pair_tok, std::move(args[i - 1]), std::move(right_spine));
            }
        }

        AstToken fn_tok;
        fn_tok.kind = AstNodeKind::FuncCall;
        fn_tok.func_id = spec->id;
        fn_tok.text = ident;
        return makeNode(fn_tok, std::move(args[0]), std::move(right_spine));
    }

    AstPtr parsePrimary() {
        char c = peek();
        if (c == '(') {
            get();
            AstPtr inside = parseBinary(1);
            if (peek() != ')') throw std::runtime_error("Falta ')'");
            get();
            return inside;
        }

        if (c == '"') {
            ++m_pos;
            std::string str_val;
            bool closed = false;
            while (m_pos < m_src.size()) {
                char ch = m_src[m_pos++];
                if (ch == '\\' && m_pos < m_src.size()) str_val.push_back(m_src[m_pos++]);
                else if (ch == '"') { closed = true; break; }
                else str_val.push_back(ch);
            }
            if (!closed) throw std::runtime_error("Falta '\"' de cierre");
            AstToken tok;
            tok.kind = AstNodeKind::StringLiteral;
            tok.text = "\"" + str_val + "\"";
            tok.evaluated = CellEvalValue{true, 0.0, str_val, ""};
            return makeNode(tok);
        }

        if (texcel_detail::isChar<std::isdigit>(c) || c == '.') {
            size_t start = m_pos;
            bool has_dot = false;
            while (m_pos < m_src.size()) {
                char ch = m_src[m_pos];
                if (texcel_detail::isChar<std::isdigit>(ch)) ++m_pos;
                else if (ch == '.' && !has_dot) { has_dot = true; ++m_pos; }
                else break;
            }
            double val = std::stod(m_src.substr(start, m_pos - start));
            AstToken tok;
            tok.kind = AstNodeKind::Number;
            tok.text = formatCellNumber(val);
            tok.evaluated = CellEvalValue{false, val, "", ""};
            return makeNode(tok);
        }

        if (texcel_detail::isChar<std::isalpha>(c) || c == '$') {
            std::string ident = readIdent();
            if (peek() == '(') {
                get();
                return parseFunctionCall(ident);
            }
            if (ident == "TRUE" || ident == "FALSE") {
                std::string bval = (ident == "TRUE") ? "True" : "False";
                AstToken tok;
                tok.kind = AstNodeKind::StringLiteral;
                tok.text = "\"" + bval + "\"";
                tok.evaluated = CellEvalValue{true, 0.0, bval, ""};
                return makeNode(tok);
            }

            CellCoord a = CellCoord::parse(ident);
            if (!a.valid) throw std::runtime_error("Celda inválida: " + ident);

            if (peek() == ':') {
                get();
                CellCoord b = CellCoord::parse(readIdent());
                if (!b.valid) throw std::runtime_error("Rango inválido");
                addRangeRef(a, b);
                AstToken tok;
                tok.kind = AstNodeKind::CellRange;
                tok.range_a = a;
                tok.range_b = b;
                tok.text = a.toId() + ":" + b.toId();
                return makeNode(tok);
            }

            std::string cid = a.toId();
            addRef(cid);
            AstToken tok;
            tok.kind = AstNodeKind::CellRef;
            tok.text = cid;
            return makeNode(tok);
        }

        throw std::runtime_error("Carácter inesperado");
    }
};

#endif // __TEXCEL_PARSER_H__
