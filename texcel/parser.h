/**
 * @file parser.h
 * @ingroup texcel
 * @brief Parser de fórmulas por Precedence Climbing hacia `BinaryTreeNode<AstToken>`.
 */

#ifndef __TEXCEL_PARSER_H__
#define __TEXCEL_PARSER_H__

#include "types.h"
#include <cstring>
#include <memory>

/**
 * @class FormulaParser
 * @brief Parser compacto por Precedence Climbing con gestión RAII y árbol binario uniforme de argumentos.
 */
class FormulaParser {
    struct AstDeleter {
        void operator()(texcel_detail::AstNode* n) const { texcel_detail::freeAst(n); }
    };
    using AstPtr = std::unique_ptr<texcel_detail::AstNode, AstDeleter>;

    std::string m_src;
    size_t      m_pos = 0;
    texcel_detail::StringVec& m_refs;

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

    void registerRangeRefs(const CellCoord& a, const CellCoord& b) {
        texcel_detail::forEachCellInRange(a, b, [&](const std::string& cid) {
            texcel_detail::addUniqueStr(m_refs, cid);
        });
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
    FormulaParser(const std::string& src, texcel_detail::StringVec& refs) : m_src(src), m_refs(refs) {}

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
    // Precedence Climbing sobre BINARY_OPS
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

    // Unary := ('+' | '-') Unary | Primary
    AstPtr parseUnary() {
        char c = peek();
        if (c == '+') { get(); return parseUnary(); }
        if (c == '-') {
            get();
            AstToken zero{AstNodeKind::Number, "0", 0.0};
            AstToken neg{AstNodeKind::BinaryOp, "-", 0.0, '-'};
            return makeNode(neg, makeNode(zero), parseUnary());
        }
        return parsePrimary();
    }

    // Todas las funciones usan la misma espina binaria: FuncCall(arg0, ArgPair(arg1, ...))
    AstPtr parseFunctionCall(const std::string& ident) {
        bool known = (ident == "SUM" || ident == "AVG" || ident == "MIN" || ident == "MAX" ||
                      ident == "CONCAT" || ident == "IF" || ident == "COUNTIF" || ident == "COUNTIFS");
        if (!known) throw std::runtime_error("Función desconocida: " + ident);

        Vector<VectorAscTraits<texcel_detail::AstNode*>> raw_args;
        try {
            if (peek() != ')') {
                while (true) {
                    raw_args.push_back(parseBinary(1).release());
                    char sep = peek();
                    if (sep == ',') { get(); continue; }
                    if (sep == ')') { get(); break; }
                    throw std::runtime_error("Se esperaba ',' o ')' en " + ident);
                }
            } else {
                get();
            }
        } catch (...) {
            for (size_t i = 0; i < raw_args.size(); ++i) texcel_detail::freeAst(raw_args[i].getValue());
            throw;
        }

        size_t argc = raw_args.size();
        auto failArity = [&](const std::string& msg) {
            for (size_t i = 0; i < argc; ++i) texcel_detail::freeAst(raw_args[i].getValue());
            throw std::runtime_error(msg);
        };
        if (argc == 0) failArity(ident + " requiere al menos 1 argumento");
        if (ident == "IF" && argc != 3) failArity("IF requiere exactamente 3 argumentos");
        if (ident == "COUNTIF" && argc != 2) failArity("COUNTIF requiere exactamente 2 argumentos");
        if (ident == "COUNTIFS" && (argc < 2 || argc % 2 != 0)) failArity("COUNTIFS requiere pares (rango, criterio)");

        AstPtr right_spine = nullptr;
        if (argc >= 2) {
            right_spine = AstPtr(raw_args[argc - 1].getValue());
            for (size_t i = argc - 1; i > 1; --i) {
                AstToken pair_tok;
                pair_tok.kind = AstNodeKind::ArgPair;
                pair_tok.text = ",";
                right_spine = makeNode(pair_tok, AstPtr(raw_args[i - 1].getValue()), std::move(right_spine));
            }
        }

        AstToken fn_tok;
        fn_tok.kind = AstNodeKind::FuncCall;
        fn_tok.func_name = ident;
        fn_tok.text = ident;
        return makeNode(fn_tok, AstPtr(raw_args[0].getValue()), std::move(right_spine));
    }

    // Primary := '(' Binary ')' | '"' String '"' | Number | Func(...) | TRUE | FALSE | CellRef | CellRange
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
            tok.func_name = str_val;
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
            tok.number = val;
            tok.text = SpreadsheetCell::formatNumber(val);
            return makeNode(tok);
        }

        if (texcel_detail::isChar<std::isalpha>(c) || c == '$') {
            std::string ident = readIdent();
            if (peek() == '(') {
                get();
                return parseFunctionCall(ident);
            }
            if (ident == "TRUE" || ident == "FALSE") {
                AstToken tok;
                tok.kind = AstNodeKind::StringLiteral;
                tok.func_name = (ident == "TRUE") ? "True" : "False";
                tok.text = "\"" + tok.func_name + "\"";
                return makeNode(tok);
            }

            CellCoord a = CellCoord::parse(ident);
            if (!a.valid) throw std::runtime_error("Celda inválida: " + ident);

            // Rango A1:B2 reconocido en un único punto
            if (peek() == ':') {
                get();
                CellCoord b = CellCoord::parse(readIdent());
                if (!b.valid) throw std::runtime_error("Rango inválido");
                registerRangeRefs(a, b);
                AstToken tok;
                tok.kind = AstNodeKind::CellRange;
                tok.range_start = a.toId();
                tok.range_end = b.toId();
                tok.text = tok.range_start + ":" + tok.range_end;
                return makeNode(tok);
            }

            std::string cid = a.toId();
            texcel_detail::addUniqueStr(m_refs, cid);
            AstToken tok;
            tok.kind = AstNodeKind::CellRef;
            tok.text = cid;
            return makeNode(tok);
        }

        throw std::runtime_error("Carácter inesperado");
    }
};

#endif // __TEXCEL_PARSER_H__
