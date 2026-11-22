/**
 * @file parser.h
 * @brief Parser de descenso recursivo de fórmulas hacia `BinaryTreeNode<AstToken>`.
 */

#ifndef __TEXCEL_PARSER_H__
#define __TEXCEL_PARSER_H__

#include "types.h"

/**
 * @class FormulaParser
 * @brief Parser de descenso recursivo que construye un árbol `BinaryTreeNode<AstToken>`.
 */
class FormulaParser {
    std::string m_src;
    size_t      m_pos = 0;
    texcel_detail::StringVec&  m_refs;

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
    FormulaParser(const std::string& src, texcel_detail::StringVec& refs) : m_src(src), m_refs(refs) {}

    texcel_detail::AstNode* parse() {
        skipSpaces();
        if (m_pos < m_src.size() && m_src[m_pos] == '=') ++m_pos;
        skipSpaces();
        if (m_pos >= m_src.size()) throw std::runtime_error("Expresión vacía");
        texcel_detail::AstNode* root = parseComparison();
        skipSpaces();
        if (m_pos < m_src.size()) {
            texcel_detail::freeAst(root);
            throw std::runtime_error("Token inesperado");
        }
        return root;
    }

private:
    // Comparison := Concat { ('=' | '<>' | '!=' | '<=' | '>=' | '<' | '>') Concat }
    texcel_detail::AstNode* parseComparison() {
        texcel_detail::AstNode* left = parseConcat();
        while (true) {
            skipSpaces();
            if (m_pos >= m_src.size()) break;
            std::string op_str;
            char op_code = 0;
            if (m_src.compare(m_pos, 2, "<>") == 0 || m_src.compare(m_pos, 2, "!=") == 0) {
                op_str = "<>"; op_code = '!'; m_pos += 2;
            } else if (m_src.compare(m_pos, 2, "<=") == 0) {
                op_str = "<="; op_code = 'L'; m_pos += 2;
            } else if (m_src.compare(m_pos, 2, ">=") == 0) {
                op_str = ">="; op_code = 'G'; m_pos += 2;
            } else if (m_src[m_pos] == '=' || m_src[m_pos] == '<' || m_src[m_pos] == '>') {
                op_code = m_src[m_pos++];
                op_str = std::string(1, op_code);
            } else {
                break;
            }

            texcel_detail::AstNode* right = nullptr;
            try {
                right = parseConcat();
            } catch (...) {
                texcel_detail::freeAst(left);
                throw;
            }
            AstToken tok;
            tok.kind = AstNodeKind::BinaryOp;
            tok.op = op_code;
            tok.text = op_str;
            left = texcel_detail::makeBinaryNode(tok, left, right);
        }
        return left;
    }

    // Concat := Expr { '&' Expr }
    texcel_detail::AstNode* parseConcat() {
        texcel_detail::AstNode* left = parseExpr();
        while (peek() == '&') {
            get();
            texcel_detail::AstNode* right = nullptr;
            try {
                right = parseExpr();
            } catch (...) {
                texcel_detail::freeAst(left);
                throw;
            }
            AstToken tok;
            tok.kind = AstNodeKind::BinaryOp;
            tok.op = '&';
            tok.text = "&";
            left = texcel_detail::makeBinaryNode(tok, left, right);
        }
        return left;
    }

