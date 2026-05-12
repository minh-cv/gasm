#include <lexer.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <string_view>
#include <iostream>
#include <unordered_set>
#include <token.hpp>

using enum gasm::Token::Type;

gasm::LexResult gasm::lex(const std::string& code) {
    struct Lexer {
        const std::string& code;
        std::vector<gasm::Token> tokens{};
        std::vector<LexResult::Error> errors{};
        std::vector<std::string_view> lines{};
        std::size_t current = 0;
        std::size_t start = 0;
        std::size_t line = 1;
        std::size_t col_start_idx = 0;
        gasm::Token::Type type = TOKEN_EOF;

        bool match_next(char c) {
            return current + 1 < code.size() && code[current + 1] == c;
        }

        void lex_op_eq(gasm::Token::Type optype, gasm::Token::Type opeqtype) {
            Lexer& l = *this;
            if (!l.match_next('=')) {
                    l.type = optype;
                    return;
                }
                ++l.current;
                l.type = opeqtype;
        }

        void lex_shift_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype, gasm::Token::Type shiftshifttype, gasm::Token::Type shiftshifteqtype) {
            Lexer& l = *this;
            if (l.match_next('=')) {
                ++l.current;
                l.type = cmpeqtype;
                return;
            }
            if (!l.match_next(op)) {
                l.type = cmptype;
                return;
            }
            ++l.current;
            l.lex_shift(op, shifttype, shifteqtype, shiftshifttype, shiftshifteqtype);
        }

        void lex_shift(char op, gasm::Token::Type cmptype, gasm::Token::Type cmpeqtype, gasm::Token::Type shifttype, gasm::Token::Type shifteqtype) {
            Lexer& l = *this;
            if (l.match_next('=')) {
                ++l.current;
                l.type = cmpeqtype;
                return;
            }
            if (!l.match_next(op)) {
                l.type = cmptype;
                return;
            }
            ++l.current;
            l.lex_op_eq(shifttype, shifteqtype);
        }

        const std::unordered_map<std::string, gasm::Token::Type> DIRECTIVES{{".macro", MACRO}, {".include", INCLUDE}, {".options", OPTIONS}, {".protect", NO_PARAM_DIRECTIVE}, {".align", ALIGN}, {".ascii", TEXT}, {".text", TEXT}, {".breakpoint", NO_PARAM_DIRECTIVE}, {".unprotect", NO_PARAM_DIRECTIVE}, {".pcheckoff", CHECKOFF}, {".tcheckoff", CHECKOFF}, {".verify", VERIFY}};

        bool match_directive(const std::string& str) const {
            return DIRECTIVES.count(str) == 1;
        }

        bool match_op_identifier(const std::string& str) const {
            static const std::unordered_set<std::string> OP_IDENTIFIER{"ADD", "ADDC", "SUB", "SUBC", "MUL", "MULC", "DIV", "DIVC", "AND", "ANDC", "OR", "ORC", "XOR", "XORC", "CMPEQ", "CMPLT", "CMPLE", "SHL", "SHLC", "SHR", "SHRC", "SRA", "SRAC", "BEQ", "BF", "BNE", "BT", "BR", "JMP", "LD", "ST", "MOVE", "CMOVE"};
            return OP_IDENTIFIER.count(str) == 1;
        }

        static bool check_identifier_character(char c) {
            return std::isalnum(c) || c == '.' || c == '_' || c == '$';
        }

        void lex_identifier() {
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
                // while (current + 1 < code.size() && code[current + 1] != '\n') {
                //     ++current;
                // }
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

        void lex_number() {
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

        void lex_string() {
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

        void lex_char() {
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

        void error(std::string_view msg) {
            type = ERROR;
            errors.push_back({msg.data(), line, start + 1 - col_start_idx});
        }

        void push_line(std::string::const_iterator end) {
            std::string_view line_str{
                code.begin() + static_cast<std::string_view::difference_type>(col_start_idx), 
                end};
            lines.push_back(line_str);
        }

        void push_token() {
            auto& l = *this;
            std::string_view lexeme = {
                code.cbegin() + static_cast<std::string_view::difference_type>(l.start), 
                code.cbegin() + static_cast<std::string_view::difference_type>(l.current + 1)};
            l.tokens.push_back({.lexeme = std::move(lexeme), .type = l.type, .line = l.line, .col = l.start - l.col_start_idx + 1});
        }

        void merge_token() {
            auto& l = *this;
            std::string_view& view = l.tokens.back().lexeme;
            view = std::string_view{
                view.data(), 
                l.current + 1 - l.start + view.size()
            };
        }
    };
    
    Lexer l{code};
    while (l.current < code.size()) {
        l.start = l.current;
        switch (code[l.current]) {
        case '(':
            l.type = LEFT_PAREN; break;
        case ')':
            l.type = RIGHT_PAREN; break;
        case '[':
            l.type = LEFT_BRACKET; break;
        case ']':
            l.type = RIGHT_BRACKET; break;
        case '{':
            l.type = LEFT_BRACE; break;
        case '}':
            l.type = RIGHT_BRACE; break;
        case ',':
            l.type = COMMA; break;
        case ':':
            l.type = COLON; break;
        case '%':
            l.type = PERCENTAGE; break;
        case '~':
            l.type = TILDE; break;
        case '=':
            l.lex_op_eq(EQUAL, EQUAL_EQUAL);
            break;
        case '!':
            l.lex_op_eq(BANG, BANG_EQUAL); 
            break;
        case '+':
            l.lex_op_eq(PLUS, PLUS_EQUAL);
            break;
        case '-':
            l.lex_op_eq(MINUS, MINUS_EQUAL);
            break;
        case '*':
            l.lex_op_eq(STAR, STAR_EQUAL);
            break;
        case '/':
            l.lex_op_eq(SLASH, SLASH_EQUAL);
            break;
        case '^':
            l.lex_op_eq(CARET, CARET_EQUAL);
            break;
        case '&':
            l.lex_op_eq(AMPERSAND, AMPERSAND_EQUAL);
            break;
        case '#':
            l.lex_op_eq(HASH, HASH_EQUAL);
            break;
        case '>':
            if (!(l.current + 1 < code.size() && code[l.current + 1] == '>')) {
                l.error("unrecognized character '>'");
                break;
            }
            ++l.current;
            l.lex_shift('>', GREATER_GREATER, GREATER_GREATER_EQUAL, GREATER_GREATER_GREATER, GREATER_GREATER_GREATER_EQUAL);
            break;
        case '<':
            l.lex_shift('<', LESS, LESS_EQUAL, LESS_LESS, LESS_LESS_EQUAL);
            break;
        case '|':
            while (l.current + 1 < code.size() && code[l.current + 1] != '\n') {
                ++l.current;
            }
            l.type = COMMENT;
            break;
        case ' ': case '\t': case '\r': case '\v': case '\f':
            l.type = BLANK;
            break;
        case '\n':
            l.type = NEWLINE;
            break;
        case '"':
            l.lex_string();
            break;
        case '\'':
            l.lex_char();
            break;
        default:
            if (std::isdigit(code[l.current])) {
                l.lex_number();
                break;
            }
            if (Lexer::check_identifier_character(code[l.current])) {
                l.lex_identifier();
                break;
            }
            l.error(std::string{"unrecognized character '"} + code[l.current] + "'");
            break;
        }
        if ((l.type == BLANK || l.type == NEWLINE) && !(l.tokens.empty()) && l.tokens.back().type == l.type && l.tokens.back().lexeme.data() + l.tokens.back().lexeme.size() == l.code.data() + l.start) {
            l.merge_token();
        }
        else {
            l.push_token();
        }
        if (l.type == NEWLINE) {
            l.push_line(code.begin() + static_cast<std::string_view::difference_type>(l.current + 1));
            ++l.line;
            l.col_start_idx = l.current + 1;
        }
        ++l.current;
    }
    l.push_line(code.cend());

    l.tokens.push_back({.lexeme = {code.end(), code.end()}, .type = TOKEN_EOF, .line = l.line, .col = l.current - l.col_start_idx + 1});
    return {l.tokens, l.errors, l.lines};
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