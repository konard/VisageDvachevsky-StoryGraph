#include <iostream>
#include <vector>
#include "NovelMind/scripting/vm.hpp"
#include "NovelMind/scripting/opcode.hpp"

using namespace NovelMind::scripting;

int main() {
    VirtualMachine vm;

    std::cout << "Testing OR short-circuit evaluation\n";
    std::cout << "====================================\n\n";

    // Test 1: true || true (should short-circuit, not evaluate right)
    std::cout << "Test 1: true || true (should return true, short-circuit)\n";
    {
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 1},      // Push true
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF, 7},        // If true, jump to end (past right side)
            {OpCode::POP, 0},            // Pop if false
            {OpCode::PUSH_BOOL, 1},      // Right side: Push true
            {OpCode::PUSH_INT, 999},     // This should NOT be executed
            {OpCode::POP, 0},            // Clean up the 999
            // Position 7 (end):
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

    // Test 2: false || true (should evaluate right)
    std::cout << "Test 2: false || true (should return true, evaluate right)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 0},      // Push false
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF, 6},        // If true, jump to end
            {OpCode::POP, 0},            // Pop the false
            {OpCode::PUSH_BOOL, 1},      // Right side: Push true
            // Position 5 (not jumped to yet):
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

    // Test 3: false || false (should evaluate right)
    std::cout << "Test 3: false || false (should return false, evaluate right)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 0},      // Push false
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF, 6},        // If true, jump to end
            {OpCode::POP, 0},            // Pop the false
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

    // Test 4: true || false (should short-circuit)
    std::cout << "Test 4: true || false (should return true, short-circuit)\n";
    {
        vm = VirtualMachine(); // Reset
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 1},      // Push true
            {OpCode::DUP, 0},            // Duplicate for short-circuit
            {OpCode::JUMP_IF, 6},        // If true, jump to end
            {OpCode::POP, 0},            // Pop if false
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
        std::cout << "  " << (boolVal ? "PASS" : "FAIL") << "\n\n";
    }

    std::cout << "All tests completed!\n";
    return 0;
}
