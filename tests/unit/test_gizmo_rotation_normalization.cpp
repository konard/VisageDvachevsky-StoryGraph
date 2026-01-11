/**
 * @file test_gizmo_rotation_normalization.cpp
 * @brief Unit tests for NMTransformGizmo rotation normalization
 *
 * Tests cover:
 * - Rotation normalization to 0-360 range
 * - Continuous rotation without accumulation
 * - Negative rotation handling
 * - Boundary conditions (0°, 360°, 720°, etc.)
 * - Floating-point precision at large values
 * - Scene setObjectRotation normalization
 *
 * Related to Issue #452 - Gizmo rotation accumulates beyond 360 degrees
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <cmath>
#include <memory>

using namespace NovelMind::editor::qt;
using Catch::Matchers::WithinAbs;

// Helper function to normalize rotation for testing
qreal normalizeRotation(qreal degrees) {
    qreal normalized = std::fmod(degrees, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized;
}

// =============================================================================
// NMTransformGizmo Rotation Normalization Tests
// =============================================================================

TEST_CASE("NMTransformGizmo normalizes rotation to 0-360 range", "[gizmo][rotation][normalization]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new NMSceneGraphicsScene();
    auto *view = new QGraphicsView(scene);

    SECTION("Positive rotation beyond 360 degrees is normalized") {
        auto *obj = new NMSceneObject("test_obj", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Set rotation to 450 degrees (should normalize to 90)
        scene->setObjectRotation("test_obj", 450.0);
        qreal rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(90.0, 0.001));

        // Set rotation to 720 degrees (should normalize to 0)
        scene->setObjectRotation("test_obj", 720.0);
        rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(0.0, 0.001));

        // Set rotation to 1080 degrees (should normalize to 0)
        scene->setObjectRotation("test_obj", 1080.0);
        rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(0.0, 0.001));
    }

    SECTION("Negative rotation is normalized to positive 0-360 range") {
        auto *obj = new NMSceneObject("test_obj2", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Set rotation to -90 degrees (should normalize to 270)
        scene->setObjectRotation("test_obj2", -90.0);
        qreal rotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(rotation, WithinAbs(270.0, 0.001));

        // Set rotation to -180 degrees (should normalize to 180)
        scene->setObjectRotation("test_obj2", -180.0);
        rotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(rotation, WithinAbs(180.0, 0.001));

        // Set rotation to -450 degrees (should normalize to 270)
        scene->setObjectRotation("test_obj2", -450.0);
        rotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(rotation, WithinAbs(270.0, 0.001));
    }

    SECTION("Boundary values are handled correctly") {
        auto *obj = new NMSceneObject("test_obj3", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Exactly 0 degrees
        scene->setObjectRotation("test_obj3", 0.0);
        qreal rotation = scene->getObjectRotation("test_obj3");
        REQUIRE_THAT(rotation, WithinAbs(0.0, 0.001));

        // Exactly 360 degrees (should normalize to 0)
        scene->setObjectRotation("test_obj3", 360.0);
        rotation = scene->getObjectRotation("test_obj3");
        REQUIRE_THAT(rotation, WithinAbs(0.0, 0.001));

        // Just below 360 degrees
        scene->setObjectRotation("test_obj3", 359.9);
        rotation = scene->getObjectRotation("test_obj3");
        REQUIRE_THAT(rotation, WithinAbs(359.9, 0.001));
    }

    SECTION("Very large rotation values are normalized") {
        auto *obj = new NMSceneObject("test_obj4", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // 3600 degrees (10 full rotations, should normalize to 0)
        scene->setObjectRotation("test_obj4", 3600.0);
        qreal rotation = scene->getObjectRotation("test_obj4");
        REQUIRE_THAT(rotation, WithinAbs(0.0, 0.001));

        // 3690 degrees (should normalize to 90)
        scene->setObjectRotation("test_obj4", 3690.0);
        rotation = scene->getObjectRotation("test_obj4");
        REQUIRE_THAT(rotation, WithinAbs(90.0, 0.001));

        // Very large negative value
        scene->setObjectRotation("test_obj4", -3690.0);
        rotation = scene->getObjectRotation("test_obj4");
        REQUIRE_THAT(rotation, WithinAbs(270.0, 0.001));
    }

    delete view;
    delete scene;
}

TEST_CASE("NMTransformGizmo continuous rotation does not accumulate", "[gizmo][rotation][continuous]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new NMSceneGraphicsScene();
    auto *view = new QGraphicsView(scene);
    auto *gizmo = new NMTransformGizmo();
    scene->addItem(gizmo);

    SECTION("Repeated 90-degree rotations stay within 0-360 range") {
        auto *obj = new NMSceneObject("test_obj", NMSceneObjectType::Character);
        scene->addSceneObject(obj);
        gizmo->setTargetObjectId("test_obj");
        gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);

        // Simulate multiple rotation operations
        // Start at 0, add 90 four times -> should cycle back to 0
        for (int i = 0; i < 4; ++i) {
            qreal currentRotation = scene->getObjectRotation("test_obj");
            scene->setObjectRotation("test_obj", currentRotation + 90.0);
        }

        qreal finalRotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(finalRotation, WithinAbs(0.0, 0.001));
    }

    SECTION("Repeated 45-degree rotations stay within 0-360 range") {
        auto *obj = new NMSceneObject("test_obj2", NMSceneObjectType::Character);
        scene->addSceneObject(obj);
        gizmo->setTargetObjectId("test_obj2");
        gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);

        // Simulate 16 rotations of 45 degrees (2 full rotations)
        for (int i = 0; i < 16; ++i) {
            qreal currentRotation = scene->getObjectRotation("test_obj2");
            scene->setObjectRotation("test_obj2", currentRotation + 45.0);
        }

        qreal finalRotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(finalRotation, WithinAbs(0.0, 0.001));
    }

    SECTION("Many small rotations do not cause precision loss") {
        auto *obj = new NMSceneObject("test_obj3", NMSceneObjectType::Character);
        scene->addSceneObject(obj);
        gizmo->setTargetObjectId("test_obj3");
        gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);

        // Simulate 360 rotations of 1 degree
        for (int i = 0; i < 360; ++i) {
            qreal currentRotation = scene->getObjectRotation("test_obj3");
            scene->setObjectRotation("test_obj3", currentRotation + 1.0);
        }

        qreal finalRotation = scene->getObjectRotation("test_obj3");
        // Allow slightly larger tolerance for accumulated floating-point errors
        REQUIRE_THAT(finalRotation, WithinAbs(0.0, 0.01));
    }

    delete view;
    delete scene;
}

TEST_CASE("NMTransformGizmo rotation precision with large values", "[gizmo][rotation][precision]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new NMSceneGraphicsScene();
    auto *view = new QGraphicsView(scene);

    SECTION("Rotation precision is maintained after normalization") {
        auto *obj = new NMSceneObject("test_obj", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Test that fractional degrees are preserved
        scene->setObjectRotation("test_obj", 45.123);
        qreal rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(45.123, 0.001));

        // Test with value beyond 360
        scene->setObjectRotation("test_obj", 405.123);
        rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(45.123, 0.001));

        // Test with large value
        scene->setObjectRotation("test_obj", 3645.123);
        rotation = scene->getObjectRotation("test_obj");
        REQUIRE_THAT(rotation, WithinAbs(45.123, 0.001));
    }

    SECTION("Very small fractional rotations are preserved") {
        auto *obj = new NMSceneObject("test_obj2", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        scene->setObjectRotation("test_obj2", 0.001);
        qreal rotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(rotation, WithinAbs(0.001, 0.0001));

        scene->setObjectRotation("test_obj2", 359.999);
        rotation = scene->getObjectRotation("test_obj2");
        REQUIRE_THAT(rotation, WithinAbs(359.999, 0.001));
    }

    delete view;
    delete scene;
}

TEST_CASE("NMSceneGraphicsScene setObjectRotation normalization", "[scene][rotation][normalization]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new NMSceneGraphicsScene();
    auto *view = new QGraphicsView(scene);

    SECTION("setObjectRotation normalizes input values") {
        auto *obj = new NMSceneObject("test_obj", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Test various input values
        struct TestCase {
            qreal input;
            qreal expected;
        };

        std::vector<TestCase> testCases = {
            {0.0, 0.0},
            {90.0, 90.0},
            {180.0, 180.0},
            {270.0, 270.0},
            {360.0, 0.0},
            {450.0, 90.0},
            {720.0, 0.0},
            {-90.0, 270.0},
            {-180.0, 180.0},
            {-270.0, 90.0},
            {-360.0, 0.0},
            {-450.0, 270.0},
            {1000.0, 280.0},
            {-1000.0, 80.0}
        };

        for (const auto& testCase : testCases) {
            scene->setObjectRotation("test_obj", testCase.input);
            qreal rotation = scene->getObjectRotation("test_obj");
            REQUIRE_THAT(rotation, WithinAbs(testCase.expected, 0.001));
        }
    }

    SECTION("setObjectRotation handles edge cases") {
        auto *obj = new NMSceneObject("test_obj2", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        // Exactly 0
        REQUIRE(scene->setObjectRotation("test_obj2", 0.0));
        REQUIRE_THAT(scene->getObjectRotation("test_obj2"), WithinAbs(0.0, 0.001));

        // Very close to 360
        REQUIRE(scene->setObjectRotation("test_obj2", 359.9999));
        qreal rotation = scene->getObjectRotation("test_obj2");
        REQUIRE(rotation >= 0.0);
        REQUIRE(rotation < 360.0);

        // Exactly 360 (should become 0)
        REQUIRE(scene->setObjectRotation("test_obj2", 360.0));
        REQUIRE_THAT(scene->getObjectRotation("test_obj2"), WithinAbs(0.0, 0.001));
    }

    SECTION("setObjectRotation returns false for non-existent object") {
        REQUIRE_FALSE(scene->setObjectRotation("non_existent", 90.0));
    }

    delete view;
    delete scene;
}

TEST_CASE("Rotation normalization does not affect other transforms", "[gizmo][rotation][isolation]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new NMSceneGraphicsScene();
    auto *view = new QGraphicsView(scene);

    SECTION("Rotation normalization preserves position") {
        auto *obj = new NMSceneObject("test_obj", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        obj->setPos(100.0, 200.0);
        scene->setObjectRotation("test_obj", 450.0);

        REQUIRE_THAT(obj->pos().x(), WithinAbs(100.0, 0.001));
        REQUIRE_THAT(obj->pos().y(), WithinAbs(200.0, 0.001));
    }

    SECTION("Rotation normalization preserves scale") {
        auto *obj = new NMSceneObject("test_obj2", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        obj->setScaleXY(2.0, 3.0);
        scene->setObjectRotation("test_obj2", -450.0);

        REQUIRE_THAT(obj->scaleX(), WithinAbs(2.0, 0.001));
        REQUIRE_THAT(obj->scaleY(), WithinAbs(3.0, 0.001));
    }

    SECTION("Rotation normalization preserves opacity") {
        auto *obj = new NMSceneObject("test_obj3", NMSceneObjectType::Character);
        scene->addSceneObject(obj);

        obj->setOpacity(0.5);
        scene->setObjectRotation("test_obj3", 720.0);

        REQUIRE_THAT(obj->opacity(), WithinAbs(0.5, 0.001));
    }

    delete view;
    delete scene;
}
