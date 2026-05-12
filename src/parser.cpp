#include <parser.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <optional>
#include <functional>
#include <variant>
#include <unordered_set>
#include <token.hpp>

using namespace gasm;
using enum gasm::Token::Type;

ParseResult gasm::parse(const LexResult& lex_result) {
    const auto& tokens = lex_result.tokens;
struct Parser {
    const std::vector<gasm::Token>& tokens;
    std::vector<ParseResult::Error> errors{};
    std::size_t current = 0;

    std::string_view substr(std::size_t start, std::size_t current_idx) {
        if (current_idx == start) {
            return {tokens[start].lexeme.begin(), tokens[start].lexeme.begin()}; 
        }
        return {tokens[start].lexeme.begin(), tokens[current_idx - 1].lexeme.end()};
    }

    Verbatim parse_macro() {
        std::size_t start = current;
        assert(match(MACRO));
        if (!expect_blank(IDENTIFIER, std::string{"expect macro name after .macro"})) {
            return {};
        }

        if (match_blank(LEFT_PAREN)) {
            if (match_blank(IDENTIFIER)) {
                while (match_blank(COMMA)) {
                    if (!expect_blank(IDENTIFIER, "expect macro parameter name")) {
                        return {};
                    }
                }
            }

            if (!expect_blank(RIGHT_PAREN, "unclosed macro parameter list")) {
                return {};
            }
        }

        if (match_blank(LEFT_BRACE)) {
            if (!match_until(RIGHT_BRACE)) {
                error("unterminated macro body");
                return {};
            }
            ++current;
            return {substr(start, current)};
        }

        bool res = match_until({NEWLINE, TOKEN_EOF, COMMENT});
        assert(res);
        return {substr(start, current)};
    }

    Verbatim parse_include() {
        std::size_t start = current;
        assert(match(INCLUDE));
        match(BLANK);
        std::size_t file_start = current;
        bool res = match_until({BLANK, NEWLINE, TOKEN_EOF, COMMENT});
        assert(res);
        if (substr(file_start, current) == "") {
            error("empty file name");
            return {};
        }
        return {substr(start, current)};
    }

    std::optional<Expr> parse_expr(bool allow_reg, std::optional<std::reference_wrapper<const Token>> op) {
        std::size_t start = current;
        assert(!op.has_value() || is_gamma_binary_op(op->get().type));
        while (match_blank(MINUS) || match(TILDE)) {}
        match(BLANK);
        auto atomic_op = parse_atomic(allow_reg, true);
        if (atomic_op == std::nullopt) {
            return {};
        }
        match(BLANK);
        Token expr_op = peek();
        if (!is_op(expr_op.type)) {
            return Expr{substr(start, current), atomic_op->lowest_precedence, atomic_op->inner_lowest_precedence, atomic_op->has_reg_identifier};
        }
        std::string lexeme = std::string{expr_op.lexeme};
        if (!is_binary_op(expr_op.type)) {
            error(std::string{"operator '"} + lexeme + "' cannot be used as a binary operator");
            return {};
        }
        if (!is_beta_binary_op(expr_op.type)) {
            error(std::string{"operator '"} + lexeme + "' cannot be used in Beta context");
            return {};
        }
        ++current;
        match(BLANK);
        auto sub_expr = parse_expr(allow_reg, op);
        if (sub_expr == std::nullopt) {
            return {};
        }
        int expr_precedence = precedence(expr_op.type);
        if (op.has_value() && expr_precedence > precedence(op->get().type)) {
            error(std::string{"operator '"} + lexeme + "' has lower precedence than '" + std::string{op->get().lexeme} + "', use parenthesis if necessary");
            return {};
        }
        return Expr{
            substr(start, current), 
            std::max(expr_precedence, sub_expr->lowest_precedence),
            std::max(expr_precedence, sub_expr->inner_lowest_precedence), 
            sub_expr->has_reg_identifier || atomic_op->has_reg_identifier
        };
    }

    std::optional<MemExpr> parse_mem_expr() {
        assert(match(LEFT_BRACKET));
        match(BLANK);
        MemExpr mem_expr{};
        if (check_reg_expr()) {
            mem_expr.reg = parse_reg_expr();
            if (mem_expr.reg == std::nullopt) {
                return {}; // todo: have better error handling
            }
            match(BLANK);
            mem_expr.is_plus = peek().type == PLUS;
            auto op = std::cref(peek());
            if (match_blank(PLUS) || match(MINUS)) {
                match(BLANK);
                mem_expr.expr = parse_expr(false, op);
                if (mem_expr.expr == std::nullopt) {
                    return {};
                }
            }
        }
        else {
            mem_expr.expr = parse_expr(false, {});
            if (mem_expr.expr == std::nullopt) {
                return {};
            }
        }
        expect_blank(RIGHT_BRACKET, "unclosed bracket");
        return mem_expr;
    }

    std::optional<Store> parse_store() {
        Store store;
        auto mem_expr = parse_mem_expr();
        if (mem_expr == std::nullopt) {
            return {};
        }
        store.mem = *std::move(mem_expr);

        expect_blank(EQUAL, std::string{"expect '='"});

        match(BLANK);
        auto reg_expr = parse_reg_expr();
        if (reg_expr == std::nullopt) {
            return {};
        }
        store.reg = *std::move(reg_expr);
        return store;
    }

    std::optional<Load> parse_load() {
        Load load;
        auto reg_expr = parse_reg_expr();
        if (reg_expr == std::nullopt) return {};
        load.reg = *std::move(reg_expr);

        expect_blank(EQUAL, std::string{"expect '='"});

        match(BLANK);
        auto mem_expr = parse_mem_expr();
        if (mem_expr == std::nullopt) {
            return {};
        }
        load.mem = *std::move(mem_expr);
        return load;
    }

    std::optional<Stmt> parse_stmt() {
        std::size_t start = current;
        switch (peek().type) {
        case INCLUDE:
            return parse_include();
        case MACRO:
            return parse_macro();
        case IDENTIFIER: case NUMBER: {
            const Token& identifier = peek();
            assert(match_primary());
            match(BLANK);
            if (match(COLON)) {
                return Verbatim{substr(start, current)};
            }
            if (peek().type == LEFT_PAREN) {
                auto op = parse_macro_param();
                if (op == std::nullopt) {
                    return {};
                }
                auto inst = parse_inst(identifier, *op);
                if (inst == std::nullopt) {
                    return Verbatim{substr(start, current)};
                }
                return inst;
            }
            if (match(EQUAL)) {
                match(BLANK);
            }
            if (is_beta_binary_op(peek().type)) {
                ++current;
                match(BLANK);
                auto op = parse_expr(true, {});
                if (op == std::nullopt) {
                    return {};
                }
                return Verbatim{substr(start, current)};
            }
            return Verbatim{substr(start, current)};
        }
        case LEFT_BRACKET: {
            auto op = parse_store();
            if (op == std::nullopt) {
                return {};
            }
            return *std::move(op);
        }
        case REG_IDENTIFIER: case R_PAREN: {
            std::optional<RegExpr> rc = parse_reg_expr();
            if (rc == std::nullopt) {
                return {};
            }
            match(BLANK);
            switch (peek().type) {
            case EQUAL: {
                assert(match(EQUAL));
                match(BLANK);
                return parse_rhseq(*rc);
            }
            case PLUS_EQUAL: case MINUS_EQUAL: case STAR_EQUAL: case SLASH_EQUAL: case AMPERSAND_EQUAL: case HASH_EQUAL: case CARET_EQUAL: case GREATER_GREATER_EQUAL: case GREATER_GREATER_GREATER_EQUAL: case LESS_LESS_EQUAL: {
                return parse_rhsopeq(*rc);
            }
            default:
                if (rc->is_reg_identifier) {
                    return Verbatim{rc->data};
                }
                error("r-parenthesis cannot be used as a standalone expression");
                return {};
            }
            if (check_reg_expr()) {
                std::optional<RegExpr> ra = parse_reg_expr();
                if (ra == std::nullopt) {
                    return {};
                }
            }
            break;
        }
        case IF:
            return parse_if();
        case GOTO: {
            auto op = parse_goto();
            if (op == std::nullopt) {
                return {};
            }
            return Goto{false, {}, {}, *op, false};
        }
        case BLANK: case NEWLINE: case COMMENT:
            ++current;
            return Verbatim{substr(start, current)};
        case NO_PARAM_DIRECTIVE:
            ++current;
            return Verbatim{substr(start, current)};
        case TEXT:
            ++current;
            if (!expect_blank(STRING, "expect string after '.ascii' or '.text'")) {
                return {};
            }
            return Verbatim{substr(start, current)};
        case OPTIONS:
            ++current;
            match_until({TOKEN_EOF, NEWLINE, COMMENT});
            return Verbatim{substr(start, current)};
        case ALIGN:
            ++current;
            match(BLANK);
            switch (peek().type) {
            case NEWLINE: case COMMENT: case TOKEN_EOF:
                return Verbatim{substr(start, current)};
            default:
                auto expr = parse_expr(true, {});
                if (expr == std::nullopt) {
                    return {};
                }
                return Verbatim{substr(start, current)};
            }
        case CHECKOFF:
            ++current;
            if (!expect_blank(STRING, "expect two strings and a term after '.pcheckoff' or '.tcheckoff'")) {
                return {};
            }
            if (!expect_blank(STRING, "expect two strings and a term after '.pcheckoff' or '.tcheckoff'")) {
                return {};
            }
            match(BLANK);
            expect_primary("expect two strings and a term after '.pcheckoff' or '.tcheckoff'");
            return Verbatim{substr(start, current)};
        case VERIFY: {
            ++current;
            match_until({TOKEN_EOF, NEWLINE, COMMENT});
            return Verbatim{substr(start, current)};
        }
        default:
            error("unexpected token");
            return {};
        }
    }

    std::optional<Goto> parse_if() {
        assert(match(IF));
        if (!match_blank(LEFT_PAREN)) {
            return {};
        }
        
        bool neg = false;
        if (match_blank(BANG)) {
            neg = true;
        }

        match(BLANK);
        if (!check_reg_expr()) {
            error("expect a register");
            return {};
        }
        std::optional<RegExpr> cond = parse_reg_expr();
        if (cond == std::nullopt) {
            return {};
        }
        match(BLANK);
        bool uses_boolean = true;
        bool neg_boolean = peek().type == BANG_EQUAL;
        if (!neg && (match(BANG_EQUAL) || match(EQUAL_EQUAL))) {
            uses_boolean = false;
            neg = neg_boolean;
            match(BLANK);
            if (peek().lexeme != "0") {
                error("only '== 0' or '!= 0' is supported for BEQ/BF form");
                return {};
            }
            assert(peek().type == NUMBER);
            ++current;
            match(BLANK);
        }
        if (!expect(RIGHT_PAREN, "unclosed parenthesis")) {
            return {};
        }
        
        match(BLANK);
        std::optional<RegExpr> dest;
        if (check_reg_expr()) {
            dest = parse_reg_expr();
            if (dest == std::nullopt) {
                return {};
            }
            if (!expect_blank(EQUAL, "expect assignment")) {
                return {};
            }
        }
        
        match(BLANK);
        auto goto_expr = parse_goto();
        if (goto_expr == std::nullopt) {
            return {};
        }
        if (std::holds_alternative<RegExpr>(*goto_expr)) {
            error("goto <register> cannot go after if clause");
            return {};
        }

        return Goto{neg, cond, dest, *goto_expr, uses_boolean};
    }

    std::optional<Stmt> parse_rhseq(const RegExpr& rc) {
        switch (peek().type) {
        case MINUS: {
            std::size_t saved = current;
            ++current;
            match(BLANK);
            if (check_reg_expr()) {
                auto op = parse_reg_expr();
                if (op == std::nullopt) {
                    return {};
                }
                return {Op{MINUS, RegExpr{"R31", true}, *op, rc}};
            }
            current = saved;
            auto op = parse_expr(false, {});
            if (op == std::nullopt) {
                return {};
            }
            return {Move{*op, rc}};
        }
        case TILDE: {
            std::size_t saved = current;
            ++current;
            match(BLANK);
            if (check_reg_expr()) {
                auto op = parse_reg_expr();
                if (op == std::nullopt) {
                    return {};
                }
                return {OpC{CARET, *op, Expr{"-1", 0, 0, false}, rc}};
            }
            current = saved;
            auto op = parse_expr(false, {});
            if (op == std::nullopt) {
                return {};
            }
            return {Move{*op, rc}};
        }
        case IDENTIFIER: case NUMBER: case LEFT_PAREN: {
            auto op = parse_expr(false, {});
            if (op == std::nullopt) {
                return {};
            }
            return {Move{*op, rc}};
        }
        case LEFT_BRACKET: {
            auto op = parse_mem_expr();
            if (op == std::nullopt) {
                return {};
            }
            return {Load{*op, rc}};
        }
        case REG_IDENTIFIER: case R_PAREN: {
            auto ra = parse_reg_expr();
            if (ra == std::nullopt) {
                return {};
            }
            match(BLANK);
            if (is_gamma_binary_op(peek().type)) {
                const Token& op_token = peek();
                ++current;
                match(BLANK);
                if (check_reg_expr()) {
                    auto rb = parse_reg_expr();
                    if (rb == std::nullopt) {
                        return {};
                    }
                    return {Op{op_token.type, *ra, *rb, rc}};
                }
                auto lit = parse_expr(false, op_token);
                if (lit == std::nullopt) {
                    return {};
                }
                return {OpC{op_token.type, *ra, *lit, rc}};
            }
            return {Move{*ra, rc}};
        }
        case GOTO: {
            auto goto_src = parse_goto();
            if (goto_src == std::nullopt) {
                return {};
            }
            return {Goto{false, {}, rc, *goto_src, false}};
        }
        default:
            error("expected right hand side of instruction");
            return {};
        }
    }

    std::optional<Stmt> parse_rhsopeq(const RegExpr& rc) {
        Token::Type type = static_cast<Token::Type>(peek().type - 1);
        ++current;
        match(BLANK);
        if (check_reg_expr()) {
            auto op = parse_reg_expr();
            if (op == std::nullopt) {
                return {};
            }
            return Op{type, rc, *op, rc};
        }
        auto op = parse_expr(false, {});
        if (op == std::nullopt) {
            return {};
        }
        return OpC{type, rc, *op, rc};
    }

    std::optional<std::variant<RegExpr, Expr>> parse_goto() {
        if (!expect(GOTO, "expected 'goto'")) return {};
        match(BLANK);
        if (check_reg_expr()) {
            auto op = parse_reg_expr();
            if (op == std::nullopt) {
                return {};
            }
            return *op;
        }
        auto op = parse_expr(false, {});
        if (op == std::nullopt) {
            return {};
        }
        return *op;
    }

    std::optional<Inst> parse_inst(const Token& identifier, std::vector<std::variant<Expr, RegExpr>> params) {
        using Pair = std::pair<std::string_view, unsigned int>;
        static auto hasher = [](Pair p) {
            return (std::hash<std::string_view>()(p.first) << 8) + p.second;
        };
        using ValidPairs = std::unordered_set<Pair, decltype(hasher)>;
        static const ValidPairs valid_pairs = [] {
            static ValidPairs valid_pairs;
            for (std::string_view opcode: {"ADD", "SUB", "MUL", "DIV", "SHL", "SHR", "SRA", "CMPEQ", "CMPLT", "CMPLE", "AND", "OR", "XOR"}) {
                valid_pairs.insert({opcode, 0x37});
            }
            for (std::string_view opcode: {"ADDC", "SUBC", "MULC", "DIVC", "SHLC", "SHRC", "SRAC", "CMPEQC", "CMPLTC", "CMPLEC", "ANDC", "ORC", "XORC", "LD", "ST", "BEQ", "BNE", "BF", "BT"}) {
                valid_pairs.insert({opcode, 0x35});
            }
            for (std::string_view opcode: {"BEQ", "BNE", "BF", "BT", "ST"}) {
                valid_pairs.insert({opcode, 0x21});
            }
            for (std::string_view opcode: {"LD", "CMOVE", "BR"}) {
                valid_pairs.insert({opcode, 0x22});
            }
            for (std::string_view opcode: {"JMP", "MOVE"}) {
                valid_pairs.insert({opcode, 0x23});
            }
            valid_pairs.insert({"JMP", 0x11});
            valid_pairs.insert({"BR", 0x10});
            return valid_pairs;
        }();
        auto reg_bit = [&](std::size_t i) -> unsigned int {
            return i < params.size() &&
                std::holds_alternative<RegExpr>(params[i]);
        };

        unsigned int param_types =
            reg_bit(0) |
            (reg_bit(1) << 1u) |
            (reg_bit(2) << 2u);
        unsigned int params_code = static_cast<unsigned int>((params.size() << 4u) | param_types);
        Pair pair = {identifier.lexeme, params_code};
        if (valid_pairs.count(pair) != 1) {
            return {};
        }
        return Inst{params, identifier, params_code};
        
    }

    std::optional<RegExpr> parse_reg_expr() {
        std::size_t start = current;
        if (match(R_PAREN)) {
            auto op = parse_expr(true, {});
            if (!(expect_blank(RIGHT_PAREN, "unclosed r-parenthesis") && op != std::nullopt)) return {};
            return RegExpr{substr(start, current), false};
        }
        if (match(REG_IDENTIFIER)) {
            return RegExpr{substr(start, current), true};
        }
        error("expect register expression");
        return {};
    }

    bool check_reg_expr() {
        Token::Type type = peek().type;
        return type == R_PAREN || type == REG_IDENTIFIER;
    }

    bool match_blank(Token::Type type) {
        match(BLANK);
        return match(type);
    }

    int precedence(Token::Type type) {
        switch (type) {
        case STAR: case SLASH: case PERCENTAGE:
            return 1;
        case PLUS: case MINUS:
            return 2;
        case GREATER_GREATER: case LESS_LESS: case GREATER_GREATER_GREATER:
            return 3;
        case LESS: case LESS_EQUAL:
            return 4;
        case EQUAL_EQUAL:
            return 5;
        case AMPERSAND:
            return 6;
        case CARET:
            return 7;
        case HASH:
            return 8;
        default:
            assert(false);
        }
    }

    bool is_op(Token::Type type) {
        switch (type) {
            case PLUS: case MINUS: case STAR: case SLASH:
            case GREATER_GREATER:
            case GREATER_GREATER_GREATER: case LESS: case LESS_EQUAL:
            case EQUAL_EQUAL: case LESS_LESS: case HASH: case AMPERSAND:
            case CARET: case PERCENTAGE: case TILDE:
            return true;

            default:
            return false;
        }
    }

    bool is_binary_op(Token::Type type) {
        return is_op(type) && type != TILDE;
    }

    bool is_beta_binary_op(Token::Type type) {
        switch (type) {
            case PLUS: case MINUS: case STAR: case SLASH: 
            case GREATER_GREATER:case LESS_LESS: case PERCENTAGE:
            return true;

            default:
            return false;
        }
    }

    bool is_gamma_binary_op(Token::Type type) {
        switch (type) {
            case PLUS: case MINUS: case STAR: case SLASH:
            case GREATER_GREATER:
            case GREATER_GREATER_GREATER: case LESS: case LESS_EQUAL:
            case EQUAL_EQUAL: case LESS_LESS: case HASH: case AMPERSAND:
            case CARET:
            return true;

            default:
            return false;
        }
    }

    std::optional<Expr> parse_atomic(bool allow_reg, bool allow_paren) {
        std::size_t start = current;
        if (allow_reg && match(REG_IDENTIFIER)) {
            return Expr{substr(start, current), 0, 0, true};
        }
        if (allow_paren && match(LEFT_PAREN)) {
            auto op = parse_expr(true, {});
            if (!(expect_blank(RIGHT_PAREN, "unclosed parenthesis") && op != std::nullopt)) {
                return {};
            }
            return Expr{substr(start, current), 0, op->inner_lowest_precedence, false};
        }
        if (!expect_primary(std::string{"expect one of: identifier, number"} + (allow_reg ? ", register" : "") + (allow_paren ? ", expression" : ""))) {
            return {};
        }
        match(BLANK);
        if (peek().type == LEFT_PAREN) {
            auto op = parse_macro_param();
            if (op == std::nullopt) {
                return {};
            }
        }
        return Expr{substr(start, current), 0, 0, false};
    }

    std::optional<std::vector<std::variant<Expr, RegExpr>>> parse_macro_param() {
        assert(match(LEFT_PAREN));
        std::vector<std::variant<Expr, RegExpr>> vec;
        if (match_blank(RIGHT_PAREN)) {
            return vec;
        }
        do {
            match(BLANK);
            if (peek().type == REG_IDENTIFIER) {
                auto op = parse_reg_expr();
                if (op == std::nullopt) {
                    return {};
                }
                vec.push_back(*op);
            }
            else {
                auto op = parse_expr(true, {});
                if (!op.has_value()) {
                    return {};
                }
                vec.push_back(*op);
            }
        } while (match_blank(COMMA));

        if (!expect_blank(RIGHT_PAREN, "unclosed macro parameter list")) {
            return {};
        }
        return vec;
    }

    const Token& peek() {
        return tokens[current];
    }

    const Token& peek_next() {
        return tokens[current + 1];
    }

    bool match(Token::Type type) {
        if (current < tokens.size() && tokens[current].type == type) {
            ++current;
            return true;
        }
        return false;
    }

    bool match_until(Token::Type type) {
        while (current < tokens.size() && peek().type != type) {
            ++current;
        }
        return !(current == tokens.size());
    }

    bool match_until(std::initializer_list<Token::Type> types) {
        while (current < tokens.size()) {
            bool flag = false;
            for (const auto& type: types) {
                if (peek().type == type) {
                    flag = true;
                    break;
                }
            }
            if (flag) break;
            ++current;
        }
        return !(current == tokens.size());
    }

    bool expect(Token::Type type, const std::string& msg) {
        if (match(type)) {
            return true;
        }
        error(msg);
        return false;
    }

    bool expect_blank(Token::Type type, const std::string& msg) {
        match(BLANK);
        return expect(type, msg);
    }

    bool match_primary() {
        if (current < tokens.size() && (tokens[current].type == NUMBER || tokens[current].type == IDENTIFIER)) {
            ++current;
            return true;
        }
        return false;
    }

    bool expect_primary(const std::string& msg) {
        if (match_primary()) return true;
        error(msg);
        return false;
    }

    void error(const std::string& msg) {
        const Token& token = peek();
        errors.push_back({token, msg});
    }

} p{tokens};
    std::vector<Stmt> stmts;
    while (p.current < tokens.size() - 1) {
        auto stmt = p.parse_stmt();
        if (stmt == std::nullopt) {
            ++p.current;
            continue;
        }
        if (std::holds_alternative<Verbatim>(*stmt) && !stmts.empty() && std::holds_alternative<Verbatim>(stmts.back()) && std::get<Verbatim>(*stmt).data.begin() == std::get<Verbatim>(stmts.back()).data.end()) {
            std::get<Verbatim>(stmts.back()).data = {
                std::get<Verbatim>(stmts.back()).data.begin(),
                std::get<Verbatim>(*stmt).data.end(),
            };
            continue;
        }
        stmts.push_back(*std::move(stmt));
    }
    return ParseResult{stmts, lex_result.lines, p.errors};
}


void gasm::ParseResult::print_error(const Error& err) const {
    std::string_view line_str = lines[err.token.line - 1];
    std::cerr << "[line " << err.token.line << ", col " << err.token.col << "] Error: " << err.msg << "\n\t" << line_str << (line_str.size() == 0 || line_str.back() == '\n' ? "" : "\n") << "\t";
    std::string substr = {line_str.begin(), line_str.begin() + err.token.col - 1};
    for (char& c: substr) {
        switch (c) {
        case ' ': case '\t': case '\r': case '\v': case '\f':
            break;
        default:
            c = ' ';
            break;
        }
    }
    std::cerr << substr << "^\n";
}

bool gasm::ParseResult::ok() const {
    return errors.empty();
}