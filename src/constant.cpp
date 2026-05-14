#include "token.hpp"
#include <cassert>
#include <constant.hpp>
#include <optional>
#include <string_view>
#include <unordered_map>

using namespace gasm;
using enum gasm::Token::Type;

const std::unordered_map<std::string_view, gasm::Token::Type> gasm::DIRECTIVES{
    {".macro", MACRO},
    {".include", INCLUDE},
    {".options", OPTIONS},
    {".protect", NO_PARAM_DIRECTIVE},
    {".align", ALIGN},
    {".ascii", TEXT},
    {".text", TEXT},
    {".breakpoint", NO_PARAM_DIRECTIVE},
    {".unprotect", NO_PARAM_DIRECTIVE},
    {".pcheckoff", CHECKOFF},
    {".tcheckoff", CHECKOFF},
    {".verify", VERIFY}};

std::optional<unsigned int> gasm::match_inst(std::string_view inst, std::size_t arity) {
    using Pair = std::pair<std::string_view, std::size_t>;
    static auto hasher = [](Pair p) {
        return (std::hash<std::string_view>()(p.first) << 4) + p.second;
    };
    using ValidPairs = std::unordered_map<Pair, unsigned int, decltype(hasher)>;
    static const ValidPairs valid_pairs = [] {
        static ValidPairs pairs;
        for (std::string_view opcode: {"ADD", "SUB", "MUL", "DIV", "SHL", "SHR", "SRA", "CMPEQ", "CMPLT", "CMPLE", "AND", "OR", "XOR"}) {
            pairs.insert({{opcode, 3}, 0x37});
        }
        for (std::string_view opcode: {"ADDC", "SUBC", "MULC", "DIVC", "SHLC", "SHRC", "SRAC", "CMPEQC", "CMPLTC", "CMPLEC", "ANDC", "ORC", "XORC", "LD", "ST", "BEQ", "BNE", "BF", "BT"}) {
            pairs.insert({{opcode, 3}, 0x35});
        }
        for (std::string_view opcode: {"BEQ", "BNE", "BF", "BT", "ST"}) {
            pairs.insert({{opcode, 2}, 0x21});
        }
        for (std::string_view opcode: {"LD", "CMOVE", "BR"}) {
            pairs.insert({{opcode, 2}, 0x22});
        }
        for (std::string_view opcode: {"JMP", "MOVE"}) {
            pairs.insert({{opcode, 2}, 0x23});
        }
        pairs.insert({{"JMP", 1}, 0x11});
        pairs.insert({{"BR", 1}, 0x10});
        return pairs;
    }();
    if (valid_pairs.count({inst, arity}) == 0) {
        return {};
    }
    return valid_pairs.at({inst, arity});
}

std::optional<int> gasm::precedence(Token::Type type) {
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
        return {};
    }
}

bool gasm::is_op(Token::Type type) {
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

bool gasm::is_binary_op(Token::Type type) {
    return is_op(type) && type != TILDE;
}

bool gasm::is_beta_binary_op(Token::Type type) {
    switch (type) {
        case PLUS: case MINUS: case STAR: case SLASH: 
        case GREATER_GREATER:case LESS_LESS: case PERCENTAGE:
        return true;

        default:
        return false;
    }
}

bool gasm::is_gamma_binary_op(Token::Type type) {
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

std::optional<std::string_view> gasm::get_operator_string(Token::Type type) {
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
        return {};
    }
}

std::optional<std::string_view> gasm::get_instruction(Token::Type type) {
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
        return {};
    }
}

std::optional<Token::Type> gasm::get_type(std::string_view view) {
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
    return {};
}