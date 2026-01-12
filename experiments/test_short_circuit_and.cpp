#include <iostream>
#include <vector>
#include "NovelMind/scripting/vm.hpp"
#include "NovelMind/scripting/opcode.hpp"

using namespace NovelMind::scripting;

int main() {
    VirtualMachine vm;

    std::cout << "Testing AND short-circuit evaluation\n";
    std::cout << "====================================\n\n";

    // Test 1: false && false (should short-circuit, not evaluate right)
    std::cout << "Test 1: false && false (should return false, short-circuit)\n";
    {
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 0},      // Push false
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF_NOT, 6},    // If false, jump to end
            {OpCode::POP, 0},            // Pop if true
            {OpCode::PUSH_BOOL, 0},      // Right side: Push false (should not execute)
            // Position 6 (end):
            {OpCode::STORE_VAR, 0},      // Store result
            {OpCode::HALT, 0}
        };

        std::vector<std::string> strings = {"result"};
        auto result = vm.load(program, strings);
        if (!result.isOk()) {
            std::cout << "  FAIL: Failed to load program\n";
            return 1;
        }

        vm.run();
        auto val = vm.getVariable("result");
        bool boolVal = std::holds_alternative<bool>(val) && std::get<bool>(val);
        std::cout << "  Result: " << (boolVal ? "true" : "false") << "\n";
        std::cout << "  " << (!boolVal ? "PASS" : "FAIL") << "\n\n";
    }

    // Test 2: true && false (should evaluate right)
    std::cout << "Test 2: true && false (should return false, evaluate right)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 1},      // Push true
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF_NOT, 6},    // If false, jump to end
            {OpCode::POP, 0},            // Pop the true
            {OpCode::PUSH_BOOL, 0},      // Right side: Push false
            // Position 6 (end):
            {OpCode::STORE_VAR, 0},      // Store result
            {OpCode::HALT, 0}
        };

        std::vector<std::string> strings = {"result"};
        auto result = vm.load(program, strings);
        if (!result.isOk()) {
            std::cout << "  FAIL: Failed to load program\n";
            return 1;
        }

        vm.run();
        auto val = vm.getVariable("result");
        bool boolVal = std::holds_alternative<bool>(val) && std::get<bool>(val);
        std::cout << "  Result: " << (boolVal ? "true" : "false") << "\n";
        std::cout << "  " << (!boolVal ? "PASS" : "FAIL") << "\n\n";
    }

    // Test 3: true && true (should evaluate right)
    std::cout << "Test 3: true && true (should return true, evaluate right)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 1},      // Push true
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF_NOT, 6},    // If false, jump to end
            {OpCode::POP, 0},            // Pop the true
            {OpCode::PUSH_BOOL, 1},      // Right side: Push true
            // Position 6 (end):
            {OpCode::STORE_VAR, 0},      // Store result
            {OpCode::HALT, 0}
        };

        std::vector<std::string> strings = {"result"};
        auto result = vm.load(program, strings);
        if (!result.isOk()) {
            std::cout << "  FAIL: Failed to load program\n";
            return 1;
        }

        vm.run();
        auto val = vm.getVariable("result");
        bool boolVal = std::holds_alternative<bool>(val) && std::get<bool>(val);
        std::cout << "  Result: " << (boolVal ? "true" : "false") << "\n";
        std::cout << "  " << (boolVal ? "PASS" : "FAIL") << "\n\n";
    }

    // Test 4: false && true (should short-circuit)
    std::cout << "Test 4: false && true (should return false, short-circuit)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 0},      // Push false
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF_NOT, 6},    // If false, jump to end
            {OpCode::POP, 0},            // Pop if true
            {OpCode::PUSH_BOOL, 1},      // Right side: Push true (should not execute)
            // Position 6 (end):
            {OpCode::STORE_VAR, 0},      // Store result
            {OpCode::HALT, 0}
        };

        std::vector<std::string> strings = {"result"};
        auto result = vm.load(program, strings);
        if (!result.isOk()) {
            std::cout << "  FAIL: Failed to load program\n";
            return 1;
        }

        vm.run();
        auto val = vm.getVariable("result");
        bool boolVal = std::holds_alternative<bool>(val) && std::get<bool>(val);
        std::cout << "  Result: " << (boolVal ? "true" : "false") << "\n";
        std::cout << "  " << (!boolVal ? "PASS" : "FAIL") << "\n\n";
    }

    std::cout << "All tests completed!\n";
    return 0;
}
