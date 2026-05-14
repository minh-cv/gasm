#include <fstream>
#include <iostream>
#include <optional>
#include <algorithm>
#include <variant>
#include <cerrno>
#include <cstring>

#include <token.hpp>
#include <lexer.hpp>
#include <parser.hpp>
#include <transpiler.hpp>

namespace {
std::string escape(std::string_view s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            case '\r': result += "\\r"; break;
            case '\\': result += "\\\\"; break;
            case '\"': result += "\\\""; break;
            case '\0': result += "\\0"; break;
            // case '\'': result += "\\'"; break;
            default:   result += c; break;
        }
    }
    return result;
}

auto print_stmt = [](auto&& arg) {
    using namespace gasm;
    using T = std::decay_t<decltype(arg)>;

    auto print_opt = []<typename T>(const std::optional<T>& opt, auto&& print_then) {
        if (opt == std::nullopt) {
            std::cout << "{}";
            return;
        }
        print_then(*opt);
    };

    auto print_expr =[&](const auto& expr) {
        std::cout << (std::is_same_v<std::decay_t<decltype(expr)>, RegExpr> ? "$" : "") << "{" << escape(expr.data) << "}";
    };

    auto print_mem = [&](const MemExpr& mem) {
        print_opt(mem.reg, print_expr);
        if (mem.is_plus) std::cout << " + ";
        else std::cout << " - ";
        print_opt(mem.expr, print_expr);
    };

    if constexpr (std::is_same_v<T, Verbatim>) {
        std::cout << "Verbatim(" << escape(arg.data) << ")\n";
    }
    else if constexpr (std::is_same_v<T, Op>) {
        std::cout << "Op(" << gasm::Token::to_string(arg.type) << ", ";
        print_expr(arg.ra);
        std::cout << ", ";
        print_expr(arg.rb);
        std::cout << ", ";
        print_expr(arg.rc);
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, Move>) {
        std::cout << "Move(";
        std::visit(print_expr, arg.src);
        std::cout << ", ";
        print_expr(arg.rc);
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, Inst>) {
        std::cout << "Inst(";
        std::cout <<  arg.identifier.lexeme;
        for (const auto& param: arg.params) {
            std::cout << ", ";
            std::visit(print_expr, param);
        }
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, OpC>) {
        std::cout << "OpC(" << gasm::Token::to_string(arg.type) << ", ";
        print_expr(arg.ra);
        std::cout << ", ";
        print_expr(arg.lit);
        std::cout << ", ";
        print_expr(arg.rc);
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, Load>) {
        std::cout << "Load(";
        print_mem(arg.mem);
        std::cout << ", ";
        print_expr(arg.reg);
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, Store>) {
        std::cout << "Store(";
        print_mem(arg.mem);
        std::cout << ", ";
        print_expr(arg.reg);
        std::cout << ")\n";
    }
    else if constexpr (std::is_same_v<T, Goto>) {
        std::cout << "Goto(";
        if (arg.neg) std::cout << "!, ";
        print_opt(arg.cond, print_expr);
        std::cout << ", ";
        print_opt(arg.dest, print_expr);
        std::cout << ", ";
        std::visit(print_expr, arg.target);
        std::cout << ")\n";
    }
    else {
        std::cout << "Unimplemented: " << typeid(arg).name() << "\n";
    }
};
}

int invalid_param(int errcode) {
    std::cerr << "Usage: gasm <input_source> [-g | -b | --debug]\n";
    return errcode;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return invalid_param(1);
    }
    std::ifstream input_source{argv[1]};
    if (!input_source.is_open()) {
        std::cerr << "Error: cannot find file '" << argv[1] << "'.";
        return 2;
    }

    enum {
        GAMMA,
        BETA,
        DEBUG,
    } mode = BETA;

    if (argc >= 3) {
        std::string mode_str = argv[2];
        if (mode_str == "-g") {
            mode = GAMMA;
        }
        else if (mode_str == "--debug") {
            mode = DEBUG;
        }
        else if (mode_str == "-b") {}
        else {
            return invalid_param(1);
        }
    }

    std::string text((std::istreambuf_iterator<char>(input_source)),
                  std::istreambuf_iterator<char>());
    if (std::any_of(text.begin(), text.end(), [] (char c) {return c & 0x80;})) {
        std::cerr << "Error: non-ASCII encoding is not supported";
        return 3;
    }
    gasm::Lexer lexer{text};
    auto lex_result = lexer.lex();
    if (mode == DEBUG) {
        std::cout << "== Lexer ==\n";
        for (const gasm::Token& token: lex_result.tokens) {
            std::cout << '\"' << escape(token.lexeme) << "\" " << gasm::Token::to_string(token.type) << " " << token.line << " " << token.col << '\n';
        }
        std::cout << std::flush;
    }
    if (!lex_result.ok()) {
        for (const auto& error: lex_result.errors) {
            lex_result.print_error(error);
        }
    }
    gasm::Parser parser{lex_result};
    gasm::ParseResult parse_result = parser.parse();
    if (mode == DEBUG) {
        std::cout << "\n== Parser ==\n";
        for (const gasm::Stmt& stmt: parse_result.stmts) {
            std::visit(print_stmt, stmt);
        }
    }
    if (!parse_result.ok()) {
        for (const auto& error: parse_result.errors) {
            parse_result.print_error(error);
        }
        std::cout << std::flush;
    }

    if (mode != DEBUG && !(parse_result.ok() && lex_result.ok())) {
        return 4;
    }

    gasm::ToBeta to_beta{std::cout, parse_result};
    gasm::ToGamma to_gamma{std::cout, parse_result};

    if (mode == DEBUG) {
        std::cout << "\n== Beta ==\n";
        to_beta.transpile();
        std::cout << "\n\n== Gamma ==\n";
        to_gamma.transpile();
        std::cout << "\n";
        std::cout << std::flush;
    }
    else if (mode == GAMMA) {
        to_gamma.transpile();
    }
    else {
        to_beta.transpile();
    }
}