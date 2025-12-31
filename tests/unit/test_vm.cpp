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
