#include <catch2/catch_test_macros.hpp>

#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/transform.hpp"
#include <cmath>

using namespace NovelMind;
using namespace NovelMind::renderer;

// ============================================================================
// Renderer Color Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Color - Construction", "[renderer][color]") {
  SECTION("Default constructor") {
    Color color;
    // Default color values depend on implementation
  }

  SECTION("RGBA constructor") {
    Color color{255, 128, 64, 200};
    CHECK(color.r == 255);
    CHECK(color.g == 128);
    CHECK(color.b == 64);
    CHECK(color.a == 200);
  }

  SECTION("RGB constructor (full alpha)") {
    Color color{100, 150, 200, 255};
    CHECK(color.r == 100);
    CHECK(color.g == 150);
    CHECK(color.b == 200);
    CHECK(color.a == 255);
  }

  SECTION("Black color") {
    Color black{0, 0, 0, 255};
    CHECK(black.r == 0);
    CHECK(black.g == 0);
    CHECK(black.b == 0);
    CHECK(black.a == 255);
  }

  SECTION("White color") {
    Color white{255, 255, 255, 255};
    CHECK(white.r == 255);
    CHECK(white.g == 255);
    CHECK(white.b == 255);
    CHECK(white.a == 255);
  }

  SECTION("Transparent color") {
    Color transparent{0, 0, 0, 0};
    CHECK(transparent.a == 0);
  }
}

TEST_CASE("Color - Alpha channel", "[renderer][color][alpha]") {
  SECTION("Fully opaque") {
    Color color{100, 100, 100, 255};
    CHECK(color.a == 255);
  }

  SECTION("Semi-transparent") {
    Color color{100, 100, 100, 128};
    CHECK(color.a == 128);
  }

  SECTION("Fully transparent") {
    Color color{100, 100, 100, 0};
    CHECK(color.a == 0);
  }
}

TEST_CASE("Color - Predefined colors", "[renderer][color]") {
  SECTION("Primary colors") {
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};
    Color blue{0, 0, 255, 255};

    CHECK(red.r == 255);
    CHECK(green.g == 255);
    CHECK(blue.b == 255);
  }

  SECTION("Grayscale") {
    Color black{0, 0, 0, 255};
    Color gray{128, 128, 128, 255};
    Color white{255, 255, 255, 255};

    CHECK(black.r == black.g);
    CHECK(black.g == black.b);

    CHECK(gray.r == gray.g);
    CHECK(gray.g == gray.b);

    CHECK(white.r == white.g);
    CHECK(white.g == white.b);
  }
}

// ============================================================================
// Transform Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Transform2D - Construction", "[renderer][transform]") {
  SECTION("Default constructor") {
    Transform2D transform;
    // Default values depend on implementation
    // Typically: position (0,0), scale (1,1), rotation 0
  }

  SECTION("Position and scale") {
    Transform2D transform;
    transform.x = 100.0f;
    transform.y = 200.0f;
    transform.scaleX = 2.0f;
    transform.scaleY = 3.0f;

    CHECK(transform.x == 100.0f);
    CHECK(transform.y == 200.0f);
    CHECK(transform.scaleX == 2.0f);
    CHECK(transform.scaleY == 3.0f);
  }

  SECTION("Rotation") {
    Transform2D transform;
    transform.rotation = 45.0f;

    CHECK(transform.rotation == 45.0f);
  }
}

TEST_CASE("Transform2D - Identity transform", "[renderer][transform]") {
  Transform2D identity;
  identity.x = 0.0f;
  identity.y = 0.0f;
  identity.scaleX = 1.0f;
  identity.scaleY = 1.0f;
  identity.rotation = 0.0f;

  SECTION("Identity position") {
    CHECK(identity.x == 0.0f);
    CHECK(identity.y == 0.0f);
  }

  SECTION("Identity scale") {
    CHECK(identity.scaleX == 1.0f);
    CHECK(identity.scaleY == 1.0f);
  }

  SECTION("Identity rotation") {
    CHECK(identity.rotation == 0.0f);
  }
}

TEST_CASE("Transform2D - Translation", "[renderer][transform]") {
  Transform2D transform;

  SECTION("Translate to positive coordinates") {
    transform.x = 100.0f;
    transform.y = 200.0f;

    CHECK(transform.x == 100.0f);
    CHECK(transform.y == 200.0f);
  }

  SECTION("Translate to negative coordinates") {
    transform.x = -50.0f;
    transform.y = -75.0f;

    CHECK(transform.x == -50.0f);
    CHECK(transform.y == -75.0f);
  }

  SECTION("Translate to zero") {
    transform.x = 100.0f;
    transform.y = 200.0f;
    transform.x = 0.0f;
    transform.y = 0.0f;

    CHECK(transform.x == 0.0f);
    CHECK(transform.y == 0.0f);
  }
}

