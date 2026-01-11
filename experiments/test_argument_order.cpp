// Experiment to verify the argument order issue
// This demonstrates the problem: when we pop arguments from stack,
// they come out in reverse order from how they were pushed

#include <iostream>
#include <vector>
#include <algorithm>

struct Value {
    int data;
};

int main() {
    std::vector<Value> stack;

    // Simulate pushing arguments for a function call foo(1, 2, 3)
    // Arguments are pushed in order: arg0, arg1, arg2
    std::cout << "Pushing arguments 1, 2, 3 onto stack...\n";
    stack.push_back({1});  // arg0
    stack.push_back({2});  // arg1
    stack.push_back({3});  // arg2

    std::cout << "Stack (bottom to top): ";
    for (const auto& v : stack) {
        std::cout << v.data << " ";
    }
    std::cout << "\n\n";

    // Method 1: Pop arguments directly (WRONG - reverse order!)
    std::cout << "Method 1: Pop directly (WRONG)\n";
    std::vector<Value> args_wrong;
    std::vector<Value> temp_stack = stack;  // Copy for demonstration

    auto popArg = [&temp_stack]() -> Value {
        Value val = temp_stack.back();
        temp_stack.pop_back();
        return val;
    };

    args_wrong.push_back(popArg());  // Gets 3
    args_wrong.push_back(popArg());  // Gets 2
    args_wrong.push_back(popArg());  // Gets 1

    std::cout << "  Args array: ";
    for (const auto& v : args_wrong) {
        std::cout << v.data << " ";
    }
    std::cout << "  <- WRONG ORDER!\n\n";

    // Method 2: Pop into temp array, then reverse (CORRECT)
    std::cout << "Method 2: Pop into temp, then reverse (CORRECT)\n";
    std::vector<Value> args_correct;
    temp_stack = stack;  // Reset

    for (int i = 0; i < 3; i++) {
        args_correct.push_back(popArg());
    }

    std::cout << "  Before reverse: ";
    for (const auto& v : args_correct) {
        std::cout << v.data << " ";
    }
    std::cout << "\n";

    std::reverse(args_correct.begin(), args_correct.end());

    std::cout << "  After reverse:  ";
    for (const auto& v : args_correct) {
        std::cout << v.data << " ";
    }
    std::cout << "  <- CORRECT ORDER!\n\n";

    // Method 3: Manual ordering (what SHOW_CHARACTER does - fragile)
    std::cout << "Method 3: Manual ordering (fragile but works for fixed args)\n";
    std::vector<Value> args_manual;
    temp_stack = stack;  // Reset

    Value arg2 = popArg();  // Pop last argument first
    Value arg1 = popArg();
    Value arg0 = popArg();

    args_manual.push_back(arg0);  // Push in correct order
    args_manual.push_back(arg1);
    args_manual.push_back(arg2);

    std::cout << "  Args array: ";
    for (const auto& v : args_manual) {
        std::cout << v.data << " ";
    }
    std::cout << "  <- Works but fragile!\n";

    return 0;
}
