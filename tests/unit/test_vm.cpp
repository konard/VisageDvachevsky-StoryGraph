#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/vm.hpp"

using namespace NovelMind::scripting;

TEST_CASE("VM initial state", "[scripting]")
{
    VirtualMachine vm;

    REQUIRE_FALSE(vm.isRunning());
    REQUIRE_FALSE(vm.isPaused());
    REQUIRE_FALSE(vm.isWaiting());
}

TEST_CASE("VM load empty program fails", "[scripting]")
{
    VirtualMachine vm;

    auto result = vm.load({}, {});
    REQUIRE(result.isError());
}

TEST_CASE("VM load and run simple program", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 42},
        {OpCode::HALT, 0}
    };

    auto result = vm.load(program, {});
    REQUIRE(result.isOk());

    vm.run();
    REQUIRE(vm.isHalted());
}

TEST_CASE("VM arithmetic operations", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 10},
        {OpCode::PUSH_INT, 5},
        {OpCode::ADD, 0},
        {OpCode::STORE_VAR, 0},  // Store to "result"
        {OpCode::HALT, 0}
    };

    std::vector<std::string> strings = {"result"};

    auto result = vm.load(program, strings);
    REQUIRE(result.isOk());

    vm.run();

    auto val = vm.getVariable("result");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 15);
}

TEST_CASE("VM subtraction", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 20},
        {OpCode::PUSH_INT, 8},
        {OpCode::SUB, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    auto val = vm.getVariable("result");
    REQUIRE(std::get<NovelMind::i32>(val) == 12);
}

TEST_CASE("VM multiplication", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 6},
        {OpCode::PUSH_INT, 7},
        {OpCode::MUL, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    auto val = vm.getVariable("result");
    REQUIRE(std::get<NovelMind::i32>(val) == 42);
}

TEST_CASE("VM comparison operations", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 5},
        {OpCode::PUSH_INT, 5},
        {OpCode::EQ, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"equal"});
    vm.run();

    auto val = vm.getVariable("equal");
    REQUIRE(std::holds_alternative<bool>(val));
    REQUIRE(std::get<bool>(val) == true);
}

TEST_CASE("VM conditional jump", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_BOOL, 1},      // true
        {OpCode::JUMP_IF, 4},        // Jump to instruction 4 if true
        {OpCode::PUSH_INT, 0},       // This should be skipped
        {OpCode::JUMP, 5},
        {OpCode::PUSH_INT, 1},       // This should execute
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    auto val = vm.getVariable("result");
    REQUIRE(std::get<NovelMind::i32>(val) == 1);
}

TEST_CASE("VM flags", "[scripting]")
{
    VirtualMachine vm;

    vm.load({{OpCode::HALT, 0}}, {});

    vm.setFlag("test_flag", true);
    REQUIRE(vm.getFlag("test_flag") == true);

    vm.setFlag("test_flag", false);
    REQUIRE(vm.getFlag("test_flag") == false);
}

TEST_CASE("VM variables", "[scripting]")
{
    VirtualMachine vm;

    vm.load({{OpCode::HALT, 0}}, {});

    vm.setVariable("int_var", NovelMind::i32{100});
    vm.setVariable("str_var", std::string{"hello"});
    vm.setVariable("bool_var", true);

    REQUIRE(std::get<NovelMind::i32>(vm.getVariable("int_var")) == 100);
    REQUIRE(std::get<std::string>(vm.getVariable("str_var")) == "hello");
    REQUIRE(std::get<bool>(vm.getVariable("bool_var")) == true);
}

TEST_CASE("VM pause and resume", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::NOP, 0},
        {OpCode::NOP, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.step();
    vm.pause();

    REQUIRE(vm.isPaused());

    vm.resume();
    REQUIRE_FALSE(vm.isPaused());
}

TEST_CASE("VM reset", "[scripting]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();
    REQUIRE(vm.isHalted());

    vm.reset();
    REQUIRE_FALSE(vm.isHalted());
    REQUIRE_FALSE(vm.isRunning());
}

