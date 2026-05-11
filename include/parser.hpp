#ifndef GASM_PARSER
#define GASM_PARSER

#include <vector>
#include <string>
#include <variant>
#include <optional>

#include <token.hpp>
#include <lexer.hpp>

namespace gasm {
    struct Verbatim {
        std::string_view data;
    };

    struct Expr {
        std::string_view data;
        int lowest_precedence;
        int inner_lowest_precedence;
        bool has_reg_identifier;
    };

    struct RegExpr {
        std::string_view data;
        bool is_reg_identifier;
    };

    struct MemExpr {
        std::optional<RegExpr> reg;
        bool is_plus;
        std::optional<Expr> expr;
    };

    struct Load {
        MemExpr mem;
        RegExpr reg;
    };

    struct Store {
        MemExpr mem;
        RegExpr reg;
    };

    struct Goto {
        bool neg;
        std::optional<RegExpr> cond;
        std::optional<RegExpr> dest;
        std::variant<RegExpr, Expr> target;
        bool uses_boolean;
    };

    struct Op {
        Token::Type type;
        RegExpr ra;
        RegExpr rb;
        RegExpr rc;
    };

    struct OpC {
        Token::Type type;
        RegExpr ra;
        Expr lit;
        RegExpr rc;
    };

    struct Move {
        std::variant<RegExpr, Expr> src;
        RegExpr rc;
    };

    struct Inst {
        std::vector<std::variant<Expr, RegExpr>> params;
        const Token& identifier;
        unsigned int code;
    };

    using Stmt = std::variant<
        Verbatim,
        Move,
        Op,
        OpC,
        Goto,
        Load,
        Store,
        Inst
    >;

    struct ParseResult {
        std::vector<Stmt> stmts;
        const std::vector<std::string_view>& lines;

        struct Error {
            const Token& token;
            std::string msg;
        };
        std::vector<Error> errors;

        void print_error(const Error& err) const;
        bool ok() const;
    };

    ParseResult parse(const LexResult& tokens);
}

#endif