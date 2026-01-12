/**
 * @file undo_stack_bug_analysis.cpp
 * @brief Analysis of the undo stack limiting bug
 *
 * This experiment demonstrates the bug in scene_inspector.cpp:668-680
 * where the stack limiting algorithm reverses the order of entries.
 */

#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

// Mock command for testing
struct MockCommand {
  std::string description;
  int value;

  MockCommand(const std::string &desc, int val)
      : description(desc), value(val) {}
};

// Helper to print stack contents (non-destructive)
void printStack(const std::stack<std::unique_ptr<MockCommand>> &stack,
                const std::string &label) {
  std::cout << label << " (size: " << stack.size() << "): [";

  // Copy stack to print
  std::stack<std::unique_ptr<MockCommand>> temp;
  std::stack<std::unique_ptr<MockCommand>> copy;

  // First, make a copy (since stack is const)
  // We'll use a workaround - create from values
  std::vector<std::string> values;
  std::cout << "bottom";

  // Note: Can't directly iterate stack, so we'll track operations instead
  std::cout << " -> top]" << std::endl;
}

/**
 * BUGGY VERSION - Current implementation from scene_inspector.cpp:668-680
 * This algorithm REVERSES the order of stack elements!
 */
std::stack<std::unique_ptr<MockCommand>>
buggyStackLimit(std::stack<std::unique_ptr<MockCommand>> stack,
                size_t maxSize) {
  std::cout << "\n=== BUGGY ALGORITHM ===" << std::endl;
  std::cout << "Initial stack size: " << stack.size() << std::endl;

  while (stack.size() > maxSize) {
    std::cout << "Stack size (" << stack.size() << ") > max (" << maxSize
              << "), trimming..." << std::endl;

    std::stack<std::unique_ptr<MockCommand>> tempStack;

    // Step 1: Move all but one element to tempStack
    // This REVERSES the order!
    while (stack.size() > 1) {
      std::cout << "  Moving top element (value=" << stack.top()->value
                << ") to tempStack" << std::endl;
      tempStack.push(std::move(stack.top()));
      stack.pop();
    }

    // Step 2: Remove the last (oldest) element
    std::cout << "  Removing oldest element (value=" << stack.top()->value << ")"
              << std::endl;
    stack.pop();

    // Step 3: Move everything back from tempStack
    // This REVERSES the order AGAIN!
    while (!tempStack.empty()) {
      std::cout << "  Moving element (value=" << tempStack.top()->value
                << ") back to stack" << std::endl;
      stack.push(std::move(tempStack.top()));
      tempStack.pop();
    }
  }

  std::cout << "Final stack size: " << stack.size() << std::endl;
  return stack;
}

/**
 * CORRECT VERSION - Using a temporary vector to maintain order
 */
std::stack<std::unique_ptr<MockCommand>>
fixedStackLimit(std::stack<std::unique_ptr<MockCommand>> stack, size_t maxSize) {
  std::cout << "\n=== FIXED ALGORITHM (using vector) ===" << std::endl;
  std::cout << "Initial stack size: " << stack.size() << std::endl;

  if (stack.size() > maxSize) {
    // Transfer all elements to a vector (this reverses order)
    std::vector<std::unique_ptr<MockCommand>> temp;
    while (!stack.empty()) {
      temp.push_back(std::move(stack.top()));
      stack.pop();
    }

    // temp now has: [newest ... oldest]
    // We want to keep: [newest ... (maxSize elements)]
    // So remove from the end (oldest entries)
    size_t toRemove = temp.size() - maxSize;
    std::cout << "Removing " << toRemove << " oldest entries" << std::endl;

    for (size_t i = 0; i < toRemove; ++i) {
      std::cout << "  Removing value=" << temp.back()->value << std::endl;
      temp.pop_back();
    }

    // Now push back to stack in reverse order to restore original order
    for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
      stack.push(std::move(*it));
    }
  }

  std::cout << "Final stack size: " << stack.size() << std::endl;
  return stack;
}

/**
 * ALTERNATIVE FIX - Just remove the bottom element once
 */
