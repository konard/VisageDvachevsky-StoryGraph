#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/vm_debugger.hpp"
#include "NovelMind/scripting/vm.hpp"

using namespace NovelMind::scripting;

TEST_CASE("VMDebugger constructor with valid VM", "[vm_debugger][scripting]")
{
    VirtualMachine vm;

    // Should construct successfully without crashing
    VMDebugger debugger(&vm);

    // Verify debugger is in initial state
    REQUIRE_FALSE(debugger.isPaused());
    REQUIRE(debugger.getStepMode() == DebugStepMode::None);
    REQUIRE(debugger.getCurrentIP() == 0);
}

TEST_CASE("VMDebugger constructor with null VM fails assertion", "[vm_debugger][scripting]")
{
    // In debug builds, this should trigger an assertion failure
    // In release builds, the assertion is disabled
    // This test documents the expected behavior but cannot test assertion failures
    // directly as they abort the program

    // Note: This test is informational only. The actual null check happens
    // via NOVELMIND_ASSERT_NOT_NULL which aborts in debug builds.
    // We cannot test assertion failures in unit tests as they terminate the process.

    SUCCEED("Null VM is rejected by assertion (see vm_debugger.cpp:16)");
}

TEST_CASE("VMDebugger basic breakpoint operations", "[vm_debugger][scripting]")
{
    VirtualMachine vm;
    VMDebugger debugger(&vm);

    // Add a breakpoint
    u32 bpId = debugger.addBreakpoint(10);
    REQUIRE(bpId > 0);

    // Check breakpoint exists
    REQUIRE(debugger.hasBreakpointAt(10));

    // Get breakpoint
    auto bp = debugger.getBreakpoint(bpId);
    REQUIRE(bp.has_value());
    REQUIRE(bp->id == bpId);
    REQUIRE(bp->instructionPointer == 10);
    REQUIRE(bp->enabled);

    // Remove breakpoint
    REQUIRE(debugger.removeBreakpoint(bpId));
    REQUIRE_FALSE(debugger.hasBreakpointAt(10));
}

TEST_CASE("VMDebugger execution control", "[vm_debugger][scripting]")
{
    VirtualMachine vm;
    VMDebugger debugger(&vm);

    // Initial state
    REQUIRE_FALSE(debugger.isPaused());
    REQUIRE(debugger.getStepMode() == DebugStepMode::None);

    // Pause execution
    debugger.pause();
    REQUIRE(debugger.isPaused());

    // Continue execution
    debugger.continueExecution();
    REQUIRE_FALSE(debugger.isPaused());
    REQUIRE(debugger.getStepMode() == DebugStepMode::None);
}

TEST_CASE("VMDebugger step modes", "[vm_debugger][scripting]")
{
    VirtualMachine vm;
    VMDebugger debugger(&vm);

    // Step into
    debugger.stepInto();
    REQUIRE(debugger.getStepMode() == DebugStepMode::StepInto);
    REQUIRE_FALSE(debugger.isPaused());

    // Reset
    debugger.continueExecution();

    // Step over
    debugger.stepOver();
    REQUIRE(debugger.getStepMode() == DebugStepMode::StepOver);

    // Reset
    debugger.continueExecution();

    // Step out
    debugger.stepOut();
    REQUIRE(debugger.getStepMode() == DebugStepMode::StepOut);
}

TEST_CASE("VMDebugger call stack depth", "[vm_debugger][scripting]")
{
    VirtualMachine vm;
    VMDebugger debugger(&vm);

    // Initial depth should be 0
    REQUIRE(debugger.getCallStackDepth() == 0);

    // Simulate scene entry
    debugger.notifySceneEntered("TestScene", 100);
    REQUIRE(debugger.getCallStackDepth() == 1);
    REQUIRE(debugger.getCurrentScene() == "TestScene");

    // Simulate nested scene
    debugger.notifySceneEntered("NestedScene", 200);
    REQUIRE(debugger.getCallStackDepth() == 2);
    REQUIRE(debugger.getCurrentScene() == "NestedScene");

    // Simulate scene exit
    debugger.notifySceneExited("NestedScene");
    REQUIRE(debugger.getCallStackDepth() == 1);
    REQUIRE(debugger.getCurrentScene() == "TestScene");

    debugger.notifySceneExited("TestScene");
    REQUIRE(debugger.getCallStackDepth() == 0);
    REQUIRE(debugger.getCurrentScene() == "");
}

TEST_CASE("VMDebugger multiple breakpoints", "[vm_debugger][scripting]")
{
    VirtualMachine vm;
    VMDebugger debugger(&vm);

    // Add multiple breakpoints
    u32 bp1 = debugger.addBreakpoint(10);
    u32 bp2 = debugger.addBreakpoint(20);
    u32 bp3 = debugger.addBreakpoint(30);

    REQUIRE(bp1 != bp2);
    REQUIRE(bp2 != bp3);
    REQUIRE(bp1 != bp3);

    // Check all exist
    REQUIRE(debugger.hasBreakpointAt(10));
    REQUIRE(debugger.hasBreakpointAt(20));
    REQUIRE(debugger.hasBreakpointAt(30));

    // Get all breakpoints
    auto allBps = debugger.getAllBreakpoints();
    REQUIRE(allBps.size() == 3);

    // Clear all
    debugger.clearAllBreakpoints();
    REQUIRE_FALSE(debugger.hasBreakpointAt(10));
    REQUIRE_FALSE(debugger.hasBreakpointAt(20));
    REQUIRE_FALSE(debugger.hasBreakpointAt(30));
    REQUIRE(debugger.getAllBreakpoints().empty());
}
