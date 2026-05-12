#ifndef GASM_TOKEN
#define GASM_TOKEN

#include <string_view>
#include <array>

namespace gasm {
struct Token {
    enum Type {
        // Single-character tokens.
        LEFT_PAREN,
        RIGHT_PAREN,
        LEFT_BRACKET,
        RIGHT_BRACKET,
        LEFT_BRACE,
        RIGHT_BRACE,
        COLON,
        COMMA,
        TILDE,
        DOLLAR,
        PERCENTAGE,

        // One or two character tokens.
        EQUAL,
        EQUAL_EQUAL,
        BANG,
        BANG_EQUAL,
        LESS,
        LESS_EQUAL,
        MINUS,
        MINUS_EQUAL,
        PLUS,
        PLUS_EQUAL,
        HASH,
        HASH_EQUAL,
        SLASH,
        SLASH_EQUAL,
        STAR,
        STAR_EQUAL,
        GREATER_GREATER,
        GREATER_GREATER_EQUAL,
        GREATER_GREATER_GREATER,
        GREATER_GREATER_GREATER_EQUAL,
        LESS_LESS,
        LESS_LESS_EQUAL,
        AMPERSAND,
        AMPERSAND_EQUAL,
        R_PAREN, // r( or R(
        CARET,
        CARET_EQUAL,

        // Literals.
        NUMBER,
        IDENTIFIER,
        REG_IDENTIFIER,
        STRING,

        // Directives
        MACRO,
        INCLUDE,
        OPTIONS,
        ALIGN,
        TEXT,
        NO_PARAM_DIRECTIVE,
        CHECKOFF,
        VERIFY,

        // Others
        COMMENT,
        BLANK,
        NEWLINE,
        ERROR,

        // Keywords.
        GOTO,
        IF,

        TOKEN_EOF,
    };
    std::string_view lexeme;
    Type type;
    std::size_t line;
    std::size_t col;

    static std::string_view to_string(Type type) {
        static const std::array map{
        // Single-character tokens.
        "LEFT_PAREN",
        "RIGHT_PAREN",
        "LEFT_BRACKET",
        "RIGHT_BRACKET",
        "LEFT_BRACE",
        "RIGHT_BRACE",
        "COLON",
        "COMMA",
        "TILDE",
        "DOLLAR",
        "PERCENTAGE",

        // One or two character tokens.
        "EQUAL",
        "EQUAL_EQUAL",
        "BANG",
        "BANG_EQUAL",
        "LESS",
        "LESS_EQUAL",
        "MINUS",
        "MINUS_EQUAL",
        "PLUS",
        "PLUS_EQUAL",
        "HASH",
        "HASH_EQUAL",
        "SLASH",
        "SLASH_EQUAL",
        "STAR",
        "STAR_EQUAL",
        "GREATER_GREATER",
        "GREATER_GREATER_EQUAL",
        "GREATER_GREATER_GREATER",
        "GREATER_GREATER_GREATER_EQUAL",
        "LESS_LESS",
        "LESS_LESS_EQUAL",
        "AMPERSAND",
        "AMPERSAND_EQUAL",
        "R_PAREN", // r( or R(
        "CARET",
        "CARET_EQUAL",

        // Literals.
        "NUMBER",
        "IDENTIFIER",
        "REG_IDENTIFIER",
        "STRING",

        // Directives
        "MACRO",
        "INCLUDE",
        "OPTIONS",
        "ALIGN",
        "TEXT",
        "NO_PARAM_DIRECTIVE",
        "CHECKOFF",
        "VERIFY",

        // Others
        "COMMENT",
        "BLANK",
        "NEWLINE",
        "ERROR",

        // Keywords.
        "GOTO",
        "IF",

        "TOKEN_EOF",
        };
        return map[static_cast<std::size_t>(type)];
    }
};

}

#endif