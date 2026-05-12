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
    LexResult lex(const std::string& code);
}

#endif