TEST_CASE("VM JUMP to address 0", "[scripting][jump]")
{
    VirtualMachine vm;

    // Simpler test: Just test JUMP_IF to address 0 directly
    // Program counts from 0 to 3, then halts
    // IP tracking:
    // 0: PUSH_INT 1        -> stack: [1]
    // 1: LOAD_VAR counter  -> stack: [1, counter]
    // 2: ADD               -> stack: [counter+1]
    // 3: DUP               -> stack: [counter+1, counter+1]
    // 4: STORE_VAR counter -> counter = counter+1, stack: [counter+1]
    // 5: PUSH_INT 3        -> stack: [counter+1, 3]
    // 6: LT                -> stack: [counter+1 < 3]
    // 7: JUMP_IF 0         -> if true, jump to 0
    // 8: HALT
    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},       // 0: Push 1
        {OpCode::LOAD_VAR, 0},       // 1: Load counter
        {OpCode::ADD, 0},            // 2: counter + 1
        {OpCode::DUP, 0},            // 3: Duplicate result
        {OpCode::STORE_VAR, 0},      // 4: Store back to counter
        {OpCode::PUSH_INT, 3},       // 5: Push 3
        {OpCode::LT, 0},             // 6: counter < 3
        {OpCode::JUMP_IF, 0},        // 7: If true, jump to instruction 0
        {OpCode::HALT, 0}            // 8: Otherwise halt
    };

    vm.load(program, {"counter"});
    vm.setVariable("counter", NovelMind::i32{0});

    // Track IP values during execution
    int iterations = 0;
    const int maxIterations = 50; // Safety limit

    while (!vm.isHalted() && iterations < maxIterations) {
        vm.step();
        iterations++;
    }

    // Counter should be 3 (we looped 3 times: 0->1, 1->2, 2->3, then 3 < 3 is false)
    auto val = vm.getVariable("counter");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 3);
    REQUIRE(vm.isHalted());
}

TEST_CASE("VM JUMP to address 0 unconditional", "[scripting][jump]")
{
    VirtualMachine vm;

    // Simple program that jumps back to 0 unconditionally
    // Use a counter to ensure the jump works (max iterations will stop it)
    std::vector<Instruction> program = {
        {OpCode::LOAD_VAR, 0},       // 0: Load counter
        {OpCode::PUSH_INT, 1},       // 1: Push 1
        {OpCode::ADD, 0},            // 2: counter + 1
        {OpCode::STORE_VAR, 0},      // 3: Store back to counter
        {OpCode::JUMP, 0}            // 4: Unconditionally jump to 0
    };

    vm.load(program, {"counter"});
    vm.setVariable("counter", NovelMind::i32{0});

    // Execute exactly 10 steps (2 full loops = 10 instructions)
    for (int i = 0; i < 10; i++) {
        bool continued = vm.step();
        REQUIRE(continued);  // Should continue, not halt
    }

    // Counter should be 2 (completed 2 full loops)
    auto val = vm.getVariable("counter");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 2);

    // VM should NOT be halted (loop is infinite)
    REQUIRE_FALSE(vm.isHalted());
}

TEST_CASE("VM JUMP to middle of program", "[scripting][jump]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},       // 0
        {OpCode::JUMP, 3},           // 1: Skip to instruction 3
        {OpCode::PUSH_INT, 999},     // 2: Should be skipped
        {OpCode::STORE_VAR, 0},      // 3: Store 1 to result
        {OpCode::HALT, 0}            // 4
    };

    vm.load(program, {"result"});
    vm.run();

    // Result should be 1 (instruction 2 was skipped)
    auto val = vm.getVariable("result");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 1);
}

TEST_CASE("VM JUMP to invalid address halts", "[scripting][jump]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::JUMP, 999}          // 0: Invalid jump
    };

    vm.load(program, {});
    bool continued = vm.step();

    // VM should halt due to invalid jump
    REQUIRE_FALSE(continued);
    REQUIRE(vm.isHalted());
}

