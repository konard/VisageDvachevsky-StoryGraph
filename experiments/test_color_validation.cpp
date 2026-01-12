// Test script to verify color literal validation
#include "NovelMind/scripting/lexer.hpp"
#include <iostream>
#include <vector>

using namespace NovelMind::scripting;

void testColor(const std::string& input) {
    Lexer lexer;
    auto result = lexer.tokenize(input);

    std::cout << "Input: \"" << input << "\" -> ";
    if (result.isOk()) {
        const auto& tokens = result.value();
        if (tokens.size() > 0) {
            std::cout << "OK, Token: \"" << tokens[0].lexeme << "\" Type: "
                      << static_cast<int>(tokens[0].type) << std::endl;
        } else {
            std::cout << "OK but no tokens" << std::endl;
        }
    } else {
        std::cout << "ERROR: " << result.error() << std::endl;
    }
}

int main() {
    std::cout << "Testing valid color formats:\n";
    testColor("#FFF");           // Valid 3-digit
    testColor("#FFCC00");        // Valid 6-digit
    testColor("#FFAA");          // Valid 4-digit with alpha
    testColor("#FF00CC88");      // Valid 8-digit with alpha

    std::cout << "\nTesting INVALID formats that should be rejected:\n";
    testColor("#GGG");           // Invalid: G is not hex
    testColor("#GGGGGG");        // Invalid: G is not hex
    testColor("#FF00ZZ");        // Invalid: Z is not hex
    testColor("#12345G");        // Invalid: G is not hex
    testColor("#12");            // Invalid length
    testColor("#12345");         // Invalid length
    testColor("#1234567");       // Invalid length
    testColor("#123456789");     // Invalid length
    testColor("#");              // Empty color

    return 0;
}
