#include <catch2/catch_test_macros.hpp>
#include "NovelMind/core/timer.hpp"
#include <thread>
#include <chrono>

using namespace NovelMind::core;

TEST_CASE("Timer initial state", "[timer]") {
  Timer timer;

  REQUIRE(timer.getDeltaTime() == 0.0);
  REQUIRE(timer.getElapsedSeconds() >= 0.0);
}

TEST_CASE("Timer tick updates delta time", "[timer]") {
  Timer timer;

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  timer.tick();

  REQUIRE(timer.getDeltaTime() > 0.0);
}

TEST_CASE("Timer elapsed time increases", "[timer]") {
  Timer timer;

  double elapsed1 = timer.getElapsedSeconds();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  double elapsed2 = timer.getElapsedSeconds();

  REQUIRE(elapsed2 > elapsed1);
}

TEST_CASE("Timer reset clears delta time", "[timer]") {
  Timer timer;

  timer.tick();
  timer.reset();

  REQUIRE(timer.getDeltaTime() == 0.0);
}

TEST_CASE("Timer elapsed milliseconds", "[timer]") {
  Timer timer;

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  double ms = timer.getElapsedMilliseconds();

  REQUIRE(ms >= 9.0); // Allow some tolerance
}
