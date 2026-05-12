#ifndef GASM_CONSTANTS
#define GASM_CONSTANTS

#include <optional>
#include <token.hpp>
#include <unordered_map>

namespace gasm {
    extern const std::unordered_map<std::string_view, gasm::Token::Type> DIRECTIVES;
    
    std::optional<unsigned int> match_inst(std::string_view inst, std::size_t arity);
    std::optional<int> precedence(Token::Type type);
    bool is_op(Token::Type type);
    bool is_binary_op(Token::Type type);
    bool is_gamma_binary_op(Token::Type type);
    bool is_beta_binary_op(Token::Type type);
    std::optional<std::string_view> get_instruction(Token::Type type);
    std::optional<std::string_view> get_operator_string(Token::Type type);
}

#endif