TEST_CASE("VM JUMP_IF_NOT to address 0", "[scripting][jump]")
{
    VirtualMachine vm;

    // Program: Loop while counter < 3 using JUMP_IF_NOT
    std::vector<Instruction> program = {
        {OpCode::LOAD_VAR, 0},       // 0: Load counter
        {OpCode::PUSH_INT, 1},       // 1: Push 1
        {OpCode::ADD, 0},            // 2: counter + 1
        {OpCode::DUP, 0},            // 3: Duplicate for comparison
        {OpCode::STORE_VAR, 0},      // 4: Store back to counter
        {OpCode::PUSH_INT, 3},       // 5: Push 3
        {OpCode::GE, 0},             // 6: counter >= 3
        {OpCode::JUMP_IF_NOT, 0},    // 7: If NOT (counter >= 3), jump to 0
        {OpCode::HALT, 0}            // 8: Otherwise halt
    };

    vm.load(program, {"counter"});
    vm.setVariable("counter", NovelMind::i32{0});

    // Execute until halted
    int iterations = 0;
    const int maxIterations = 50; // Safety limit

    while (!vm.isHalted() && iterations < maxIterations) {
        vm.step();
        iterations++;
    }

    // Counter should be 3
    auto val = vm.getVariable("counter");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 3);
    REQUIRE(vm.isHalted());
}

TEST_CASE("VM stack overflow protection", "[scripting][security]")
{
    VirtualMachine vm;

    // Get default stack size limit
    NovelMind::usize defaultLimit = vm.securityGuard().limits().maxStackSize;

    // Create a program that pushes many values onto the stack
    std::vector<Instruction> program;

    // Push values up to and beyond the limit
    for (NovelMind::usize i = 0; i < defaultLimit + 10; ++i) {
        NovelMind::u32 value = static_cast<NovelMind::u32>(i);
        program.push_back({OpCode::PUSH_INT, value});
    }
    program.push_back({OpCode::HALT, 0});

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack overflow before completing
    REQUIRE(vm.isHalted());

    // Check that a security violation was recorded
    REQUIRE(vm.securityGuard().hasViolation());
    auto violation = vm.securityGuard().lastViolation();
    REQUIRE(violation != nullptr);
    REQUIRE(violation->type == NovelMind::scripting::SecurityViolationType::StackOverflow);
}

TEST_CASE("VM stack overflow with custom limit", "[scripting][security]")
{
    VirtualMachine vm;

    // Set a small custom stack size limit
    NovelMind::scripting::VMSecurityLimits limits;
    limits.maxStackSize = 10;
    vm.securityGuard().setLimits(limits);

    // Create a program that pushes 15 values (exceeds limit of 10)
    std::vector<Instruction> program;
    for (NovelMind::u32 i = 0; i < 15; ++i) {
        program.push_back({OpCode::PUSH_INT, i});
    }
    program.push_back({OpCode::HALT, 0});

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack overflow
    REQUIRE(vm.isHalted());
    REQUIRE(vm.securityGuard().hasViolation());
}

TEST_CASE("VM stack within limits", "[scripting][security]")
{
    VirtualMachine vm;

    // Set a reasonable stack limit
    NovelMind::scripting::VMSecurityLimits limits;
    limits.maxStackSize = 100;
    vm.securityGuard().setLimits(limits);

    // Push 50 values (well within limit)
    std::vector<Instruction> program;
    for (NovelMind::u32 i = 0; i < 50; ++i) {
        program.push_back({OpCode::PUSH_INT, i});
    }
    program.push_back({OpCode::HALT, 0});

    vm.load(program, {});
    vm.run();

    // VM should complete successfully
    REQUIRE(vm.isHalted());
    // No security violation should be recorded
    REQUIRE_FALSE(vm.securityGuard().hasViolation());
}

TEST_CASE("VM infinite loop with stack overflow protection", "[scripting][security]")
{
    VirtualMachine vm;

    // Set a small stack limit to trigger overflow quickly
    NovelMind::scripting::VMSecurityLimits limits;
    limits.maxStackSize = 100;
    vm.securityGuard().setLimits(limits);

    // Infinite loop that keeps pushing values (simulates malicious script)
    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},       // 0: Push a value
        {OpCode::JUMP, 0}            // 1: Jump back to 0 (infinite loop)
    };

    vm.load(program, {});

    // Execute many steps - should be stopped by stack overflow
    int iterations = 0;
    const int maxIterations = 1000;

    while (!vm.isHalted() && iterations < maxIterations) {
        vm.step();
        iterations++;
    }

    // VM should have halted due to stack overflow, not max iterations
    REQUIRE(vm.isHalted());
    REQUIRE(vm.securityGuard().hasViolation());
    REQUIRE(iterations < maxIterations);
}