    // Expr := Term { ('+' | '-') Term }
    texcel_detail::AstNode* parseExpr() {
        texcel_detail::AstNode* left = parseTerm();
        while (true) {
            char c = peek();
            if (c == '+' || c == '-') {
                get();
                texcel_detail::AstNode* right = nullptr;
                try {
                    right = parseTerm();
                } catch (...) {
                    texcel_detail::freeAst(left);
                    throw;
                }
                AstToken tok;
                tok.kind = AstNodeKind::BinaryOp;
                tok.op = c;
                tok.text = std::string(1, c);
                left = texcel_detail::makeBinaryNode(tok, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // Term := Power { ('*' | '/') Power }
    texcel_detail::AstNode* parseTerm() {
        texcel_detail::AstNode* left = parsePower();
        while (true) {
            char c = peek();
            if (c == '*' || c == '/') {
                get();
                texcel_detail::AstNode* right = nullptr;
                try {
                    right = parsePower();
                } catch (...) {
                    texcel_detail::freeAst(left);
                    throw;
                }
                AstToken tok;
                tok.kind = AstNodeKind::BinaryOp;
                tok.op = c;
                tok.text = std::string(1, c);
                left = texcel_detail::makeBinaryNode(tok, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // Power := Unary [ '^' Power ]
    texcel_detail::AstNode* parsePower() {
        texcel_detail::AstNode* left = parseUnary();
        if (peek() == '^') {
            get();
            texcel_detail::AstNode* right = nullptr;
            try {
                right = parsePower();
            } catch (...) {
                texcel_detail::freeAst(left);
                throw;
            }
            AstToken tok;
            tok.kind = AstNodeKind::BinaryOp;
            tok.op = '^';
            tok.text = "^";
            left = texcel_detail::makeBinaryNode(tok, left, right);
        }
        return left;
    }

    // Unary := ('+' | '-') Unary | Primary
    texcel_detail::AstNode* parseUnary() {
        char c = peek();
        if (c == '+') {
            get();
            return parseUnary();
        }
        if (c == '-') {
            get();
            texcel_detail::AstNode* operand = parseUnary();
            AstToken zero_tok;
            zero_tok.kind = AstNodeKind::Number;
            zero_tok.number = 0.0;
            zero_tok.text = "0";
            texcel_detail::AstNode* zero_node = new texcel_detail::AstNode(zero_tok, 0);

            AstToken op_tok;
            op_tok.kind = AstNodeKind::BinaryOp;
            op_tok.op = '-';
            op_tok.text = "-";
            return texcel_detail::makeBinaryNode(op_tok, zero_node, operand);
        }
        return parsePrimary();
    }

    // Primary := '(' Concat ')' | '"' String '"' | Number | Function(...) | CellRef
    texcel_detail::AstNode* parsePrimary() {
        skipSpaces();
        char c = peek();
        if (c == '(') {
            get();
            texcel_detail::AstNode* inside = parseComparison();
            if (peek() != ')') {
                texcel_detail::freeAst(inside);
                throw std::runtime_error("Falta ')'");
            }
            get();
            return inside;
        }

        // Literal de texto entre comillas dobles ("...")
        if (c == '"') {
            ++m_pos;
            std::string str_val;
            bool closed = false;
            while (m_pos < m_src.size()) {
                char ch = m_src[m_pos++];
                if (ch == '\\' && m_pos < m_src.size()) {
                    str_val.push_back(m_src[m_pos++]);
                } else if (ch == '"') {
                    closed = true;
                    break;
                } else {
                    str_val.push_back(ch);
                }
            }
            if (!closed) throw std::runtime_error("Falta '\"' de cierre");
            AstToken tok;
            tok.kind = AstNodeKind::StringLiteral;
            tok.text = "\"" + str_val + "\"";
            tok.func_name = str_val; // reutiliza string para almacenar el literal sin comillas
            return new texcel_detail::AstNode(tok, 0);
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
            return new texcel_detail::AstNode(tok, 0);
        }

        // Identificador: Celda (A1, $C$1, B1, D4) o Función (SUM(A1:B3), AVG, MIN, MAX)
        auto isIdentChar = [](char ch) {
            return std::isalnum(static_cast<unsigned char>(ch)) || ch == '$';
        };
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '$') {
            size_t start = m_pos;
            while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) {
                ++m_pos;
            }
            std::string ident = texcel_detail::toUpper(m_src.substr(start, m_pos - start));

            if (peek() == '(') {
                get(); // consume '('
                skipSpaces();

                // Función de concatenación: CONCAT(arg1, arg2, ...) o CONCAT(A1:B2)
                if (ident == "CONCAT") {
                    texcel_detail::AstNode* acc_node = nullptr;
                    auto appendNode = [&](texcel_detail::AstNode* next) {
                        if (!acc_node) {
                            acc_node = next;
                        } else {
                            AstToken amp;
                            amp.kind = AstNodeKind::BinaryOp;
                            amp.op = '&';
                            amp.text = "&";
                            acc_node = texcel_detail::makeBinaryNode(amp, acc_node, next);
                        }
                    };

                    while (true) {
                        skipSpaces();
                        if (peek() == ')') {
                            get();
                            break;
                        }
                        // Detectar si el argumento es un rango A1:B2
                        size_t saved_pos = m_pos;
                        bool parsed_range = false;
                        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '$') {
                            size_t p1 = m_pos;
                            while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                            std::string s1 = texcel_detail::toUpper(m_src.substr(p1, m_pos - p1));
                            if (peek() == ':') {
                                get();
                                skipSpaces();
                                size_t p2 = m_pos;
                                while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                                std::string s2 = texcel_detail::toUpper(m_src.substr(p2, m_pos - p2));
                                CellCoord a = CellCoord::parse(s1);
                                CellCoord b = CellCoord::parse(s2);
                                if (!a.valid || !b.valid) {
                                    texcel_detail::freeAst(acc_node);
                                    throw std::runtime_error("Rango inválido en CONCAT");
                                }
                                TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                                TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                                for (TX r = r_min; r <= r_max; ++r) {
                                    for (TX col = c_min; col <= c_max; ++col) {
                                        std::string cid = CellCoord(col, r, true).toId();
                                        texcel_detail::addUniqueStr(m_refs, cid);
                                        AstToken rtok;
                                        rtok.kind = AstNodeKind::CellRef;
                                        rtok.text = cid;
                                        appendNode(new texcel_detail::AstNode(rtok, 0));
                                    }
                                }
                                parsed_range = true;
                            } else {
                                m_pos = saved_pos;
                            }
                        }
                        if (!parsed_range) {
                            texcel_detail::AstNode* arg = nullptr;
                            try {
                                arg = parseConcat();
                            } catch (...) {
                                texcel_detail::freeAst(acc_node);
                                throw;
                            }
                            appendNode(arg);
                        }

                        skipSpaces();
                        char sep = peek();
                        if (sep == ',') {
                            get();
                        } else if (sep == ')') {
                            get();
                            break;
                        } else {
                            texcel_detail::freeAst(acc_node);
                            throw std::runtime_error("Se esperaba ',' o ')' en CONCAT");
                        }
                    }
                    if (!acc_node) throw std::runtime_error("CONCAT requiere al menos 1 argumento");
                    return acc_node;
                }

                // Función de conteo condicional: COUNTIF(rango, criterio) / COUNTIFS(r1, c1, r2, c2, ...)
                if (ident == "COUNTIF" || ident == "COUNTIFS") {
                    AstToken tok;
                    tok.kind = AstNodeKind::FuncCountIfs;
                    tok.func_name = ident;
                    tok.text = ident + "(...)";

                    while (true) {
                        skipSpaces();
                        size_t r1_start = m_pos;
                        while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                        std::string c1_str = texcel_detail::toUpper(m_src.substr(r1_start, m_pos - r1_start));
                        std::string c2_str = c1_str;
                        if (peek() == ':') {
                            get();
                            skipSpaces();
                            size_t r2_start = m_pos;
                            while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                            c2_str = texcel_detail::toUpper(m_src.substr(r2_start, m_pos - r2_start));
                        }
                        CellCoord a = CellCoord::parse(c1_str);
                        CellCoord b = CellCoord::parse(c2_str);
                        if (!a.valid || !b.valid) throw std::runtime_error("Rango inválido en " + ident);

                        TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                        TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                        for (TX r = r_min; r <= r_max; ++r) {
                            for (TX col = c_min; col <= c_max; ++col) {
                                texcel_detail::addUniqueStr(m_refs, CellCoord(col, r, true).toId());
                            }
                        }

                        if (peek() != ',') throw std::runtime_error("Se esperaba ',' tras rango en " + ident);
                        get(); // consume ','
                        skipSpaces();

                        size_t crit_start = m_pos;
                        texcel_detail::AstNode* crit_ast = parseConcat();
                        size_t crit_end = m_pos;
                        texcel_detail::freeAst(crit_ast);

                        std::string crit_expr = texcel_detail::trim(m_src.substr(crit_start, crit_end - crit_start));
                        tok.cond_pairs.push_back(RangeCriteriaPair{
                            CellCoord(c_min, r_min, true).toId(),
                            CellCoord(c_max, r_max, true).toId(),
                            crit_expr
                        });

                        skipSpaces();
                        char sep = peek();
                        if (sep == ',') {
                            get();
                            continue;
                        } else if (sep == ')') {
                            get();
                            break;
                        } else {
                            throw std::runtime_error("Se esperaba ',' o ')' en " + ident);
                        }
                    }

                    if (ident == "COUNTIF" && tok.cond_pairs.size() != 1) {
                        throw std::runtime_error("COUNTIF acepta exactamente 1 par (rango, criterio)");
                    }
                    if (tok.cond_pairs.size() == 0) {
                        throw std::runtime_error(ident + " requiere al menos 1 condición");
                    }
                    return new texcel_detail::AstNode(tok, 0);
                }

                if (ident == "IF") {
                    texcel_detail::AstNode* cond_node = nullptr;
                    texcel_detail::AstNode* true_node = nullptr;
                    texcel_detail::AstNode* false_node = nullptr;
                    try {
                        cond_node = parseComparison();
                        if (peek() != ',') throw std::runtime_error("Se esperaba ',' tras condición en IF");
                        get();
                        true_node = parseComparison();
                        if (peek() != ',') throw std::runtime_error("Se esperaba ',' tras rama verdadera en IF");
                        get();
                        false_node = parseComparison();
                        if (peek() != ')') throw std::runtime_error("Falta ')' en IF");
                        get();
                    } catch (...) {
                        texcel_detail::freeAst(cond_node);
                        texcel_detail::freeAst(true_node);
                        texcel_detail::freeAst(false_node);
                        throw;
                    }
                    AstToken br_tok;
                    br_tok.kind = AstNodeKind::IfBranches;
                    br_tok.text = "?:";
                    texcel_detail::AstNode* br_node = texcel_detail::makeBinaryNode(br_tok, true_node, false_node);

                    AstToken if_tok;
                    if_tok.kind = AstNodeKind::FuncIf;
                    if_tok.func_name = "IF";
                    if_tok.text = "IF";
                    return texcel_detail::makeBinaryNode(if_tok, cond_node, br_node);
                }

                // Intentar primero sintaxis de rango <Celda>:<Celda>) para SUM/AVG/MIN/MAX
                size_t saved_pos = m_pos;
                size_t saved_refs = m_refs.size();
                bool parsed_as_range = false;
                if (ident == "SUM" || ident == "AVG" || ident == "MIN" || ident == "MAX") {
                    skipSpaces();
                    size_t r1_start = m_pos;
                    while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                    std::string c1_str = texcel_detail::toUpper(m_src.substr(r1_start, m_pos - r1_start));
                    if (peek() == ':') {
                        get();
                        skipSpaces();
                        size_t r2_start = m_pos;
                        while (m_pos < m_src.size() && isIdentChar(m_src[m_pos])) ++m_pos;
                        std::string c2_str = texcel_detail::toUpper(m_src.substr(r2_start, m_pos - r2_start));
                        if (peek() == ')') {
                            get();
                            CellCoord a = CellCoord::parse(c1_str);
                            CellCoord b = CellCoord::parse(c2_str);
                            if (!a.valid || !b.valid) throw std::runtime_error("Rango inválido");
                            TX c_min = std::min(a.col, b.col), c_max = std::max(a.col, b.col);
                            TX r_min = std::min(a.row, b.row), r_max = std::max(a.row, b.row);
                            for (TX r = r_min; r <= r_max; ++r) {
                                for (TX col = c_min; col <= c_max; ++col) {
                                    texcel_detail::addUniqueStr(m_refs, CellCoord(col, r, true).toId());
                                }
                            }
                            AstToken tok;
                            tok.kind = AstNodeKind::FuncRange;
                            tok.func_name = ident;
                            tok.range_start = a.toId();
                            tok.range_end = b.toId();
                            tok.text = ident + "(" + a.toId() + ":" + b.toId() + ")";
                            return new texcel_detail::AstNode(tok, 0);
                        }
                    }
                }
                if (!parsed_as_range) {
                    m_pos = saved_pos;
                    while (m_refs.size() > saved_refs) m_refs.pop_back();
                }

                // MIN / MAX escalar o variádico sobre expresiones: MIN(e1, e2, ...) / MAX(e1, e2, ...)
                if (ident == "MIN" || ident == "MAX") {
                    texcel_detail::AstNode* acc = nullptr;
                    try {
                        acc = parseComparison();
                        while (peek() == ',') {
                            get();
                            texcel_detail::AstNode* nxt = parseComparison();
                            AstToken tok;
                            tok.kind = AstNodeKind::FuncBinary;
                            tok.func_name = ident;
                            tok.text = ident;
                            acc = texcel_detail::makeBinaryNode(tok, acc, nxt);
                        }
                        if (peek() != ')') throw std::runtime_error("Falta ')' en " + ident);
                        get();
                    } catch (...) {
                        texcel_detail::freeAst(acc);
                        throw;
                    }
                    return acc;
                }

                throw std::runtime_error("Función desconocida: " + ident);
            }

            if (ident == "TRUE" || ident == "FALSE") {
                AstToken tok;
                tok.kind = AstNodeKind::StringLiteral;
                tok.func_name = (ident == "TRUE") ? "True" : "False";
                tok.text = "\"" + tok.func_name + "\"";
                return new texcel_detail::AstNode(tok, 0);
            }

            CellCoord coord = CellCoord::parse(ident);
            if (!coord.valid) throw std::runtime_error("Celda inválida: " + ident);
            std::string cid = coord.toId();
            texcel_detail::addUniqueStr(m_refs, cid);

            AstToken tok;
            tok.kind = AstNodeKind::CellRef;
            tok.text = cid;
            return new texcel_detail::AstNode(tok, 0);
        }

        throw std::runtime_error("Carácter inesperado");
    }
};

#endif // __TEXCEL_PARSER_H__
