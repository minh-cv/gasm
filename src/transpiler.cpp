#include <transpiler.hpp>

#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <variant>
#include <cassert>
#include <unordered_map>

#include <token.hpp>
#include <parser.hpp>

using namespace gasm;
using enum gasm::Token::Type;

void gasm::to_beta(std::ostream& stream, const gasm::ParseResult& parse_result) {
struct Vistor {
    std::ostream& stream;

    void operator()(const Verbatim& v) {
        stream << v.data;
    }

    std::string_view trim_reg_expr(const RegExpr& expr) {
        if (expr.is_reg_identifier) {
            return expr.data;
        }
        return std::string_view{expr.data.begin() + 2, expr.data.end() - 1};
    }

    std::string_view trim_expr(const Expr& expr, int trim_precedence) {
        if (expr.inner_lowest_precedence <= trim_precedence) {
            return expr.data;
        }
        assert(expr.data.size() >= 2);
        if (expr.data.front() == '(' && expr.data.back() == ')') {
            return expr.data.substr(1, expr.data.size() - 2);
        }
        return expr.data;
    }

    void operator()(const Move& m) {
        if (std::holds_alternative<RegExpr>(m.src)) {
            stream << "MOVE(" << trim_reg_expr(std::get<RegExpr>(m.src)) << ", " << trim_reg_expr(m.rc) << ")";
            return;
        }
        stream << "CMOVE(" << std::get<Expr>(m.src).data << ", " << trim_reg_expr(m.rc) << ")";
    }

    void operator()(const Store& st) {
        stream << "ST(" << trim_reg_expr(st.reg) << ", ";
        if (st.mem.reg == std::nullopt) {
            assert(st.mem.expr != std::nullopt);
            stream << st.mem.expr->data;
        }
        else {
            if (st.mem.expr == std::nullopt) {
                stream << "0";
            }
            else {
                if (!st.mem.is_plus) {
                    stream << "-" << st.mem.expr->data;
                }
                else {
                    stream << trim_expr(*st.mem.expr, 1); // precedence(PLUS)
                }
            }
            stream << ", " << trim_reg_expr(*st.mem.reg);
        }
        stream << ")";
    }

    void operator()(const Load& st) {
        stream << "LD(";
        if (st.mem.reg == std::nullopt) {
            assert(st.mem.expr != std::nullopt);
            stream << st.mem.expr->data;
        }
        else {
            stream << trim_reg_expr(*st.mem.reg) << ", ";
            if (st.mem.expr == std::nullopt) {
                stream << "0";
            }
            else {
                if (!st.mem.is_plus) {
                    stream << "-" << st.mem.expr->data;
                }
                else {
                    stream << trim_expr(*st.mem.expr, 2);
                }
            }
        }
        stream << ", " << trim_reg_expr(st.reg) << ")";
    }

    void operator()(const Inst& inst) {
        stream << inst.identifier.lexeme << "(";
        if (inst.params.size() == 0) {
            stream << ")";
            return;
        }
        {
            std::size_t i = 0;
            if (std::holds_alternative<RegExpr>(inst.params[i])) {
                stream << trim_reg_expr(std::get<RegExpr>(inst.params[i]));
            }
            else {
                stream << std::get<Expr>(inst.params[i]).data;
            }
        }
        for (std::size_t i = 1; i < inst.params.size(); i++) {
            stream << ", ";
            if (std::holds_alternative<RegExpr>(inst.params[i])) {
                stream << trim_reg_expr(std::get<RegExpr>(inst.params[i]));
            }
            else {
                stream << std::get<Expr>(inst.params[i]).data;
            }
        }
        stream << ")";
    }

    void operator()(const Goto& g) {
        if (g.cond == std::nullopt) {
            if (std::holds_alternative<RegExpr>(g.target)) {
                stream << "JMP(";
                stream << trim_reg_expr(std::get<RegExpr>(g.target));
                if (g.dest != std::nullopt) {
                    stream << ", " << trim_reg_expr(*g.dest);
                }
                stream << ")";
                return;
            }
            stream << "BR(" << std::get<Expr>(g.target).data;
            if (g.dest != std::nullopt) {
                stream << ", " << trim_reg_expr(*g.dest);
            }
            stream << ")";
            return;
        }
        if (g.neg) {
            if (g.uses_boolean) {
                stream << "BF(";
            }
            else {
                stream << "BEQ(";
            }
        }
        else {
            if (g.uses_boolean) {
                stream << "BT(";
            }
            else {
                stream << "BNE(";
            }
        }
        assert(std::holds_alternative<Expr>(g.target));
        const Expr& target = std::get<Expr>(g.target);
        stream << trim_reg_expr(*g.cond) << ", " << target.data;
        if (g.dest != std::nullopt) {
            stream << ", " << trim_reg_expr(*g.dest);
        }
        stream << ")";
    }

    

    std::string_view get_opcode(Token::Type type) {
        switch (type) {
        case PLUS:
            return "ADD";
        case MINUS:
            return "SUB";
        case STAR:
            return "MUL";
        case SLASH:
            return "DIV";
        case AMPERSAND:
            return "AND";
        case HASH:
            return "OR";
        case LESS:
            return "CMPLT";
        case EQUAL_EQUAL:
            return "CMPEQ";
        case LESS_EQUAL:
            return "CMPLE";
        case GREATER_GREATER:
            return "SHR";
        case GREATER_GREATER_GREATER:
            return "SRA";
        case LESS_LESS:
            return "SHL";
        case CARET:
            return "XOR";
        default:
            assert(false);
        }
    }

    void operator()(const Op& op) {
        stream << get_opcode(op.type) << "(" 
            << trim_reg_expr(op.ra) << ", "
            << trim_reg_expr(op.rb) << ", "
            << trim_reg_expr(op.rc) << ")";
    }

    void operator()(const OpC& op) {
        stream << get_opcode(op.type) << "C(" 
            << trim_reg_expr(op.ra) << ", "
            << op.lit.data << ", "
            << trim_reg_expr(op.rc) << ")";
    }

} visitor{stream};

    for (const auto& stmt: parse_result.stmts) {
        std::visit(visitor, stmt);
    }
}
void gasm::to_gamma(std::ostream& stream, const gasm::ParseResult& parse_result) {
struct Vistor {
    std::ostream& stream;

    void operator()(const Verbatim& v) {
        stream << v.data;
    }

    std::string_view trim_reg_expr(const RegExpr& expr) {
        if (expr.is_reg_identifier) {
            return expr.data;
        }
        return std::string_view{expr.data.begin() + 2, expr.data.end() - 1};
    }

    std::string_view trim_expr(const Expr& expr, int trim_precedence) {
        if (expr.inner_lowest_precedence <= trim_precedence) {
            return expr.data;
        }
        assert(expr.data.size() >= 2);
        if (expr.data.front() == '(' && expr.data.back() == ')') {
            return expr.data.substr(1, expr.data.size() - 2);
        }
        return expr.data;
    }

    void operator()(const Move& m) {
        stream << m.rc.data << " = ";
        if (std::holds_alternative<RegExpr>(m.src)) {
            stream << std::get<RegExpr>(m.src).data;
            return;
        }
        stream << std::get<Expr>(m.src).data;
    }

    void print_mem(const MemExpr& expr) {
        stream << "[";
        if (expr.reg == std::nullopt) {
            assert(expr.expr != std::nullopt);
            stream << expr.expr->data << "]";
            return;
        }
        stream << expr.reg->data;
        if (expr.expr == std::nullopt) {
            stream << "]";
            return;
        }
        stream << (expr.is_plus ? " + " : " - ") << expr.expr->data << "]";
    }

    void operator()(const Store& st) {
        print_mem(st.mem);
        stream << " = " << st.reg.data;
    }

    void operator()(const Load& st) {
        stream << st.reg.data << " = " ;
        print_mem(st.mem);
    }

    Token::Type get_type(std::string_view view) {
        static const std::unordered_map<std::string_view, Token::Type> mp{
            {"ADD", PLUS},
            {"SUB", MINUS},
            {"MUL", STAR},
            {"DIV", SLASH},
            {"AND", AMPERSAND},
            {"OR", HASH},
            {"CMPLT", LESS},
            {"CMPEQ", EQUAL_EQUAL},
            {"CMPLE", LESS_EQUAL},
            {"SHR", GREATER_GREATER},
            {"SRA", GREATER_GREATER_GREATER},
            {"SHL", LESS_LESS},
            {"XOR", CARET},
        };
        if (mp.count(view) == 1) {
            return mp.at(view);
        }
        assert(false);
    }

    RegExpr transform_reg_expr(const std::variant<Expr, RegExpr>& var, std::string& buffer) {
        if (std::holds_alternative<RegExpr>(var)) {
            return std::get<RegExpr>(var);
        }
        const Expr& expr = std::get<Expr>(var);
        buffer = std::string{"R("} + std::string{expr.data.begin(), expr.data.end()} + ")";
        return RegExpr{{buffer.begin(), buffer.end()}, false};
    }

    Expr transform_expr(const std::variant<Expr, RegExpr>& var, std::string& buffer, int transform_precedence = std::numeric_limits<int>::max()) {
        if (std::holds_alternative<Expr>(var)) {
            const Expr& expr = std::get<Expr>(var);
            if (expr.lowest_precedence <= transform_precedence && !expr.has_reg_identifier) {
                return expr;
            }
            buffer = std::string{"("} + std::string{expr.data.begin(), expr.data.end()} + ")";
            return Expr{{buffer.begin(), buffer.end()}, 0, expr.inner_lowest_precedence, false};
        }
        const RegExpr& expr = std::get<RegExpr>(var);
        buffer = std::string{"("} + std::string{expr.data.begin(), expr.data.end()} + ")";
        return Expr{{buffer.begin(), buffer.end()}, 0, 0, false};
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

    void operator()(const Inst& inst) {
        std::string buffer[3];
        std::size_t size = inst.params.size();
        if (inst.identifier.lexeme == "CMOVE") {
            Expr src = transform_expr(inst.params[0], buffer[0]);
            RegExpr rc = transform_reg_expr(inst.params[1], buffer[1]);
            operator()(Move{
                src,
                rc
            });
            return;
        }
        if (inst.identifier.lexeme == "MOVE") {
            RegExpr src = transform_reg_expr(inst.params[0], buffer[0]);
            RegExpr rc = transform_reg_expr(inst.params[1], buffer[1]);
            operator()(Move{
                src,
                rc
            });
            return;
        }
        if (inst.identifier.lexeme == "LD") {
            RegExpr reg = transform_reg_expr(size == 2 ? inst.params[1] : inst.params[2], buffer[0]);
            std::optional<RegExpr> base = size == 2 ? std::optional<RegExpr>{} : std::optional{transform_reg_expr(inst.params[0], buffer[1])};
            std::optional<Expr> offset = transform_expr(size == 2 ? inst.params[0] : inst.params[1], buffer[2], precedence(PLUS));
            bool is_plus = true;
            if (size == 3) {
                assert(offset->data.size() != 0);
                if (offset->data == "0") {
                    offset = {};
                }
                else if (offset->data[0] == '-') {
                    is_plus = false;
                    offset->data = offset->data.substr(1);
                }
            }
            operator()(Load{
                MemExpr{
                    base,
                    is_plus,
                    offset
                },
                reg
            });
            return;
        }
        if (inst.identifier.lexeme == "ST") {
            RegExpr reg = transform_reg_expr(inst.params[0], buffer[0]);
            std::optional<RegExpr> base = size == 2 ? std::optional<RegExpr>{} : std::optional{transform_reg_expr(inst.params[2], buffer[1])};
            std::optional<Expr> offset = transform_expr(inst.params[1], buffer[2], precedence(PLUS));
            bool is_plus = true;
            if (size == 3) {
                assert(offset->data.size() != 0);
                if (offset->data == "0") {
                    offset = {};
                }
                else if (offset->data[0] == '-') {
                    is_plus = false;
                    offset->data = offset->data.substr(1);
                }
            }
            operator()(Store{
                MemExpr{
                    base,
                    is_plus,
                    offset
                },
                reg
            });
            return;
        }
        if (inst.identifier.lexeme == "JMP") {
            RegExpr src = transform_reg_expr(inst.params[0], buffer[0]);
            std::optional<RegExpr> dest{};
            if (size == 2) {
                dest = transform_reg_expr(inst.params[1], buffer[1]);
            }
            operator()(Goto{
                false,
                {},
                dest,
                src,
                false
            });
            return;
        }
        if (inst.identifier.lexeme == "BR") {
            Expr src = transform_expr(inst.params[0], buffer[0]);
            std::optional<RegExpr> dest{};
            if (size == 2) {
                dest = transform_reg_expr(inst.params[1], buffer[1]);
            }
            operator()(Goto{
                false,
                {},
                dest,
                src,
                false
            });
            return;
        }
        if (inst.identifier.lexeme == "BEQ") {
            RegExpr cond = transform_reg_expr(inst.params[0], buffer[0]);
            Expr label = transform_expr(inst.params[1], buffer[1]);
            std::optional<RegExpr> dest;
            if (size == 3) {
                dest = transform_reg_expr(inst.params[2], buffer[2]);
            }
            operator()(Goto{
                true,
                cond,
                dest,
                label,
                false
            });
            return;
        }
        if (inst.identifier.lexeme == "BF") {
            RegExpr cond = transform_reg_expr(inst.params[0], buffer[0]);
            Expr label = transform_expr(inst.params[1], buffer[1]);
            std::optional<RegExpr> dest;
            if (size == 3) {
                dest = transform_reg_expr(inst.params[2], buffer[2]);
            }
            operator()(Goto{
                true,
                cond,
                dest,
                label,
                true
            });
            return;
        }
        if (inst.identifier.lexeme == "BNE") {
            RegExpr cond = transform_reg_expr(inst.params[0], buffer[0]);
            Expr label = transform_expr(inst.params[1], buffer[1]);
            std::optional<RegExpr> dest;
            if (size == 3) {
                dest = transform_reg_expr(inst.params[2], buffer[2]);
            }
            operator()(Goto{
                false,
                cond,
                dest,
                label,
                false
            });
            return;
        }
        if (inst.identifier.lexeme == "BT") {
            RegExpr cond = transform_reg_expr(inst.params[0], buffer[0]);
            Expr label = transform_expr(inst.params[1], buffer[1]);
            std::optional<RegExpr> dest;
            if (size == 3) {
                dest = transform_reg_expr(inst.params[2], buffer[2]);
            }
            operator()(Goto{
                false,
                cond,
                dest,
                label,
                true
            });
            return;
        }
        switch (inst.code) {
        case 0x37:
            operator()(Op{
                get_type(inst.identifier.lexeme), 
                transform_reg_expr(inst.params[0], buffer[0]), 
                transform_reg_expr(inst.params[1], buffer[1]), 
                transform_reg_expr(inst.params[2], buffer[2])
            });
            break;
        case 0x35: {
            Token::Type type = get_type(inst.identifier.lexeme.substr(0, inst.identifier.lexeme.size() - 1));
            operator()(OpC{
                type,
                transform_reg_expr(inst.params[0], buffer[0]), 
                transform_expr(inst.params[1], buffer[1], precedence(type)), 
                transform_reg_expr(inst.params[2], buffer[2])
            });
            break;
        }
        default:
            assert(false);
        }
    }

    void operator()(const Goto& g) {
        if (g.cond != std::nullopt) {
            stream << "if (";
            if (g.neg && g.uses_boolean) {
                stream << "!";
            }
            stream << g.cond->data;
            if (!g.uses_boolean) {
                stream << (g.neg ? " != 0" : " == 0");
            }
            stream << ") ";
        }
        if (g.dest != std::nullopt) {
            stream << g.dest->data << " = ";
        }
        stream << "goto ";
        if (std::holds_alternative<RegExpr>(g.target)) {
            stream << std::get<RegExpr>(g.target).data;
            return;
        }
        stream << std::get<Expr>(g.target).data;
    }

    std::string_view get_sign(Token::Type type) {
        switch (type) {
        case PLUS:
            return "+";
        case MINUS:
            return "-";
        case STAR:
            return "*";
        case SLASH:
            return "/";
        case AMPERSAND:
            return "&";
        case HASH:
            return "#";
        case LESS:
            return "<";
        case EQUAL_EQUAL:
            return "==";
        case LESS_EQUAL:
            return "<=";
        case GREATER_GREATER:
            return ">>";
        case GREATER_GREATER_GREATER:
            return ">>>";
        case LESS_LESS:
            return "<<";
        case CARET:
            return "^";
        default:
            assert(false);
        }
    }

    void operator()(const Op& op) {
        if (op.type == MINUS && (op.ra.data == "r31" || op.ra.data == "R31")) {
            stream << op.rc.data << " = -" << op.rb.data;
        }
        else if (op.rc.data == op.ra.data) {
            stream << op.rc.data << " " << get_sign(op.type) << "= " << op.rb.data;
        }
        else {
            stream << op.rc.data << " = " << op.ra.data << " " << get_sign(op.type) << " " << op.rb.data;
        }
    }

    void operator()(const OpC& op) {
        if (op.type == CARET && (op.lit.data == "-1")) {
            stream << op.rc.data << " = ~" << op.ra.data;
        }
        else if (op.rc.data == op.ra.data) {
            stream << op.rc.data << " " << get_sign(op.type) << "= " << op.lit.data;
        }
        else {
            stream << op.rc.data << " = " << op.ra.data << " " << get_sign(op.type) << " " << op.lit.data;
        }
    }

} visitor{stream};

    for (const auto& stmt: parse_result.stmts) {
        std::visit(visitor, stmt);
    }
}