TEST_CASE("VM IP bounds validation - program runs past end", "[scripting][security]")
{
    VirtualMachine vm;

    // Program without HALT - IP will increment past the end
    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 42},      // 0
        {OpCode::STORE_VAR, 0}       // 1 - no HALT, IP will be 2 after this
    };

    vm.load(program, {"result"});

    // First two steps should succeed
    REQUIRE(vm.step());  // Execute instruction 0
    REQUIRE(vm.step());  // Execute instruction 1

    // Third step should fail - IP is now 2, which is >= program.size() (2)
    REQUIRE_FALSE(vm.step());
    REQUIRE(vm.isHalted());

    // Verify the variable was set correctly before halting
    auto val = vm.getVariable("result");
    REQUIRE(std::holds_alternative<NovelMind::i32>(val));
    REQUIRE(std::get<NovelMind::i32>(val) == 42);
}

TEST_CASE("VM IP bounds validation - corrupted IP", "[scripting][security]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},
        {OpCode::NOP, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});

    // Execute first instruction
    REQUIRE(vm.step());  // IP is now 1

    // Manually try to set IP to an invalid value using setIP
    vm.setIP(999);

    // setIP should reject invalid IP (logs warning but doesn't change IP)
    // VM should remain in a valid state at IP=1
    // Step should execute the NOP instruction at IP=1
    REQUIRE(vm.step());  // Execute NOP at IP=1, IP becomes 2
    REQUIRE_FALSE(vm.isHalted());

    // One more step should execute HALT
    REQUIRE_FALSE(vm.step());  // Execute HALT, returns false because halted
    REQUIRE(vm.isHalted());
}

TEST_CASE("VM IP bounds validation - setIP beyond bounds", "[scripting][security]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 1},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});

    // Try to set IP beyond program bounds - setIP should reject this
    vm.setIP(10);  // program.size() is 2

    // setIP rejected the invalid value, so IP should still be 0
    // VM should remain in a valid state and step() should succeed
    REQUIRE(vm.step());
    REQUIRE_FALSE(vm.isHalted());  // Executed PUSH_INT, not halted yet
}

TEST_CASE("VM function call argument order", "[scripting][vm]")
{
    VirtualMachine vm;

    // Test that SHOW_CHARACTER receives arguments in the correct order
    // Push arguments: character ID, then position
    std::vector<Instruction> program = {
        {OpCode::PUSH_STRING, 0},  // Push character ID "hero"
        {OpCode::PUSH_INT, 2},     // Push position (2 = right)
        {OpCode::SHOW_CHARACTER, 0},
        {OpCode::HALT, 0}
    };

    std::vector<std::string> strings = {"hero"};

    vm.load(program, strings);

    // Register callback to capture arguments
    std::vector<NovelMind::scripting::Value> capturedArgs;
    vm.registerCallback(OpCode::SHOW_CHARACTER,
        [&capturedArgs](const std::vector<NovelMind::scripting::Value>& args) {
            capturedArgs = args;
        });

    vm.run();

    // Verify we got 2 arguments in the correct order
    REQUIRE(capturedArgs.size() == 2);

    // First argument should be the character ID
    REQUIRE(std::holds_alternative<std::string>(capturedArgs[0]));
    REQUIRE(std::get<std::string>(capturedArgs[0]) == "hero");

    // Second argument should be the position
    REQUIRE(std::holds_alternative<NovelMind::i32>(capturedArgs[1]));
    REQUIRE(std::get<NovelMind::i32>(capturedArgs[1]) == 2);
}