TEST_CASE("Transform2D - Scale", "[renderer][transform]") {
  Transform2D transform;

  SECTION("Uniform scale") {
    transform.scaleX = 2.0f;
    transform.scaleY = 2.0f;

    CHECK(transform.scaleX == 2.0f);
    CHECK(transform.scaleY == 2.0f);
  }

  SECTION("Non-uniform scale") {
    transform.scaleX = 3.0f;
    transform.scaleY = 0.5f;

    CHECK(transform.scaleX == 3.0f);
    CHECK(transform.scaleY == 0.5f);
  }

  SECTION("Scale less than one") {
    transform.scaleX = 0.5f;
    transform.scaleY = 0.25f;

    CHECK(transform.scaleX == 0.5f);
    CHECK(transform.scaleY == 0.25f);
  }

  SECTION("Negative scale (flip)") {
    transform.scaleX = -1.0f;
    transform.scaleY = 1.0f;

    CHECK(transform.scaleX == -1.0f);
    CHECK(transform.scaleY == 1.0f);
  }

  SECTION("Zero scale") {
    transform.scaleX = 0.0f;
    transform.scaleY = 0.0f;

    CHECK(transform.scaleX == 0.0f);
    CHECK(transform.scaleY == 0.0f);
  }
}

TEST_CASE("Transform2D - Rotation", "[renderer][transform]") {
  Transform2D transform;

  SECTION("90 degree rotation") {
    transform.rotation = 90.0f;
    CHECK(transform.rotation == 90.0f);
  }

  SECTION("180 degree rotation") {
    transform.rotation = 180.0f;
    CHECK(transform.rotation == 180.0f);
  }

  SECTION("270 degree rotation") {
    transform.rotation = 270.0f;
    CHECK(transform.rotation == 270.0f);
  }

  SECTION("360 degree rotation (full circle)") {
    transform.rotation = 360.0f;
    CHECK(transform.rotation == 360.0f);
  }

  SECTION("Negative rotation") {
    transform.rotation = -45.0f;
    CHECK(transform.rotation == -45.0f);
  }

  SECTION("Arbitrary angle") {
    transform.rotation = 37.5f;
    CHECK(transform.rotation == 37.5f);
  }
}

TEST_CASE("Transform2D - Combined transformations", "[renderer][transform]") {
  Transform2D transform;

  SECTION("Translate and scale") {
    transform.x = 100.0f;
    transform.y = 200.0f;
    transform.scaleX = 2.0f;
    transform.scaleY = 3.0f;

    CHECK(transform.x == 100.0f);
    CHECK(transform.y == 200.0f);
    CHECK(transform.scaleX == 2.0f);
    CHECK(transform.scaleY == 3.0f);
  }

  SECTION("Translate and rotate") {
    transform.x = 50.0f;
    transform.y = 75.0f;
    transform.rotation = 45.0f;

    CHECK(transform.x == 50.0f);
    CHECK(transform.y == 75.0f);
    CHECK(transform.rotation == 45.0f);
  }

  SECTION("Scale and rotate") {
    transform.scaleX = 1.5f;
    transform.scaleY = 2.0f;
    transform.rotation = 30.0f;

    CHECK(transform.scaleX == 1.5f);
    CHECK(transform.scaleY == 2.0f);
    CHECK(transform.rotation == 30.0f);
  }

  SECTION("Full transformation") {
    transform.x = 100.0f;
    transform.y = 200.0f;
    transform.scaleX = 2.5f;
    transform.scaleY = 1.5f;
    transform.rotation = 60.0f;

    CHECK(transform.x == 100.0f);
    CHECK(transform.y == 200.0f);
    CHECK(transform.scaleX == 2.5f);
    CHECK(transform.scaleY == 1.5f);
    CHECK(transform.rotation == 60.0f);
  }
}

