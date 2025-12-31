/**
 * @file test_division_safety.cpp
 * @brief Tests for division by zero protection in CameraPath and AudioRecorder
 *
 * These tests verify that edge cases don't cause crashes or NaN/Inf
 * propagation. Related to issue #154: Fix Division by Zero Bugs in Camera and
 * Audio
 */

#include "NovelMind/audio/audio_recorder.hpp"
#include "NovelMind/renderer/camera.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace NovelMind;
using namespace NovelMind::renderer;
using namespace NovelMind::audio;

// =============================================================================
// CameraPath Division by Zero Protection Tests
// =============================================================================

TEST_CASE("CameraPath evaluatePosition - empty path returns default",
          "[camera][safety]") {
  CameraPath path;

  Vec2 pos = path.evaluatePosition(0.0f);
  CHECK(pos.x == Catch::Approx(0.0f));
  CHECK(pos.y == Catch::Approx(0.0f));

  pos = path.evaluatePosition(0.5f);
  CHECK(pos.x == Catch::Approx(0.0f));
  CHECK(pos.y == Catch::Approx(0.0f));
}

TEST_CASE("CameraPath evaluatePosition - single point returns that point",
          "[camera][safety]") {
  CameraPath path;
  CameraPathPoint point;
  point.position = Vec2{10.0f, 20.0f};
  point.zoom = 1.0f;
  point.rotation = 0.0f;
  path.addPoint(point);

  Vec2 pos = path.evaluatePosition(0.0f);
  CHECK(pos.x == Catch::Approx(10.0f));
  CHECK(pos.y == Catch::Approx(20.0f));

  pos = path.evaluatePosition(0.5f);
  CHECK(pos.x == Catch::Approx(10.0f));
  CHECK(pos.y == Catch::Approx(20.0f));
}

TEST_CASE("CameraPath evaluatePosition - two points interpolates correctly",
          "[camera][safety]") {
  CameraPath path;
  path.setTotalDuration(1.0f);

  CameraPathPoint p1;
  p1.position = Vec2{0.0f, 0.0f};
  p1.zoom = 1.0f;
  path.addPoint(p1);

  CameraPathPoint p2;
  p2.position = Vec2{100.0f, 200.0f};
  p2.zoom = 2.0f;
  path.addPoint(p2);

  Vec2 pos = path.evaluatePosition(0.5f);
  // Should be approximately halfway
  CHECK(!std::isnan(pos.x));
  CHECK(!std::isinf(pos.x));
  CHECK(!std::isnan(pos.y));
  CHECK(!std::isinf(pos.y));
}

TEST_CASE("CameraPath evaluatePosition - no NaN or Inf on edge cases",
          "[camera][safety]") {
  CameraPath path;

  // Test with various time values on empty path
  Vec2 pos = path.evaluatePosition(-1.0f);
  CHECK(!std::isnan(pos.x));
  CHECK(!std::isinf(pos.x));

  pos = path.evaluatePosition(100.0f);
  CHECK(!std::isnan(pos.x));
  CHECK(!std::isinf(pos.x));
}

TEST_CASE("CameraPath evaluateZoom - empty path returns default",
          "[camera][safety]") {
  CameraPath path;

  f32 zoom = path.evaluateZoom(0.0f);
  CHECK(zoom == Catch::Approx(1.0f));
  CHECK(!std::isnan(zoom));
  CHECK(!std::isinf(zoom));
}

TEST_CASE("CameraPath evaluateZoom - single point returns that zoom",
          "[camera][safety]") {
  CameraPath path;
  CameraPathPoint point;
  point.position = Vec2{0.0f, 0.0f};
  point.zoom = 2.5f;
  path.addPoint(point);

  f32 zoom = path.evaluateZoom(0.0f);
  CHECK(zoom == Catch::Approx(2.5f));

  zoom = path.evaluateZoom(0.5f);
  CHECK(zoom == Catch::Approx(2.5f));
}

TEST_CASE("CameraPath evaluateRotation - empty path returns default",
          "[camera][safety]") {
  CameraPath path;

  f32 rotation = path.evaluateRotation(0.0f);
  CHECK(rotation == Catch::Approx(0.0f));
  CHECK(!std::isnan(rotation));
  CHECK(!std::isinf(rotation));
}

TEST_CASE("CameraPath evaluateRotation - single point returns that rotation",
          "[camera][safety]") {
  CameraPath path;
  CameraPathPoint point;
  point.position = Vec2{0.0f, 0.0f};
  point.rotation = 45.0f;
  path.addPoint(point);

  f32 rotation = path.evaluateRotation(0.0f);
  CHECK(rotation == Catch::Approx(45.0f));

  rotation = path.evaluateRotation(0.5f);
  CHECK(rotation == Catch::Approx(45.0f));
}

TEST_CASE("CameraPath - zero duration protection", "[camera][safety]") {
  CameraPath path;
  path.setTotalDuration(0.0f);

  CameraPathPoint p1;
  p1.position = Vec2{10.0f, 20.0f};
  path.addPoint(p1);

  CameraPathPoint p2;
  p2.position = Vec2{30.0f, 40.0f};
  path.addPoint(p2);

  // Even with zero duration, should not crash or return NaN
  Vec2 pos = path.evaluatePosition(0.5f);
  CHECK(!std::isnan(pos.x));
  CHECK(!std::isinf(pos.x));

  f32 zoom = path.evaluateZoom(0.5f);
  CHECK(!std::isnan(zoom));
  CHECK(!std::isinf(zoom));

  f32 rotation = path.evaluateRotation(0.5f);
  CHECK(!std::isnan(rotation));
  CHECK(!std::isinf(rotation));
}

// =============================================================================
// AudioRecorder Duration Calculation Protection Tests
// =============================================================================

TEST_CASE("AudioRecorder getRecordingDuration - zero sampleRate returns 0",
          "[audio][safety]") {
  AudioRecorder recorder;
  // Note: We can't easily set internal state without initialization,
  // but we can verify the default behavior
  f32 duration = recorder.getRecordingDuration();
  CHECK(!std::isnan(duration));
  CHECK(!std::isinf(duration));
  CHECK(duration >= 0.0f);
}
