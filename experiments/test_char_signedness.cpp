#include <iostream>
#include <string_view>
#include <cstdint>

// Demonstrates the char signedness issue

int main() {
    // Create a string with a high-bit byte (UTF-8 Cyrillic character Я = 0xD0 0xAF)
    std::string_view source = "Я";  // UTF-8: 0xD0 0xAF

    // Access the first byte
    char c = source[0];  // This is 0xD0 (208 in unsigned, -48 in signed)
    unsigned char uc = static_cast<unsigned char>(c);

    std::cout << "Byte value: 0x" << std::hex << static_cast<int>(uc) << std::dec << std::endl;
    std::cout << "As signed char: " << static_cast<int>(c) << std::endl;
    std::cout << "As unsigned char: " << static_cast<int>(uc) << std::endl;

    // The problem: if we compare without casting
    if (c >= 0x80) {
        std::cout << "Signed char >= 0x80: TRUE" << std::endl;
    } else {
        std::cout << "Signed char >= 0x80: FALSE (BUG!)" << std::endl;
    }

    if (uc >= 0x80) {
        std::cout << "Unsigned char >= 0x80: TRUE (CORRECT!)" << std::endl;
    } else {
        std::cout << "Unsigned char >= 0x80: FALSE" << std::endl;
    }

    return 0;
}
