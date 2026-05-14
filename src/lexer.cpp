#include "constant.hpp"
#include <lexer.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <token.hpp>

using enum gasm::Token::Type;

gasm::LexResult gasm::Lexer::lex() {
    while (current < code.size()) {
        start = current;
        switch (code[current]) {
        case '(':
            type = LEFT_PAREN; break;
        case ')':
            type = RIGHT_PAREN; break;
        case '[':
            type = LEFT_BRACKET; break;
        case ']':
            type = RIGHT_BRACKET; break;
        case '{':
            type = LEFT_BRACE; break;
        case '}':
            type = RIGHT_BRACE; break;
        case ',':
            type = COMMA; break;
        case ':':
            type = COLON; break;
        case '%':
            type = PERCENTAGE; break;
        case '~':
            type = TILDE; break;
        case '=':
            lex_op_eq(EQUAL, EQUAL_EQUAL);
            break;
        case '!':
            lex_op_eq(BANG, BANG_EQUAL); 
            break;
        case '+':
            lex_op_eq(PLUS, PLUS_EQUAL);
            break;
        case '-':
            lex_op_eq(MINUS, MINUS_EQUAL);
            break;
        case '*':
            lex_op_eq(STAR, STAR_EQUAL);
            break;
        case '/':
            lex_op_eq(SLASH, SLASH_EQUAL);
            break;
        case '^':
            lex_op_eq(CARET, CARET_EQUAL);
            break;
        case '&':
            lex_op_eq(AMPERSAND, AMPERSAND_EQUAL);
            break;
        case '#':
            lex_op_eq(HASH, HASH_EQUAL);
            break;
        case '>':
            if (!(current + 1 < code.size() && code[current + 1] == '>')) {
                error("unrecognized character '>'");
                break;
            }
            ++current;
            lex_shift('>', GREATER_GREATER, GREATER_GREATER_EQUAL, GREATER_GREATER_GREATER, GREATER_GREATER_GREATER_EQUAL);
            break;
        case '<':
            lex_shift('<', LESS, LESS_EQUAL, LESS_LESS, LESS_LESS_EQUAL);
            break;
        case '|':
            while (current + 1 < code.size() && code[current + 1] != '\n') {
                ++current;
            }
            type = COMMENT;
            break;
        case ' ': case '\t': case '\r': case '\v': case '\f':
            type = BLANK;
            break;
        case '\n':
            type = NEWLINE;
            break;
        case '"':
            lex_string();
            break;
        case '\'':
            lex_char();
            break;
        default:
            if (std::isdigit(code[current])) {
                lex_number();
                break;
            }
            if (Lexer::check_identifier_character(code[current])) {
                lex_identifier();
                break;
            }
            error(std::string{"unrecognized character '"} + code[current] + "'");
            break;
        }
        if ((type == BLANK || type == NEWLINE) && !(tokens.empty()) && tokens.back().type == type && tokens.back().lexeme.data() + tokens.back().lexeme.size() == code.data() + start) {
            merge_token();
        }
        else {
            push_token();
        }
        if (type == NEWLINE) {
            push_line(code.begin() + static_cast<std::string_view::difference_type>(current + 1));
            ++line;
            col_start_idx = current + 1;
        }
        ++current;
    }
    push_line(code.cend());

    tokens.push_back({.lexeme = {code.end(), code.end()}, .type = TOKEN_EOF, .line = line, .col = current - col_start_idx + 1});
    return {tokens, errors, lines};
}

bool gasm::Lexer::match_next(char c) {
    return current + 1 < code.size() && code[current + 1] == c;
}

void gasm::Lexer::lex_op_eq(gasm::Token::Type optype, gasm::Token::Type opeqtype) {
    if (!match_next('=')) {
        type = optype;
        return;
    }
    ++current;
    type = opeqtype;
}

void gasm::Lexer::lex_shift_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype, gasm::Token::Type shiftshifttype, gasm::Token::Type shiftshifteqtype) {
    if (match_next('=')) {
        ++current;
        type = cmpeqtype;
        return;
    }
    if (!match_next(op)) {
        type = cmptype;
        return;
    }
    ++current;
    lex_shift(op, shifttype, shifteqtype, shiftshifttype, shiftshifteqtype);
}

void gasm::Lexer::lex_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype) {
        if (match_next('=')) {
        ++current;
        type = cmpeqtype;
        return;
    }
    if (!match_next(op)) {
        type = cmptype;
        return;
    }
    ++current;
    lex_op_eq(shifttype, shifteqtype);
}

bool gasm::Lexer::check_identifier_character(char c) {
    return std::isalnum(c) || c == '.' || c == '_' || c == '$';
}

