#ifndef GASM_TRANSPILER
#define GASM_TRANSPILER

#include <vector>
#include <string>
#include <iostream>

#include <parser.hpp>

namespace gasm {
    void to_beta(std::ostream& stream, const gasm::ParseResult& parse_result);
    void to_gamma(std::ostream& stream, const gasm::ParseResult& parse_result);
}

#endif