TEST_CASE("VM multiple arguments order", "[scripting][vm]")
{
    VirtualMachine vm;

    // Test MOVE_CHARACTER with multiple arguments (without custom position)
    std::vector<Instruction> program = {
        {OpCode::PUSH_STRING, 0},  // Push character ID "hero"
        {OpCode::PUSH_INT, 1},     // Push position (1 = center)
        {OpCode::PUSH_INT, 500},   // Push duration (500ms)
        {OpCode::MOVE_CHARACTER, 0},
        {OpCode::HALT, 0}
    };

    std::vector<std::string> strings = {"hero"};

    vm.load(program, strings);

    // Register callback to capture arguments
    std::vector<NovelMind::scripting::Value> capturedArgs;
    vm.registerCallback(OpCode::MOVE_CHARACTER,
        [&capturedArgs](const std::vector<NovelMind::scripting::Value>& args) {
            capturedArgs = args;
        });

    vm.run();

    // Verify arguments are in correct order: id, position, duration
    REQUIRE(capturedArgs.size() == 3);

    // First: character ID
    REQUIRE(std::holds_alternative<std::string>(capturedArgs[0]));
    REQUIRE(std::get<std::string>(capturedArgs[0]) == "hero");

    // Second: position
    REQUIRE(std::holds_alternative<NovelMind::i32>(capturedArgs[1]));
    REQUIRE(std::get<NovelMind::i32>(capturedArgs[1]) == 1);

    // Third: duration
    REQUIRE(std::holds_alternative<NovelMind::i32>(capturedArgs[2]));
    REQUIRE(std::get<NovelMind::i32>(capturedArgs[2]) == 500);
}

// =========================================================================
// Type Coercion Tests for Comparison Operators (Issue #475)
// =========================================================================

TEST_CASE("test_vm_compare_int_float", "[scripting][coercion]")
{
    VirtualMachine vm;

    SECTION("LT: int < float") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 5},
            {OpCode::PUSH_FLOAT, 0},  // Need to encode 10.5f
            {OpCode::LT, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        // Encode 10.5f as operand
        NovelMind::f32 val = 10.5f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program[1].operand = operand;

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);  // 5 < 10.5
    }

    SECTION("GT: float > int") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_FLOAT, 0},  // 10.5f
            {OpCode::PUSH_INT, 5},
            {OpCode::GT, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        NovelMind::f32 val = 10.5f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program[0].operand = operand;

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);  // 10.5 > 5
    }

    SECTION("LE: int <= float (equal values)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 10},
            {OpCode::PUSH_FLOAT, 0},  // 10.0f
            {OpCode::LE, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        NovelMind::f32 val = 10.0f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program[1].operand = operand;

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);  // 10 <= 10.0
    }

    SECTION("GE: float >= int") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_FLOAT, 0},  // 5.5f
            {OpCode::PUSH_INT, 10},
            {OpCode::GE, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        NovelMind::f32 val = 5.5f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program[0].operand = operand;

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == false);  // 5.5 >= 10 is false
    }
}

