#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "NovelMind/scene/animation.hpp"
#include "NovelMind/renderer/color.hpp"

using namespace NovelMind;
using namespace NovelMind::scene;

TEST_CASE("Easing - Linear returns input", "[animation][easing]") {
  CHECK(ease(EaseType::Linear, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::Linear, 0.5f) == Catch::Approx(0.5f));
  CHECK(ease(EaseType::Linear, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseInQuad starts slow", "[animation][easing]") {
  f32 quarter = ease(EaseType::EaseInQuad, 0.25f);
  f32 half = ease(EaseType::EaseInQuad, 0.5f);

  // Quadratic ease-in: t^2
  CHECK(quarter == Catch::Approx(0.0625f));
  CHECK(half == Catch::Approx(0.25f));
  CHECK(ease(EaseType::EaseInQuad, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInQuad, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseOutQuad ends slow", "[animation][easing]") {
  f32 quarter = ease(EaseType::EaseOutQuad, 0.25f);
  f32 half = ease(EaseType::EaseOutQuad, 0.5f);

  // Should start fast
  CHECK(quarter > 0.25f);
  CHECK(half > 0.5f);
  CHECK(ease(EaseType::EaseOutQuad, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseOutQuad, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseInOutQuad symmetric around 0.5", "[animation][easing]") {
  f32 quarter = ease(EaseType::EaseInOutQuad, 0.25f);
  f32 threeQuarter = ease(EaseType::EaseInOutQuad, 0.75f);

  // Should be symmetric: f(0.25) + f(0.75) ≈ 1
  CHECK((quarter + threeQuarter) == Catch::Approx(1.0f).margin(0.001f));
  CHECK(ease(EaseType::EaseInOutQuad, 0.5f) == Catch::Approx(0.5f));
}

TEST_CASE("Easing - Clamps input to [0, 1]", "[animation][easing]") {
  // Negative input
  CHECK(ease(EaseType::Linear, -1.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInQuad, -0.5f) == Catch::Approx(0.0f));

  // Over 1 input
  CHECK(ease(EaseType::Linear, 2.0f) == Catch::Approx(1.0f));
  CHECK(ease(EaseType::EaseOutQuad, 1.5f) == Catch::Approx(1.0f));
}

TEST_CASE("FloatTween - Basic value interpolation", "[animation][tween]") {
  f32 target = 0.0f;
  FloatTween tween(&target, 0.0f, 100.0f, 1.0f);

  tween.start();
  CHECK(target == Catch::Approx(0.0f));

  tween.update(0.5);
  CHECK(target == Catch::Approx(50.0f).margin(1.0f));

  tween.update(0.5);
  CHECK(target == Catch::Approx(100.0f).margin(1.0f));
  CHECK(tween.isComplete());
}

TEST_CASE("FloatTween - Easing affects interpolation", "[animation][tween]") {
  f32 linear = 0.0f;
  f32 easeIn = 0.0f;

  FloatTween linearTween(&linear, 0.0f, 100.0f, 1.0f, EaseType::Linear);
  FloatTween easeInTween(&easeIn, 0.0f, 100.0f, 1.0f, EaseType::EaseInQuad);

  linearTween.start();
  easeInTween.start();

  linearTween.update(0.5);
  easeInTween.update(0.5);

  // Linear should be at 50
  CHECK(linear == Catch::Approx(50.0f).margin(1.0f));

  // Ease-in should be less than linear at midpoint (starts slow)
  CHECK(easeIn < linear);
}

TEST_CASE("FloatTween - Loop support", "[animation][tween]") {
  f32 target = 0.0f;
  FloatTween tween(&target, 0.0f, 100.0f, 1.0f);
  tween.setLoops(2);

  tween.start();

  // Complete first loop
  tween.update(1.0);
  CHECK_FALSE(tween.isComplete());

  // Complete second loop
  tween.update(1.0);
  CHECK(tween.isComplete());
}

TEST_CASE("FloatTween - Yoyo mode", "[animation][tween]") {
  f32 target = 0.0f;
  FloatTween tween(&target, 0.0f, 100.0f, 1.0f);
  tween.setLoops(2).setYoyo(true);

  tween.start();

  // Progress forward at halfway
  tween.update(0.5);
  CHECK(target == Catch::Approx(50.0f).margin(1.0f));

  // Complete first loop - enters yoyo (backward) phase
  tween.update(0.5);

  // In yoyo mode, after completing forward, direction reverses
  // Now animating backward from 100 to 0
  tween.update(0.5);
  CHECK(target == Catch::Approx(50.0f).margin(5.0f));
}

TEST_CASE("FloatTween - Completion callback", "[animation][tween]") {
  f32 target = 0.0f;
  bool callbackCalled = false;

  FloatTween tween(&target, 0.0f, 100.0f, 1.0f);
  tween.onComplete([&callbackCalled]() { callbackCalled = true; });

  tween.start();
  tween.update(1.0);

  CHECK(callbackCalled);
}

TEST_CASE("FloatTween - Pause and resume", "[animation][tween]") {
  f32 target = 0.0f;
  FloatTween tween(&target, 0.0f, 100.0f, 1.0f);

  tween.start();
  tween.update(0.5);
  f32 valueAtPause = target;

  tween.pause();
  tween.update(0.5); // Should not advance while paused
  CHECK(target == Catch::Approx(valueAtPause));

  tween.resume();
  tween.update(0.5);
  CHECK(target > valueAtPause);
}

TEST_CASE("PositionTween - 2D position interpolation", "[animation][tween]") {
  f32 x = 0.0f;
  f32 y = 0.0f;

  PositionTween tween(&x, &y, 0.0f, 0.0f, 100.0f, 200.0f, 1.0f);

  tween.start();
  tween.update(0.5);

  CHECK(x == Catch::Approx(50.0f).margin(1.0f));
  CHECK(y == Catch::Approx(100.0f).margin(1.0f));

  tween.update(0.5);

  CHECK(x == Catch::Approx(100.0f).margin(1.0f));
  CHECK(y == Catch::Approx(200.0f).margin(1.0f));
}

TEST_CASE("ColorTween - RGBA interpolation", "[animation][tween]") {
  using namespace NovelMind::renderer;

  Color target(0, 0, 0, 255);
  Color from(0, 0, 0, 255);
  Color to(255, 128, 64, 255);

  ColorTween tween(&target, from, to, 1.0f);

  tween.start();
  tween.update(0.5);

  CHECK(target.r == 127); // ~255 * 0.5
  CHECK(target.g == 64);  // ~128 * 0.5
  CHECK(target.b == 32);  // ~64 * 0.5
}

TEST_CASE("CallbackTween - Custom update function", "[animation][tween]") {
  f32 customValue = 0.0f;

  CallbackTween tween(
      [&customValue](f32 progress) {
        customValue = progress * progress; // Quadratic
      },
      1.0f);

  tween.start();
  tween.update(0.5);

  // At 50% time, value should be 0.25 (0.5 squared)
  CHECK(customValue == Catch::Approx(0.25f).margin(0.01f));
}

TEST_CASE("AnimationTimeline - Sequential execution", "[animation][timeline]") {
  f32 value1 = 0.0f;
  f32 value2 = 0.0f;

  AnimationTimeline timeline;
  timeline.append(std::make_unique<FloatTween>(&value1, 0.0f, 100.0f, 1.0f))
      .append(std::make_unique<FloatTween>(&value2, 0.0f, 100.0f, 1.0f));

  timeline.start();

  // First tween
  timeline.update(1.0);
  CHECK(value1 == Catch::Approx(100.0f).margin(1.0f));
  CHECK(value2 == Catch::Approx(0.0f));

  // Second tween
  timeline.update(1.0);
  CHECK(value2 == Catch::Approx(100.0f).margin(1.0f));
}

TEST_CASE("AnimationTimeline - Delay between animations", "[animation][timeline]") {
  f32 value = 0.0f;

  AnimationTimeline timeline;
  timeline.append(std::make_unique<FloatTween>(&value, 0.0f, 50.0f, 1.0f))
      .delay(0.5f)
      .append(std::make_unique<FloatTween>(&value, 50.0f, 100.0f, 1.0f));

  timeline.start();

  timeline.update(1.0); // First tween complete
  CHECK(value == Catch::Approx(50.0f).margin(1.0f));

  timeline.update(0.5); // Delay
  CHECK(value == Catch::Approx(50.0f).margin(1.0f));

  timeline.update(1.0); // Second tween
  CHECK(value == Catch::Approx(100.0f).margin(1.0f));
}

TEST_CASE("AnimationTimeline - Completion callback", "[animation][timeline]") {
  f32 value = 0.0f;
  bool complete = false;

  AnimationTimeline timeline;
  timeline.append(std::make_unique<FloatTween>(&value, 0.0f, 100.0f, 1.0f))
      .onComplete([&complete]() { complete = true; });

  timeline.start();
  timeline.update(1.0);

  // May need one more update cycle to trigger completion
  timeline.update(0.01);

  CHECK(complete);
}

TEST_CASE("AnimationManager - Tracks multiple animations", "[animation][manager]") {
  f32 value1 = 0.0f;
  f32 value2 = 0.0f;

  AnimationManager manager;
  manager.add("anim1", std::make_unique<FloatTween>(&value1, 0.0f, 100.0f, 1.0f));
  manager.add("anim2", std::make_unique<FloatTween>(&value2, 0.0f, 100.0f, 2.0f));

  CHECK(manager.count() == 2);
  CHECK(manager.has("anim1"));
  CHECK(manager.has("anim2"));

  manager.update(1.0);

  CHECK(value1 == Catch::Approx(100.0f).margin(1.0f));
  CHECK(value2 == Catch::Approx(50.0f).margin(1.0f));

  // First animation should be removed after completion
  manager.update(0.01);
  CHECK(manager.count() == 1);
  CHECK_FALSE(manager.has("anim1"));
  CHECK(manager.has("anim2"));
}

TEST_CASE("AnimationManager - Stop animation by ID", "[animation][manager]") {
  f32 value = 0.0f;

  AnimationManager manager;
  manager.add("test", std::make_unique<FloatTween>(&value, 0.0f, 100.0f, 1.0f));

  CHECK(manager.has("test"));

  manager.stop("test");

  CHECK_FALSE(manager.has("test"));
}

TEST_CASE("AnimationManager - Stop all animations", "[animation][manager]") {
  f32 value1 = 0.0f;
  f32 value2 = 0.0f;

  AnimationManager manager;
  manager.add("anim1", std::make_unique<FloatTween>(&value1, 0.0f, 100.0f, 1.0f));
  manager.add("anim2", std::make_unique<FloatTween>(&value2, 0.0f, 100.0f, 2.0f));

  CHECK(manager.count() == 2);

  manager.stopAll();

  CHECK(manager.count() == 0);
}

// =============================================================================
// Bezier Curve Animation Tests (Issue #130)
// =============================================================================

TEST_CASE("Easing - EaseInCubic starts very slow", "[animation][easing][bezier]") {
  f32 quarter = ease(EaseType::EaseInCubic, 0.25f);
  f32 half = ease(EaseType::EaseInCubic, 0.5f);

  // Cubic ease-in: t^3
  CHECK(quarter == Catch::Approx(0.015625f).margin(0.001f)); // 0.25^3 = 0.015625
  CHECK(half == Catch::Approx(0.125f).margin(0.001f));       // 0.5^3 = 0.125
  CHECK(ease(EaseType::EaseInCubic, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInCubic, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseOutCubic ends very slow", "[animation][easing][bezier]") {
  f32 quarter = ease(EaseType::EaseOutCubic, 0.25f);
  f32 half = ease(EaseType::EaseOutCubic, 0.5f);
  f32 threeQuarter = ease(EaseType::EaseOutCubic, 0.75f);

  // Should start fast and end slow
  CHECK(quarter > 0.25f);
  CHECK(half > 0.5f);
  CHECK(threeQuarter > 0.75f);
  CHECK(ease(EaseType::EaseOutCubic, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseOutCubic, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseInOutCubic symmetric", "[animation][easing][bezier]") {
  f32 quarter = ease(EaseType::EaseInOutCubic, 0.25f);
  f32 threeQuarter = ease(EaseType::EaseInOutCubic, 0.75f);

  // Should be symmetric: f(0.25) + f(0.75) ≈ 1
  CHECK((quarter + threeQuarter) == Catch::Approx(1.0f).margin(0.01f));
  CHECK(ease(EaseType::EaseInOutCubic, 0.5f) == Catch::Approx(0.5f));
}

TEST_CASE("Easing - EaseInBack overshoots", "[animation][easing][bezier]") {
  // EaseInBack starts with a slight overshoot (going negative)
  // At the start, value should go slightly negative before progressing
  f32 early = ease(EaseType::EaseInBack, 0.2f);

  // The curve should overshoot negative at the start
  CHECK(early < 0.2f); // Significantly less than linear

  // But still reach endpoints correctly
  CHECK(ease(EaseType::EaseInBack, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInBack, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseOutBack overshoots at end", "[animation][easing][bezier]") {
  // EaseOutBack overshoots past 1.0 before settling
  f32 late = ease(EaseType::EaseOutBack, 0.8f);

  // The curve should overshoot past linear
  CHECK(late > 0.8f);

  // Endpoints are correct
  CHECK(ease(EaseType::EaseOutBack, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseOutBack, 1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("Easing - EaseInBounce bounces at start", "[animation][easing][bezier]") {
  // EaseInBounce has oscillations at the beginning
  CHECK(ease(EaseType::EaseInBounce, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInBounce, 1.0f) == Catch::Approx(1.0f));

  // Midpoint should be non-trivial
  f32 half = ease(EaseType::EaseInBounce, 0.5f);
  CHECK(half >= 0.0f);
  CHECK(half <= 1.0f);
}

TEST_CASE("Easing - EaseOutBounce bounces at end", "[animation][easing][bezier]") {
  // EaseOutBounce has oscillations at the end (bounces before settling)
  CHECK(ease(EaseType::EaseOutBounce, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseOutBounce, 1.0f) == Catch::Approx(1.0f));

  // Should reach near 1.0 quickly, then bounce
  f32 earlyValue = ease(EaseType::EaseOutBounce, 0.3f);
  CHECK(earlyValue > 0.3f); // Faster than linear
}

TEST_CASE("Easing - EaseInElastic oscillates", "[animation][easing][bezier]") {
  // EaseInElastic has oscillatory behavior
  CHECK(ease(EaseType::EaseInElastic, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseInElastic, 1.0f) == Catch::Approx(1.0f));

  // Early values may go negative (oscillation)
  f32 early = ease(EaseType::EaseInElastic, 0.2f);
  // Just check it's valid (oscillation may cause negative values)
  CHECK(early >= -0.1f); // Allow some oscillation below 0
}

TEST_CASE("Easing - EaseOutElastic oscillates", "[animation][easing][bezier]") {
  CHECK(ease(EaseType::EaseOutElastic, 0.0f) == Catch::Approx(0.0f));
  CHECK(ease(EaseType::EaseOutElastic, 1.0f) == Catch::Approx(1.0f));

  // Mid values may overshoot past 1.0 (oscillation)
  f32 mid = ease(EaseType::EaseOutElastic, 0.5f);
  // Just check it's in a reasonable range
  CHECK(mid > 0.0f);
}

TEST_CASE("Easing - All ease types have correct endpoints", "[animation][easing][bezier]") {
  // Test that all easing functions correctly map 0 -> 0 and 1 -> 1
  std::vector<EaseType> allTypes = {
      EaseType::Linear,          EaseType::EaseInQuad,    EaseType::EaseOutQuad,
      EaseType::EaseInOutQuad,   EaseType::EaseInCubic,   EaseType::EaseOutCubic,
      EaseType::EaseInOutCubic,  EaseType::EaseInSine,    EaseType::EaseOutSine,
      EaseType::EaseInOutSine,   EaseType::EaseInExpo,    EaseType::EaseOutExpo,
      EaseType::EaseInOutExpo,   EaseType::EaseInBack,    EaseType::EaseOutBack,
      EaseType::EaseInOutBack,   EaseType::EaseInBounce,  EaseType::EaseOutBounce,
      EaseType::EaseInOutBounce, EaseType::EaseInElastic, EaseType::EaseOutElastic,
      EaseType::EaseInOutElastic};

  for (EaseType type : allTypes) {
    CHECK(ease(type, 0.0f) == Catch::Approx(0.0f).margin(0.01f));
    CHECK(ease(type, 1.0f) == Catch::Approx(1.0f).margin(0.01f));
  }
}

TEST_CASE("Easing - Monotonic curves are monotonic", "[animation][easing][bezier]") {
  // Monotonic easing functions should always increase
  std::vector<EaseType> monotonicTypes = {EaseType::Linear,         EaseType::EaseInQuad,
                                          EaseType::EaseOutQuad,    EaseType::EaseInOutQuad,
                                          EaseType::EaseInCubic,    EaseType::EaseOutCubic,
                                          EaseType::EaseInOutCubic, EaseType::EaseInSine,
                                          EaseType::EaseOutSine,    EaseType::EaseInOutSine};

  for (EaseType type : monotonicTypes) {
    f32 prev = ease(type, 0.0f);
    for (int i = 1; i <= 10; ++i) {
      f32 t = static_cast<f32>(i) / 10.0f;
      f32 curr = ease(type, t);
      CHECK(curr >= prev); // Should be monotonically increasing
      prev = curr;
    }
  }
}

TEST_CASE("FloatTween - All easing types work", "[animation][tween][bezier]") {
  // Test that FloatTween works with various easing types
  std::vector<EaseType> testTypes = {EaseType::Linear,        EaseType::EaseInQuad,
                                     EaseType::EaseOutCubic,  EaseType::EaseInOutBack,
                                     EaseType::EaseOutBounce, EaseType::EaseInElastic};

  for (EaseType type : testTypes) {
    f32 target = 0.0f;
    FloatTween tween(&target, 0.0f, 100.0f, 1.0f, type);

    tween.start();
    CHECK(target == Catch::Approx(0.0f));

    tween.update(1.0);
    CHECK(target == Catch::Approx(100.0f).margin(1.0f));
    CHECK(tween.isComplete());
  }
}