TEST_CASE("Transform2D - Edge cases", "[renderer][transform]") {
  Transform2D transform;

  SECTION("Very large translation") {
    transform.x = 10000.0f;
    transform.y = 10000.0f;

    CHECK(transform.x == 10000.0f);
    CHECK(transform.y == 10000.0f);
  }

  SECTION("Very large scale") {
    transform.scaleX = 100.0f;
    transform.scaleY = 100.0f;

    CHECK(transform.scaleX == 100.0f);
    CHECK(transform.scaleY == 100.0f);
  }

  SECTION("Very small scale") {
    transform.scaleX = 0.001f;
    transform.scaleY = 0.001f;

    CHECK(transform.scaleX == 0.001f);
    CHECK(transform.scaleY == 0.001f);
  }

  SECTION("Very large rotation") {
    transform.rotation = 720.0f; // Two full rotations

    CHECK(transform.rotation == 720.0f);
  }
}

// ============================================================================
// Color Manipulation Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Color - Component modification", "[renderer][color]") {
  Color color{100, 150, 200, 255};

  SECTION("Modify red channel") {
    color.r = 50;
    CHECK(color.r == 50);
    CHECK(color.g == 150);
    CHECK(color.b == 200);
  }

  SECTION("Modify green channel") {
    color.g = 75;
    CHECK(color.r == 100);
    CHECK(color.g == 75);
    CHECK(color.b == 200);
  }

  SECTION("Modify blue channel") {
    color.b = 225;
    CHECK(color.r == 100);
    CHECK(color.g == 150);
    CHECK(color.b == 225);
  }

  SECTION("Modify alpha channel") {
    color.a = 128;
    CHECK(color.r == 100);
    CHECK(color.g == 150);
    CHECK(color.b == 200);
    CHECK(color.a == 128);
  }

  SECTION("Set all channels to zero") {
    color.r = 0;
    color.g = 0;
    color.b = 0;
    color.a = 0;

    CHECK(color.r == 0);
    CHECK(color.g == 0);
    CHECK(color.b == 0);
    CHECK(color.a == 0);
  }

  SECTION("Set all channels to max") {
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = 255;

    CHECK(color.r == 255);
    CHECK(color.g == 255);
    CHECK(color.b == 255);
    CHECK(color.a == 255);
  }
}

// ============================================================================
// Transform Reset Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Transform2D - Reset to identity", "[renderer][transform]") {
  Transform2D transform;

  // Set to non-identity values
  transform.x = 100.0f;
  transform.y = 200.0f;
  transform.scaleX = 2.0f;
  transform.scaleY = 3.0f;
  transform.rotation = 45.0f;

  // Reset to identity
  transform.x = 0.0f;
  transform.y = 0.0f;
  transform.scaleX = 1.0f;
  transform.scaleY = 1.0f;
  transform.rotation = 0.0f;

  SECTION("Verify reset") {
    CHECK(transform.x == 0.0f);
    CHECK(transform.y == 0.0f);
    CHECK(transform.scaleX == 1.0f);
    CHECK(transform.scaleY == 1.0f);
    CHECK(transform.rotation == 0.0f);
  }
}

// ============================================================================
// Color Boundary Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Color - Boundary values", "[renderer][color]") {
  SECTION("Minimum values") {
    Color color{0, 0, 0, 0};
    CHECK(color.r == 0);
    CHECK(color.g == 0);
    CHECK(color.b == 0);
    CHECK(color.a == 0);
  }

  SECTION("Maximum values") {
    Color color{255, 255, 255, 255};
    CHECK(color.r == 255);
    CHECK(color.g == 255);
    CHECK(color.b == 255);
    CHECK(color.a == 255);
  }

  SECTION("Mixed boundary values") {
    Color color{0, 255, 0, 255};
    CHECK(color.r == 0);
    CHECK(color.g == 255);
    CHECK(color.b == 0);
    CHECK(color.a == 255);
  }
}

// ============================================================================
// Transform Precision Tests (Issue #187 - P1)
// ============================================================================

TEST_CASE("Transform2D - Floating point precision", "[renderer][transform]") {
  Transform2D transform;

  SECTION("Small increments") {
    transform.x = 0.1f;
    transform.y = 0.2f;
    transform.scaleX = 1.01f;
    transform.scaleY = 1.02f;

    CHECK(transform.x > 0.0f);
    CHECK(transform.y > 0.0f);
    CHECK(transform.scaleX > 1.0f);
    CHECK(transform.scaleY > 1.0f);
  }

  SECTION("Fractional values") {
    transform.x = 123.456f;
    transform.y = 789.012f;
    transform.rotation = 12.345f;

    // Allow small floating point error
    CHECK(std::abs(transform.x - 123.456f) < 0.001f);
    CHECK(std::abs(transform.y - 789.012f) < 0.001f);
    CHECK(std::abs(transform.rotation - 12.345f) < 0.001f);
  }
}