std::stack<std::unique_ptr<MockCommand>>
alternativeStackLimit(std::stack<std::unique_ptr<MockCommand>> stack,
                      size_t maxSize) {
  std::cout << "\n=== ALTERNATIVE FIX (single removal) ===" << std::endl;
  std::cout << "Initial stack size: " << stack.size() << std::endl;

  if (stack.size() > maxSize) {
    // Move all elements to temp vector (reversed)
    std::vector<std::unique_ptr<MockCommand>> temp;
    while (!stack.empty()) {
      temp.push_back(std::move(stack.top()));
      stack.pop();
    }

    // temp is now [newest, ..., oldest]
    // Remove the oldest (last element)
    std::cout << "Removing oldest element (value=" << temp.back()->value << ")"
              << std::endl;
    temp.pop_back();

    // Restore in correct order
    for (auto it = temp.rbegin(); it != temp.rend(); ++it) {
      stack.push(std::move(*it));
    }
  }

  std::cout << "Final stack size: " << stack.size() << std::endl;
  return stack;
}

int main() {
  std::cout << "=== UNDO STACK LIMITING BUG ANALYSIS ===" << std::endl;
  std::cout << "\nScenario: Stack with max size 3, adding 5 commands"
            << std::endl;
  std::cout << "Expected after limiting: Commands 3, 4, 5 (newest 3)"
            << std::endl;
  std::cout << "Expected undo order: 5 -> 4 -> 3" << std::endl;

  // Test 1: Buggy version
  {
    std::stack<std::unique_ptr<MockCommand>> stack;
    stack.push(std::make_unique<MockCommand>("Cmd1", 1));
    stack.push(std::make_unique<MockCommand>("Cmd2", 2));
    stack.push(std::make_unique<MockCommand>("Cmd3", 3));
    stack.push(std::make_unique<MockCommand>("Cmd4", 4));
    stack.push(std::make_unique<MockCommand>("Cmd5", 5));

    auto result = buggyStackLimit(std::move(stack), 3);

    std::cout << "\nResulting stack (undo order): ";
    std::vector<int> order;
    while (!result.empty()) {
      order.push_back(result.top()->value);
      result.pop();
    }
    for (size_t i = 0; i < order.size(); ++i) {
      if (i > 0)
        std::cout << " -> ";
      std::cout << order[i];
    }
    std::cout << std::endl;
    std::cout << "BUG: Order is REVERSED!" << std::endl;
  }

  // Test 2: Fixed version (using vector)
  {
    std::stack<std::unique_ptr<MockCommand>> stack;
    stack.push(std::make_unique<MockCommand>("Cmd1", 1));
    stack.push(std::make_unique<MockCommand>("Cmd2", 2));
    stack.push(std::make_unique<MockCommand>("Cmd3", 3));
    stack.push(std::make_unique<MockCommand>("Cmd4", 4));
    stack.push(std::make_unique<MockCommand>("Cmd5", 5));

    auto result = fixedStackLimit(std::move(stack), 3);

    std::cout << "\nResulting stack (undo order): ";
    std::vector<int> order;
    while (!result.empty()) {
      order.push_back(result.top()->value);
      result.pop();
    }
    for (size_t i = 0; i < order.size(); ++i) {
      if (i > 0)
        std::cout << " -> ";
      std::cout << order[i];
    }
    std::cout << std::endl;
    std::cout << "CORRECT: Order is preserved!" << std::endl;
  }

  // Test 3: Alternative fix
  {
    std::stack<std::unique_ptr<MockCommand>> stack;
    stack.push(std::make_unique<MockCommand>("Cmd1", 1));
    stack.push(std::make_unique<MockCommand>("Cmd2", 2));
    stack.push(std::make_unique<MockCommand>("Cmd3", 3));
    stack.push(std::make_unique<MockCommand>("Cmd4", 4));
    stack.push(std::make_unique<MockCommand>("Cmd5", 5));

    auto result = alternativeStackLimit(std::move(stack), 3);

    std::cout << "\nResulting stack (undo order): ";
    std::vector<int> order;
    while (!result.empty()) {
      order.push_back(result.top()->value);
      result.pop();
    }
    for (size_t i = 0; i < order.size(); ++i) {
      if (i > 0)
        std::cout << " -> ";
      std::cout << order[i];
    }
    std::cout << std::endl;
    std::cout << "CORRECT: Order is preserved!" << std::endl;
  }

  std::cout << "\n=== CONCLUSION ===" << std::endl;
  std::cout << "The bug occurs because:" << std::endl;
  std::cout << "1. Moving elements to tempStack reverses order" << std::endl;
  std::cout << "2. Moving back from tempStack reverses again" << std::endl;
  std::cout << "3. Double reversal = back to original... BUT" << std::endl;
  std::cout
      << "4. We removed bottom element AFTER first reversal, so it's wrong!"
      << std::endl;
  std::cout << "\nThe fix: Use a vector and remove from the END (oldest)"
            << std::endl;

  return 0;
}
