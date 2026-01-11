/**
 * @file test_gizmo_memory.cpp
 * @brief Unit tests for NMTransformGizmo memory management
 *
 * Tests cover:
 * - Memory cleanup in clearGizmo()
 * - Repeated clear operations
 * - Mode switching (which calls clearGizmo)
 * - Destructor cleanup
 * - RAII compliance
 *
 * Related to Issue #477 - Memory leak in clearGizmo
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include <QApplication>
#include <QGraphicsScene>
#include <memory>

using namespace NovelMind::editor::qt;

// Helper class to track object deletion
class DeletionTracker {
public:
    static int allocationCount;
    static int deletionCount;

    static void reset() {
        allocationCount = 0;
        deletionCount = 0;
    }

    static int getLeakCount() {
        return allocationCount - deletionCount;
    }
};

int DeletionTracker::allocationCount = 0;
int DeletionTracker::deletionCount = 0;

// Custom handle class for tracking allocations/deletions
class TrackedGizmoHandle : public QGraphicsEllipseItem {
public:
    TrackedGizmoHandle(QGraphicsItem *parent = nullptr)
        : QGraphicsEllipseItem(parent) {
        DeletionTracker::allocationCount++;
    }

    ~TrackedGizmoHandle() override {
        DeletionTracker::deletionCount++;
    }
};

// =============================================================================
// NMTransformGizmo Memory Management Tests
// =============================================================================

TEST_CASE("NMTransformGizmo clearGizmo cleans up all resources", "[gizmo][memory]")
{
    // Qt requires QApplication for graphics items
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    SECTION("Clear removes all child items") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Gizmo starts with Move mode (created in constructor)
        // Should have multiple child items
        REQUIRE(gizmo->childItems().size() > 0);

        // Clear the gizmo
        gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
        auto childCountBefore = gizmo->childItems().size();

        // Set to same mode should not change child count
        gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
        REQUIRE(gizmo->childItems().size() == childCountBefore);

        delete scene; // This should clean up gizmo and all children
    }

    SECTION("Mode switching properly cleans up old gizmo") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Switch between modes multiple times
        gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
        auto moveChildCount = gizmo->childItems().size();
        REQUIRE(moveChildCount > 0);

        gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);
        auto rotateChildCount = gizmo->childItems().size();
        REQUIRE(rotateChildCount > 0);

        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);
        auto scaleChildCount = gizmo->childItems().size();
        REQUIRE(scaleChildCount > 0);

        // Switch back to Move
        gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
        REQUIRE(gizmo->childItems().size() == moveChildCount);

        delete scene;
    }

    SECTION("Repeated clear operations are safe") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Clear multiple times by switching to same mode
        // This should not cause crashes or undefined behavior
        for (int i = 0; i < 10; ++i) {
            gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
            gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);
        }

        REQUIRE(gizmo->childItems().size() > 0);

        delete scene;
    }
}

TEST_CASE("NMTransformGizmo destructor cleans up all resources", "[gizmo][memory][raii]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    SECTION("Destructor properly cleans up when deleted directly") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Should have children from Move mode (default)
        REQUIRE(gizmo->childItems().size() > 0);

        // Delete gizmo directly
        delete gizmo;

        // Scene should still be valid
        REQUIRE(scene != nullptr);

        delete scene;
    }

    SECTION("Scene deletion cleans up gizmo") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        REQUIRE(gizmo->childItems().size() > 0);

        // Deleting scene should clean up all items including gizmo
        delete scene;
        // If we had memory leaks, valgrind would catch them
    }
}

TEST_CASE("NMTransformGizmo handles all gizmo modes correctly", "[gizmo][modes]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new QGraphicsScene();
    auto *gizmo = new NMTransformGizmo();
    scene->addItem(gizmo);

    SECTION("Move mode creates expected items") {
        gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
        REQUIRE(gizmo->mode() == NMTransformGizmo::GizmoMode::Move);
        // Move mode has: xLine, xHit, xHandle, xArrowHead, yLine, yHit, yHandle,
        // yArrowHead, center, centerHandle = 10 items
        REQUIRE(gizmo->childItems().size() == 10);
    }

    SECTION("Rotate mode creates expected items") {
        gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);
        REQUIRE(gizmo->mode() == NMTransformGizmo::GizmoMode::Rotate);
        // Rotate mode has: rotationRing (custom ring with annular hit testing), handle = 2 items
        REQUIRE(gizmo->childItems().size() == 2);
    }

    SECTION("Scale mode creates expected items") {
        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);
        REQUIRE(gizmo->mode() == NMTransformGizmo::GizmoMode::Scale);
        // Scale mode has: box, 4 corner hit areas, 4 corner handles = 9 items
        REQUIRE(gizmo->childItems().size() == 9);
    }

    delete scene;
}

TEST_CASE("NMTransformGizmo does not leak memory on mode changes", "[gizmo][memory][integration]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new QGraphicsScene();
    auto *gizmo = new NMTransformGizmo();
    scene->addItem(gizmo);

    SECTION("Rapid mode switching does not leak") {
        // Switch modes many times to stress test memory management
        for (int i = 0; i < 100; ++i) {
            gizmo->setMode(NMTransformGizmo::GizmoMode::Move);
            gizmo->setMode(NMTransformGizmo::GizmoMode::Rotate);
            gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);
        }

        // Final mode should be Scale
        REQUIRE(gizmo->mode() == NMTransformGizmo::GizmoMode::Scale);
        REQUIRE(gizmo->childItems().size() == 9);

        delete scene;
        // Valgrind or other memory checkers would catch leaks here
    }
}

TEST_CASE("NMTransformGizmo clearGizmo handles empty gizmo", "[gizmo][edge_cases]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    SECTION("Clearing gizmo multiple times is safe") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Initial state
        REQUIRE(gizmo->childItems().size() > 0);

        // Setting to current mode should be a no-op (early return in setMode)
        auto initialMode = gizmo->mode();
        auto initialChildCount = gizmo->childItems().size();
        gizmo->setMode(initialMode);
        REQUIRE(gizmo->childItems().size() == initialChildCount);

        delete scene;
    }
}

// =============================================================================
// NMTransformGizmo Scale Corner Hit Area Tests (Issue #459)
// =============================================================================

TEST_CASE("NMTransformGizmo scale corner handles have adequate hit areas", "[gizmo][scale][hit_area]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    auto *scene = new QGraphicsScene();
    auto *gizmo = new NMTransformGizmo();
    scene->addItem(gizmo);

    SECTION("Scale mode creates individual hit areas for each corner") {
        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);

        // Scale mode should have: box (1) + corner hit areas (4) + corner handles (4) = 9 items
        auto children = gizmo->childItems();
        REQUIRE(children.size() == 9);

        // Count hit areas and handles separately
        int hitAreaCount = 0;
        int handleCount = 0;

        for (auto *item : children) {
            // QGraphicsRectItem with NoBrush and NoPen is likely a hit area
            auto *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item);
            if (rectItem && rectItem->brush() == Qt::NoBrush && rectItem->pen() == Qt::NoPen) {
                hitAreaCount++;
            }
            // QGraphicsEllipseItem is a handle
            auto *ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
            if (ellipseItem) {
                handleCount++;
            }
        }

        // Should have 4 corner hit areas
        REQUIRE(hitAreaCount >= 4);
        // Should have 4 corner handles
        REQUIRE(handleCount == 4);
    }

    SECTION("Hit areas are larger than visual handles") {
        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);

        auto children = gizmo->childItems();

        // Find a hit area and a handle
        QGraphicsRectItem *hitArea = nullptr;
        QGraphicsEllipseItem *handle = nullptr;

        for (auto *item : children) {
            if (!hitArea) {
                auto *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item);
                if (rectItem && rectItem->brush() == Qt::NoBrush && rectItem->pen() == Qt::NoPen) {
                    // Skip the bounding box (which has a pen)
                    if (rectItem->rect().width() < 150) { // Hit areas are smaller than the full box
                        hitArea = rectItem;
                    }
                }
            }
            if (!handle) {
                handle = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
            }

            if (hitArea && handle) break;
        }

        // Verify we found both
        REQUIRE(hitArea != nullptr);
        REQUIRE(handle != nullptr);

        // Hit area should be larger than handle
        // Hit area is 24px, handle is 16px (at 1.0 scale)
        REQUIRE(hitArea->rect().width() > handle->rect().width());
        REQUIRE(hitArea->rect().height() > handle->rect().height());
    }

    delete scene;
}

TEST_CASE("NMTransformGizmo scale corner handles scale with DPI", "[gizmo][scale][dpi]")
{
    int argc = 0;
    char *argv[] = {nullptr};
    QApplication app(argc, argv);

    SECTION("Corner handles scale proportionally with UI scale") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Get the current UI scale
        double currentScale = NMStyleManager::instance().uiScale();

        // Create scale gizmo
        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);

        auto children = gizmo->childItems();

        // Find a corner handle
        QGraphicsEllipseItem *handle = nullptr;
        for (auto *item : children) {
            handle = qgraphicsitem_cast<QGraphicsEllipseItem*>(item);
            if (handle) break;
        }

        REQUIRE(handle != nullptr);

        // Base handle size is 16px, should be scaled by UI scale
        double expectedSize = 16.0 * currentScale;
        REQUIRE(handle->rect().width() == expectedSize);
        REQUIRE(handle->rect().height() == expectedSize);

        delete scene;
    }

    SECTION("Hit areas scale proportionally with UI scale") {
        auto *scene = new QGraphicsScene();
        auto *gizmo = new NMTransformGizmo();
        scene->addItem(gizmo);

        // Get the current UI scale
        double currentScale = NMStyleManager::instance().uiScale();

        // Create scale gizmo
        gizmo->setMode(NMTransformGizmo::GizmoMode::Scale);

        auto children = gizmo->childItems();

        // Find a corner hit area
        QGraphicsRectItem *hitArea = nullptr;
        for (auto *item : children) {
            auto *rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item);
            if (rectItem && rectItem->brush() == Qt::NoBrush && rectItem->pen() == Qt::NoPen) {
                // Skip the bounding box - hit areas are smaller
                if (rectItem->rect().width() < 150) {
                    hitArea = rectItem;
                    break;
                }
            }
        }

        REQUIRE(hitArea != nullptr);

        // Base hit area size is 24px, should be scaled by UI scale
        double expectedSize = 24.0 * currentScale;
        REQUIRE(hitArea->rect().width() == expectedSize);
        REQUIRE(hitArea->rect().height() == expectedSize);

        delete scene;
    }
}
