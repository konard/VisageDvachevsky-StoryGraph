#include <iostream>
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"

using namespace NovelMind::scripting;

int main() {
    Lexer lexer;
    Parser parser;

    // Test 1: Scene with missing closing brace
    std::cout << "=== Test 1: Scene with missing closing brace ===\n";
    auto tokens1 = lexer.tokenize(R"(
        scene incomplete {
            show Hero at center
            say "Hello"
    )");

    if (tokens1.isOk()) {
        auto result1 = parser.parse(tokens1.value());
        if (result1.isError()) {
            std::cout << "Error: " << result1.error() << "\n";
        } else {
            std::cout << "Parsing succeeded (unexpected)\n";
        }

        const auto& errors = parser.getErrors();
        std::cout << "Total errors: " << errors.size() << "\n";
        for (const auto& err : errors) {
            std::cout << "  Line " << err.location.line
                     << ", Col " << err.location.column
                     << ": " << err.message << "\n";
        }
    }

    std::cout << "\n=== Test 2: Scene ending at EOF without close brace ===\n";
    auto tokens2 = lexer.tokenize(R"(scene test {
    show background "bg_city"
)");

    if (tokens2.isOk()) {
        Parser parser2;
        auto result2 = parser2.parse(tokens2.value());
        if (result2.isError()) {
            std::cout << "Error: " << result2.error() << "\n";
        } else {
            std::cout << "Parsing succeeded (unexpected)\n";
        }

        const auto& errors2 = parser2.getErrors();
        std::cout << "Total errors: " << errors2.size() << "\n";
        for (const auto& err : errors2) {
            std::cout << "  Line " << err.location.line
                     << ", Col " << err.location.column
                     << ": " << err.message << "\n";
        }
    }

    return 0;
}