TEST_CASE("test_vm_compare_string_int", "[scripting][coercion]")
{
    VirtualMachine vm;

    SECTION("LT: string < int (lexicographic)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_STRING, 0},  // "10"
            {OpCode::PUSH_INT, 5},
            {OpCode::LT, 0},
            {OpCode::STORE_VAR, 1},  // "result" is at index 1
            {OpCode::HALT, 0}
        };

        vm.load(program, {"10", "result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        // "10" < "5" lexicographically (because '1' < '5')
        REQUIRE(std::get<bool>(result) == true);
    }

    SECTION("GT: int > string (converts int to string)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 20},
            {OpCode::PUSH_STRING, 0},  // "10"
            {OpCode::GT, 0},
            {OpCode::STORE_VAR, 1},  // "result" is at index 1
            {OpCode::HALT, 0}
        };

        vm.load(program, {"10", "result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        // "20" > "10" lexicographically
        REQUIRE(std::get<bool>(result) == true);
    }

    SECTION("EQ: string == int (converts int to string)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_STRING, 0},  // "42"
            {OpCode::PUSH_INT, 42},
            {OpCode::EQ, 0},
            {OpCode::STORE_VAR, 1},  // "result" is at index 1
            {OpCode::HALT, 0}
        };

        vm.load(program, {"42", "result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);  // "42" == "42"
    }
}

TEST_CASE("VM comparison type coercion - comprehensive", "[scripting][coercion]")
{
    VirtualMachine vm;

    SECTION("bool < bool (as int)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 0},    // false (0)
            {OpCode::PUSH_BOOL, 1},    // true (1)
            {OpCode::LT, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::get<bool>(result) == true);  // false < true (0 < 1)
    }

    SECTION("null < int (null as 0)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_NULL, 0},
            {OpCode::PUSH_INT, 5},
            {OpCode::LT, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::get<bool>(result) == true);  // 0 < 5
    }

    SECTION("int < int") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 3},
            {OpCode::PUSH_INT, 7},
            {OpCode::LT, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::get<bool>(result) == true);  // 3 < 7
    }

    SECTION("string > string (lexicographic)") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_STRING, 0},  // "banana"
            {OpCode::PUSH_STRING, 1},  // "apple"
            {OpCode::GT, 0},
            {OpCode::STORE_VAR, 2},  // "result" is at index 2
            {OpCode::HALT, 0}
        };

        vm.load(program, {"banana", "apple", "result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<bool>(result));
        REQUIRE(std::get<bool>(result) == true);  // "banana" > "apple"
    }

    SECTION("Consistency: EQ and NE type handling") {
        // Test that EQ and comparison operators use same coercion
        std::vector<Instruction> program1 = {
            {OpCode::PUSH_INT, 5},
            {OpCode::PUSH_FLOAT, 0},  // 5.0f
            {OpCode::EQ, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        NovelMind::f32 val = 5.0f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program1[1].operand = operand;

        vm.load(program1, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::get<bool>(result) == true);  // 5 == 5.0
    }
}

// =========================================================================
// Division by Zero Tests (Issue #457)
// =========================================================================

TEST_CASE("test_vm_divide_by_zero_integer", "[scripting][division]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 10},
        {OpCode::PUSH_INT, 0},  // Divisor is zero
        {OpCode::DIV, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    // VM should halt due to division by zero error
    REQUIRE(vm.isHalted());

    // The variable should not be set because the operation halted before STORE_VAR
    REQUIRE_FALSE(vm.hasVariable("result"));
}

TEST_CASE("test_vm_divide_by_zero_float", "[scripting][division]")
{
    VirtualMachine vm;

    SECTION("Float dividend, zero divisor") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_FLOAT, 0},  // 10.5f
            {OpCode::PUSH_FLOAT, 1},  // 0.0f
            {OpCode::DIV, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        // Encode 10.5f as operand
        NovelMind::f32 val1 = 10.5f;
        NovelMind::u32 operand1;
        std::memcpy(&operand1, &val1, sizeof(NovelMind::f32));
        program[0].operand = operand1;

        // Encode 0.0f as operand
        NovelMind::f32 val2 = 0.0f;
        NovelMind::u32 operand2;
        std::memcpy(&operand2, &val2, sizeof(NovelMind::f32));
        program[1].operand = operand2;

        vm.load(program, {"result"});
        vm.run();

        // VM should halt due to division by zero error
        REQUIRE(vm.isHalted());

        // The variable should not be set
        REQUIRE_FALSE(vm.hasVariable("result"));
    }

    SECTION("Integer dividend divided by float zero") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 42},
            {OpCode::PUSH_FLOAT, 0},  // 0.0f
            {OpCode::DIV, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        // Encode 0.0f as operand
        NovelMind::f32 val = 0.0f;
        NovelMind::u32 operand;
        std::memcpy(&operand, &val, sizeof(NovelMind::f32));
        program[1].operand = operand;

        vm.load(program, {"result"});
        vm.run();

        // VM should halt due to division by zero error
        REQUIRE(vm.isHalted());

        // The variable should not be set
        REQUIRE_FALSE(vm.hasVariable("result"));
    }
}

TEST_CASE("test_vm_modulo_by_zero", "[scripting][division]")
{
    VirtualMachine vm;

    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 10},
        {OpCode::PUSH_INT, 0},  // Divisor is zero
        {OpCode::MOD, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    // VM should halt due to modulo by zero error
    REQUIRE(vm.isHalted());

    // The variable should not be set because the operation halted before STORE_VAR
    REQUIRE_FALSE(vm.hasVariable("result"));
}

TEST_CASE("test_vm_division_normal_operations", "[scripting][division]")
{
    VirtualMachine vm;

    SECTION("Integer division - normal case") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 10},
            {OpCode::PUSH_INT, 2},
            {OpCode::DIV, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<NovelMind::i32>(result));
        REQUIRE(std::get<NovelMind::i32>(result) == 5);
    }

    SECTION("Float division - normal case") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_FLOAT, 0},  // 10.0f
            {OpCode::PUSH_FLOAT, 1},  // 2.0f
            {OpCode::DIV, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        NovelMind::f32 val1 = 10.0f;
        NovelMind::u32 operand1;
        std::memcpy(&operand1, &val1, sizeof(NovelMind::f32));
        program[0].operand = operand1;

        NovelMind::f32 val2 = 2.0f;
        NovelMind::u32 operand2;
        std::memcpy(&operand2, &val2, sizeof(NovelMind::f32));
        program[1].operand = operand2;

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<NovelMind::f32>(result));
        REQUIRE(std::get<NovelMind::f32>(result) == 5.0f);
    }

    SECTION("Modulo - normal case") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 10},
            {OpCode::PUSH_INT, 3},
            {OpCode::MOD, 0},
            {OpCode::STORE_VAR, 0},
            {OpCode::HALT, 0}
        };

        vm.load(program, {"result"});
        vm.run();

        auto result = vm.getVariable("result");
        REQUIRE(std::holds_alternative<NovelMind::i32>(result));
        REQUIRE(std::get<NovelMind::i32>(result) == 1);
    }
}