void gasm::Lexer::lex_identifier() {
    while (current + 1 < code.size() && (check_identifier_character(code[current + 1]))) {
        ++current;
    }
    type = IDENTIFIER;
    if (current - start == 1) {
        switch (code[start]) {
            case 'x': case 'b':
            case 'l': case 's':
            if (code[start + 1] == 'p') {
                type = REG_IDENTIFIER;
                return;
            }
            break;
            case 'X': case 'B':
            case 'L': case 'S':
            if (code[start + 1] == 'P') {
                type = REG_IDENTIFIER;
                return;
            }
            break;
        }
    }
    std::string str = code.substr(start, current - start + 1);
    if ((str == "r" || str == "R") && match_next('(')) {
        type = R_PAREN;
        ++current;
        return;
    }
    if (str == "if") {
        type = IF;
        return;
    }
    if (str == "goto") {
        type = GOTO;
        return;
    }
    if (DIRECTIVES.count(str) == 1) { 
        type = DIRECTIVES.at(str);
        return;
    }
    if (!(code[start] == 'r' || code[start] == 'R')) {
        return;
    }
    if (current - start == 1) {
        type = REG_IDENTIFIER;
        return;
    }
    if ((current - start == 2 && (
        (code[start + 1] < '3' && code[start + 1] > '0' && std::isdigit(code[start + 2])) || 
        (code[start + 1] == '3' && code[start + 2] >= '0' && code[start + 2] <= '1')))) {
        type = REG_IDENTIFIER;
        return;
    }
}

void gasm::Lexer::lex_number() {
    int base = 10;
    bool had_error = false;
    bool had_base = false;
    if (code[current] == '0' && current + 1 < code.size()) {
        char c = code[current + 1];
        if (!(check_identifier_character(code[current + 1]))) {
            type = NUMBER;
            return;
        }
        had_base = true;
        switch (std::tolower(c)) {
        case 'b':
            base = 2;
            ++current;
            break;
        case 'x':
            base = 16;
            ++current;
            break;
        case 'o':
            base = 8;
            ++current;
            break;
        default:
            had_base = false;
            break;
        };
    }
    std::size_t base_idx = current;
    while (current + 1 < code.size() && check_identifier_character(code[current + 1])) {
        char c = code[current + 1];
        if (std::isdigit(c)) {
            int digit_val = c - '0';
            if (digit_val >= base) {
                had_error = true;
            }
        }
        else if (('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')) {
            int digit_val = std::tolower(c) - 'a' + 10;
            if (digit_val >= base) {
                had_error = true;
            }
        }
        else {
            had_error = true;
        }
        ++current;
    }
    if (had_base && base_idx == current) {
        had_error = true;
    }
    if (had_error) {
        error("invalid number '" + code.substr(start, current + 1 - start) + "'");
        return;
    }
    type = NUMBER;
}

void gasm::Lexer::lex_string() {
    bool escape = false;
    bool terminated = false;
    while (current + 1 < code.size() && !terminated) {
        if (escape) {
            switch (code[current + 1]) {
            case '\n':
                error("string not closed");
                return;
            default:
                break;
            }
            escape = false;
        }
        else {
            switch (code[current + 1]) {
            case '\\':
                escape = true;
                break;
            case '"':
                terminated = true;
                break;
            case '\n':
                error("unterminated string");
                return;
            default:
                break;
            }
        }
        ++current;
    }
    if (!terminated) {
        error("unterminated string");
        return;
    }
    type = STRING;
}

void gasm::Lexer::lex_char() {
    bool escape = false;
    bool terminated = false;
    int length = 0;
    while (current + 1 < code.size() && !terminated) {
        if (escape) {
            switch (code[current + 1]) {
            case '\n':
                error("unterminated char");
                return;
            default:
                break;
            }
            escape = false;
        }
        else {
            ++length;
            switch (code[current + 1]) {
            case '\\':
                escape = true;
                break;
            case '\'':
                terminated = true;
                --length;
                break;
            case '\n':
                error("unterminated char");
                return;
            default:
                break;
            }
        }
        ++current;
    }
    if (!terminated) {
        error("unterminated char");
        return;
    }
    if (length != 1) {
        error("a char must contain exactly 1 character");
        return;
    }
    type = NUMBER;
}

void gasm::Lexer::error(std::string_view msg) {
    type = ERROR;
    errors.push_back({msg.data(), line, start + 1 - col_start_idx});
}

void gasm::Lexer::push_line(std::string::const_iterator end) {
    std::string_view line_str{
        code.begin() + static_cast<std::string_view::difference_type>(col_start_idx), 
        end};
    lines.push_back(line_str);
}

void gasm::Lexer::push_token() {
        std::string_view lexeme = {
        code.cbegin() + static_cast<std::string_view::difference_type>(start), 
        code.cbegin() + static_cast<std::string_view::difference_type>(current + 1)};
    tokens.push_back({.lexeme = std::move(lexeme), .type = type, .line = line, .col = start - col_start_idx + 1});
}

void gasm::Lexer::merge_token() {
        std::string_view& view = tokens.back().lexeme;
    view = std::string_view{
        view.data(), 
        current + 1 - start + view.size()
    };
}

void gasm::LexResult::print_error(const Error& err) const {
    std::string_view line_str = lines[err.line - 1];

    std::cerr << "[line " << err.line << ", col " << err.col << "] Error: " << err.msg << "\n\t" << line_str << (line_str.size() == 0 || line_str.back() == '\n' ? "" : "\n") << "\t";
    std::string substr = {line_str.begin(), line_str.begin() + err.col - 1};
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

bool gasm::LexResult::ok() const {
    return errors.empty();
}