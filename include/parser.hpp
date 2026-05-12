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

    struct Parser {
        const LexResult& lex_result;
        const std::vector<Token>& tokens = lex_result.tokens;
        std::vector<ParseResult::Error> errors{};
        std::size_t current = 0;

        ParseResult parse();

        std::string_view substr(std::size_t start, std::size_t current);
        Verbatim parse_macro();
        Verbatim parse_include();
        std::optional<Expr> parse_expr(bool allow_reg, std::optional<std::reference_wrapper<const Token>> op);
        std::optional<MemExpr> parse_mem_expr();
        std::optional<Store> parse_store();
        std::optional<Load> parse_load();
        std::optional<Stmt> parse_stmt();
        std::optional<Goto> parse_if();
        std::optional<Stmt> parse_rhseq(const RegExpr& rc);
        std::optional<Stmt> parse_rhsopeq(const RegExpr& rc);
        std::optional<std::variant<RegExpr, Expr>> parse_goto();
        std::optional<Inst> parse_inst(const Token& identifier, std::vector<std::variant<Expr, RegExpr>> params);
        std::optional<RegExpr> parse_reg_expr();

        bool check_reg_expr();
        bool match_blank(Token::Type type);
        std::optional<Expr> parse_atomic(bool allow_reg);
        std::optional<std::vector<std::variant<Expr, RegExpr>>> parse_macro_param();
        const Token& peek();
        bool match(Token::Type type);
        bool match_until(Token::Type type);
        bool match_until(std::initializer_list<Token::Type> types);
        bool expect(Token::Type type, const std::string& msg);
        bool expect_blank(Token::Type type, const std::string& msg);
        void error(const std::string& msg);
    };
}

#endif