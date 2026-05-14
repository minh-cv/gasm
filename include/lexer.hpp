#ifndef GASM_LEXER
#define GASM_LEXER

#include <vector>
#include <string>

#include <token.hpp>

namespace gasm {
    struct LexResult {
        struct Error {
            std::string msg;
            std::size_t line;
            std::size_t col;
        };
        std::vector<gasm::Token> tokens;
        std::vector<Error> errors;
        std::vector<std::string_view> lines;

        bool ok() const;
        void print_error(const Error& err) const;
    };
    struct Lexer {
        const std::string& code;
        std::vector<gasm::Token> tokens{};
        std::vector<LexResult::Error> errors{};
        std::vector<std::string_view> lines{};
        std::size_t current = 0;
        std::size_t start = 0;
        std::size_t line = 1;
        std::size_t col_start_idx = 0;
        gasm::Token::Type type = gasm::Token::TOKEN_EOF;

        gasm::LexResult lex();

        bool match_next(char c);
        void lex_op_eq(gasm::Token::Type optype, gasm::Token::Type opeqtype);
        void lex_shift_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype, gasm::Token::Type shiftshifttype, gasm::Token::Type shiftshifteqtype);
        void lex_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype);
        static bool check_identifier_character(char c);
        void lex_identifier();
        void lex_number();
        void lex_string();
        void lex_char();
        void error(std::string_view msg);
        void push_line(std::string::const_iterator end);
        void push_token();
        void merge_token();
    };
}

#endif