// =========================================================================
// Stack Underflow Tests (Issue #447)
// =========================================================================

TEST_CASE("test_vm_add_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // ADD with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::ADD, 0},  // Stack is empty, need 2 elements
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_add_one_element_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // ADD with only 1 element on stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 5},  // Push 1 element
        {OpCode::ADD, 0},       // Need 2 elements, only have 1
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_subtract_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // SUB with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::SUB, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_multiply_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // MUL with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::MUL, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_divide_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // DIV with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::DIV, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_modulo_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // MOD with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::MOD, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
}

TEST_CASE("test_vm_comparison_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    SECTION("EQ with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::EQ, 0},
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("LT with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::LT, 0},
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("GT with one element") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_INT, 5},
            {OpCode::GT, 0},  // Need 2 elements
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }
}

TEST_CASE("test_vm_logical_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    SECTION("AND with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::AND, 0},
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("OR with one element") {
        std::vector<Instruction> program = {
            {OpCode::PUSH_BOOL, 1},
            {OpCode::OR, 0},  // Need 2 elements
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("NOT with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::NOT, 0},  // Need 1 element
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }
}

TEST_CASE("test_vm_unary_operations_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    SECTION("NEG with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::NEG, 0},
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("POP with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::POP, 0},
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }
}

TEST_CASE("test_vm_store_var_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // STORE_VAR with empty stack should halt with error
    std::vector<Instruction> program = {
        {OpCode::STORE_VAR, 0},  // Need 1 element on stack
        {OpCode::HALT, 0}
    };

    vm.load(program, {"var"});
    vm.run();

    // VM should halt due to stack underflow
    REQUIRE(vm.isHalted());
    // Variable should not be set
    REQUIRE_FALSE(vm.hasVariable("var"));
}

TEST_CASE("test_vm_jump_if_empty_stack", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    SECTION("JUMP_IF with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::JUMP_IF, 2},  // Need 1 element on stack
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }

    SECTION("JUMP_IF_NOT with empty stack") {
        std::vector<Instruction> program = {
            {OpCode::JUMP_IF_NOT, 2},  // Need 1 element on stack
            {OpCode::HALT, 0}
        };
        vm.load(program, {});
        vm.run();
        REQUIRE(vm.isHalted());
    }
}

TEST_CASE("test_vm_normal_operations_after_fix", "[scripting][stack_underflow]")
{
    VirtualMachine vm;

    // Verify that normal operations still work correctly with sufficient stack
    std::vector<Instruction> program = {
        {OpCode::PUSH_INT, 10},
        {OpCode::PUSH_INT, 5},
        {OpCode::ADD, 0},
        {OpCode::PUSH_INT, 3},
        {OpCode::MUL, 0},
        {OpCode::STORE_VAR, 0},
        {OpCode::HALT, 0}
    };

    vm.load(program, {"result"});
    vm.run();

    // VM should complete successfully
    REQUIRE(vm.isHalted());
    auto result = vm.getVariable("result");
    REQUIRE(std::holds_alternative<NovelMind::i32>(result));
    REQUIRE(std::get<NovelMind::i32>(result) == 45);  // (10 + 5) * 3 = 45
}
