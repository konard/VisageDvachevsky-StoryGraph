/**
 * @file test_dock_panel_minimum_sizes.cpp
 * @brief Demonstration test for dock panel minimum size functionality
 *
 * This experiment demonstrates the fix for Issue #18 (Docking problems and
 * UI element overlap). The key changes are:
 *
 * 1. NMDockPanel base class now sets default minimum sizes (200x150)
 * 2. Individual panels can override with setMinimumPanelSize()
 * 3. Content widgets also receive minimum size hints
 *
 * To run this experiment:
 *   cmake --build build --target test_dock_panel_minimum_sizes
 *   ./build/bin/test_dock_panel_minimum_sizes
 *
 * Expected behavior:
 * - Panels cannot be resized below their minimum dimensions
 * - UI elements no longer overlap when panels are docked closely
 * - Text fields do not overlap buttons
 * - Headers do not cover content
 */

#include <QApplication>
#include <QDebug>
#include <QMainWindow>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

// Simulated NMDockPanel minimum size functionality
class TestDockPanel : public QDockWidget {
public:
    explicit TestDockPanel(const QString& title, QWidget* parent = nullptr)
        : QDockWidget(title, parent) {
        // Default minimum size (matching the fix)
        setMinimumSize(200, 150);

        auto* content = new QWidget(this);
        auto* layout = new QVBoxLayout(content);

        // Header
        auto* header = new QLabel(QString("<b>%1</b>").arg(title), content);
        header->setTextFormat(Qt::RichText);
        layout->addWidget(header);

        // Some sample controls that would overlap without minimum sizes
        layout->addWidget(new QLabel("Property 1:", content));
        layout->addWidget(new QLineEdit(content));
        layout->addWidget(new QLabel("Property 2:", content));
        layout->addWidget(new QLineEdit(content));
        layout->addWidget(new QPushButton("Apply", content));
        layout->addStretch();

        setWidget(content);
    }

    void setMinimumPanelSize(int width, int height) {
        setMinimumSize(width, height);
        if (widget()) {
            widget()->setMinimumSize(width - 4, height - 4);
        }
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    qDebug() << "=== Dock Panel Minimum Size Test ===";
    qDebug() << "This test demonstrates the fix for Issue #18";
    qDebug() << "";

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("Dock Panel Minimum Size Test");
    mainWindow.resize(1200, 800);

    // Create test panels with different minimum sizes
    auto* inspectorPanel = new TestDockPanel("Inspector", &mainWindow);
    inspectorPanel->setMinimumPanelSize(280, 200);  // Matches fix

    auto* storyGraphPanel = new TestDockPanel("Story Graph", &mainWindow);
    storyGraphPanel->setMinimumPanelSize(400, 300);  // Matches fix

    auto* consolePanel = new TestDockPanel("Console", &mainWindow);
    consolePanel->setMinimumPanelSize(300, 150);  // Matches fix

    auto* hierarchyPanel = new TestDockPanel("Hierarchy", &mainWindow);
    hierarchyPanel->setMinimumPanelSize(220, 180);  // Matches fix

    // Add panels to main window
    mainWindow.addDockWidget(Qt::LeftDockWidgetArea, hierarchyPanel);
    mainWindow.addDockWidget(Qt::RightDockWidgetArea, inspectorPanel);
    mainWindow.addDockWidget(Qt::TopDockWidgetArea, storyGraphPanel);
    mainWindow.addDockWidget(Qt::BottomDockWidgetArea, consolePanel);

    // Enable docking features
    mainWindow.setDockNestingEnabled(true);
    mainWindow.setDockOptions(QMainWindow::AllowTabbedDocks |
                              QMainWindow::AllowNestedDocks |
                              QMainWindow::AnimatedDocks);

    // Report panel sizes
    qDebug() << "Panel minimum sizes:";
    qDebug() << "  Inspector:" << inspectorPanel->minimumSize();
    qDebug() << "  Story Graph:" << storyGraphPanel->minimumSize();
    qDebug() << "  Console:" << consolePanel->minimumSize();
    qDebug() << "  Hierarchy:" << hierarchyPanel->minimumSize();
    qDebug() << "";
    qDebug() << "Try resizing the panels - they should not go below their minimum sizes.";
    qDebug() << "UI elements should not overlap even when panels are docked closely.";

    mainWindow.show();
    return app.exec();
}
