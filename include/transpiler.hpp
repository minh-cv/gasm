#ifndef GASM_TRANSPILER
#define GASM_TRANSPILER

#include <iostream>
#include <limits>
#include <parser.hpp>

namespace gasm {
    struct ToBeta {
        std::ostream& stream;
        const gasm::ParseResult& parse_result;
        std::string_view trim_reg_expr(const RegExpr& expr);
        std::string_view trim_expr(const Expr& expr, int trim_precedence);
        void transpile();
        void operator()(const Verbatim& v);
        void operator()(const Move& m);
        void operator()(const Store& st);
        void operator()(const Load& ld);
        void operator()(const Inst& inst);
        void operator()(const Goto& g);
        void operator()(const Op& op);
        void operator()(const OpC& op);
    };

    struct ToGamma {
        std::ostream& stream;
        const gasm::ParseResult& parse_result;
        void print_mem(const MemExpr& expr);
        RegExpr transform_reg_expr(const std::variant<Expr, RegExpr>& var, std::string& buffer);
        Expr transform_expr(const std::variant<Expr, RegExpr>& var, std::string& buffer, int transform_precedence = std::numeric_limits<int>::max());
        void transpile();
        void operator()(const Verbatim& v);
        void operator()(const Move& m);
        void operator()(const Store& st);
        void operator()(const Load& ld);
        void operator()(const Inst& inst);
        void operator()(const Goto& g);
        void operator()(const Op& op);
        void operator()(const OpC& op);
    };
}

#endif