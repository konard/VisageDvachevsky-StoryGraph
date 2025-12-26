#include "NovelMind/editor/qt/nm_style_manager.hpp"

namespace NovelMind::editor::qt {

QString NMStyleManager::colorToStyleString(const QColor &color) {
  return QString("rgb(%1, %2, %3)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue());
}

QString NMStyleManager::colorToRgbaString(const QColor &color, int alpha) {
  return QString("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

QString NMStyleManager::getStyleSheet() const {
  const auto &p = m_palette;
  const auto &pa = m_panelAccents;

  // Generate comprehensive stylesheet for premium dark theme
  // Design philosophy: Editor-grade UI that's both beautiful and functional
  return QString(R"(
/* ========================================================================== */
/* DESIGN SYSTEM: NovelMind Editor Visual Language                            */
/* Version: 2.0 - Premium Dark Theme                                          */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Global Reset & Base Styles                                                 */
/* -------------------------------------------------------------------------- */

* {
    color: %1;
    background-color: %2;
    selection-background-color: %3;
    selection-color: %1;
}

/* Focus indicators for accessibility - keyboard users can see focused elements */
*:focus {
    outline: 1px solid %3;
    outline-offset: 1px;
}

/* Remove outline from containers that shouldn't show it */
QMainWindow:focus, QWidget:focus, QFrame:focus, QScrollArea:focus,
QDockWidget:focus, QStackedWidget:focus, QSplitter:focus {
    outline: none;
}

/* -------------------------------------------------------------------------- */
/* QMainWindow - Application Shell                                            */
/* -------------------------------------------------------------------------- */

QMainWindow {
    background: %4;
}

QMainWindow::separator {
    background-color: %5;
    width: 2px;
    height: 2px;
}

QMainWindow::separator:hover {
    background-color: %3;
}

/* ========================================================================== */
/* QDockWidget & Tabs                                                         */
/* ========================================================================== */

QDockWidget {
    background: %2;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
    border: 1px solid %5;
    border-radius: 3px;
}

QDockWidget::title {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
      stop:0 %2, stop:1 %4);
    color: %1;
    padding: 5px 6px;
    border-bottom: 1px solid %5;
    font-weight: 600;
}

QDockWidget::float-button, QDockWidget::close-button {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 2px;
    width: 16px;
    height: 16px;
}

QDockWidget::float-button:hover, QDockWidget::close-button:hover {
    background: %6;
    border-color: %5;
}

QTabBar::tab {
    background: %2;
    color: %1;
    padding: 4px 8px;
    margin-right: 1px;
    border: 1px solid %5;
    border-bottom: 2px solid %5;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}

QTabBar::tab:selected {
    background: %4;
    border-color: %3;
    border-bottom: 2px solid %3;
    font-weight: 600;
}

QTabBar::tab:hover {
    background: %6;
}

QSplitter::handle {
    background: %5;
}

QSplitter::handle:hover {
    background: %3;
}

/* ========================================================================== */
/* QMenuBar                                                                   */
/* ========================================================================== */

QMenuBar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
      stop:0 %2, stop:1 %4);
    border-bottom: 1px solid %5;
    padding: 1px;
}

QMenuBar::item {
    background-color: transparent;
    padding: 3px 6px;
    border-radius: 2px;
}

QMenuBar::item:selected {
    background-color: %6;
}

QMenuBar::item:pressed {
    background-color: %3;
}

/* ========================================================================== */
/* QMenu                                                                      */
/* ========================================================================== */

QMenu {
    background-color: %2;
    border: 1px solid %5;
    padding: 3px;
}

QMenu::item {
    padding: 4px 18px 4px 6px;
    border-radius: 2px;
}

QMenu::item:selected {
    background-color: %6;
}

QMenu::item:disabled {
    color: %7;
}

QMenu::separator {
    height: 1px;
    background-color: %5;
    margin: 3px 6px;
}

QMenu::indicator {
    width: 16px;
    height: 16px;
    margin-left: 4px;
}

/* ========================================================================== */
/* QToolBar                                                                   */
/* ========================================================================== */

QToolBar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
      stop:0 %2, stop:1 %4);
    border: none;
    border-bottom: 1px solid %5;
    padding: 2px;
    spacing: 2px;
}

QToolBar::separator {
    background-color: %5;
    width: 1px;
    margin: 4px 2px;
}

QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 3px 5px;
}

QToolButton:hover {
    background-color: %6;
    border-color: %5;
}

QToolButton:pressed {
    background-color: %8;
}

QToolButton:checked {
    background-color: %3;
    border-color: %3;
}

/* ========================================================================== */
/* QDockWidget                                                                */
/* ========================================================================== */

QDockWidget {
    titlebar-close-icon: url(:/icons/close.svg);
    titlebar-normal-icon: url(:/icons/float.svg);
}

QDockWidget::title {
    background-color: %4;
    padding: 5px 6px;
    border-bottom: 1px solid %5;
    border-left: 3px solid transparent;
    text-align: left;
    font-weight: 600;
}

QDockWidget::close-button,
QDockWidget::float-button {
    background-color: transparent;
    border: none;
    padding: 2px;
}

QDockWidget::close-button:hover,
QDockWidget::float-button:hover {
    background-color: %6;
}

/* ========================================================================== */
/* QTabWidget / QTabBar                                                       */
/* ========================================================================== */

QTabWidget::pane {
    border: 1px solid %5;
    background-color: %4;
}

QTabBar::tab {
    background-color: %2;
    border: 1px solid %5;
    border-bottom: 2px solid transparent;
    padding: 4px 8px;
    margin-right: 1px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}

QTabBar::tab:selected {
    background-color: %4;
    border-color: %3;
    border-bottom: 2px solid %3;
    color: %1;
    font-weight: 600;
}

QTabBar::tab:hover:!selected {
    background-color: %6;
}

/* ========================================================================== */
/* QPushButton                                                                */
/* ========================================================================== */

QPushButton {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 4px;
    padding: 4px 10px;
    min-width: 64px;
    font-weight: 500;
}

QPushButton:hover {
    background-color: %6;
    border-color: %3;
}

QPushButton:pressed {
    background-color: %8;
}

QPushButton:disabled {
    background-color: %4;
    color: %7;
}

QPushButton:default {
    border-color: %3;
}

/* ========================================================================== */
/* QLineEdit                                                                  */
/* ========================================================================== */

QLineEdit {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 3px;
    padding: 3px 6px;
    selection-background-color: %3;
}

QLineEdit:focus {
    border-color: %3;
}

QLineEdit:disabled {
    background-color: %2;
    color: %7;
}

/* ========================================================================== */
/* QTextEdit / QPlainTextEdit                                                 */
/* ========================================================================== */

QTextEdit, QPlainTextEdit {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 3px;
    selection-background-color: %3;
}

QTextEdit:focus, QPlainTextEdit:focus {
    border-color: %3;
}

/* ========================================================================== */
/* QTreeView / QListView / QTableView                                         */
/* ========================================================================== */

QTreeView, QListView, QTableView {
    background-color: %4;
    border: 1px solid %5;
    alternate-background-color: %9;
    selection-background-color: %3;
}

QTreeView::item, QListView::item, QTableView::item {
    padding: 3px;
}

QTreeView::item:hover, QListView::item:hover, QTableView::item:hover {
    background-color: %6;
}

QTreeView::item:selected, QListView::item:selected, QTableView::item:selected {
    background-color: %3;
}

QTreeView::item:selected {
    border-left: 3px solid %11;
}

QListView::item {
    background-color: %2;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px;
}

QListView::item:selected {
    background-color: %6;
    border-color: %3;
}

QTreeView::branch:has-children:!has-siblings:closed,
QTreeView::branch:closed:has-children:has-siblings {
    border-image: none;
    image: url(:/icons/branch-closed.svg);
}

QTreeView::branch:open:has-children:!has-siblings,
QTreeView::branch:open:has-children:has-siblings {
    border-image: none;
    image: url(:/icons/branch-open.svg);
}

QHeaderView::section {
    background-color: %2;
    border: none;
    border-right: 1px solid %5;
    border-bottom: 1px solid %5;
    padding: 4px 8px;
    font-weight: 600;
}

QHeaderView::section:hover {
    background-color: %6;
}

/* ========================================================================== */
/* QScrollBar                                                                 */
/* ========================================================================== */

QScrollBar:vertical {
    background-color: %4;
    width: 10px;
    margin: 0;
}

QScrollBar:horizontal {
    background-color: %4;
    height: 10px;
    margin: 0;
}

QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background-color: %5;
    border-radius: 3px;
    min-height: 24px;
    min-width: 24px;
    margin: 1px;
}

QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background-color: %10;
}

QScrollBar::add-line, QScrollBar::sub-line {
    height: 0;
    width: 0;
}

QScrollBar::add-page, QScrollBar::sub-page {
    background: none;
}

/* ========================================================================== */
/* QSplitter                                                                  */
/* ========================================================================== */

QSplitter::handle {
    background-color: %5;
}

QSplitter::handle:hover {
    background-color: %3;
}

QSplitter::handle:horizontal {
    width: 2px;
}

QSplitter::handle:vertical {
    height: 2px;
}

/* ========================================================================== */
/* QComboBox                                                                  */
/* ========================================================================== */

QComboBox {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 3px;
    padding: 3px 6px;
    min-width: 90px;
}

QComboBox:hover {
    border-color: %3;
}

QComboBox::drop-down {
    border: none;
    width: 18px;
}

QComboBox::down-arrow {
    image: url(:/icons/dropdown.svg);
    width: 12px;
    height: 12px;
}

QComboBox QAbstractItemView {
    background-color: %2;
    border: 1px solid %5;
    selection-background-color: %3;
}

/* ========================================================================== */
/* QSpinBox / QDoubleSpinBox                                                  */
/* ========================================================================== */

QSpinBox, QDoubleSpinBox {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 3px;
    padding: 3px;
}

QSpinBox:focus, QDoubleSpinBox:focus {
    border-color: %3;
}

QSpinBox::up-button, QDoubleSpinBox::up-button {
    border: none;
    background-color: transparent;
    width: 16px;
}

QSpinBox::down-button, QDoubleSpinBox::down-button {
    border: none;
    background-color: transparent;
    width: 16px;
}

/* ========================================================================== */
/* QSlider                                                                    */
/* ========================================================================== */

QSlider::groove:horizontal {
    height: 4px;
    background-color: %5;
    border-radius: 2px;
}

QSlider::handle:horizontal {
    background-color: %3;
    width: 14px;
    height: 14px;
    margin: -5px 0;
    border-radius: 7px;
}

QSlider::handle:horizontal:hover {
    background-color: %11;
}

/* ========================================================================== */
/* QCheckBox                                                                  */
/* ========================================================================== */

QCheckBox {
    spacing: 6px;
    color: %1;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid %5;
    border-radius: 3px;
    background-color: %9;
}

QCheckBox::indicator:hover {
    border-color: %3;
    background-color: %4;
}

QCheckBox::indicator:checked {
    background-color: %3;
    border-color: %3;
    image: url(:/icons/lucide/check.svg);
}

QCheckBox::indicator:checked:hover {
    background-color: %11;
    border-color: %11;
}

QCheckBox::indicator:disabled {
    background-color: %9;
    border-color: %5;
}

QCheckBox:disabled {
    color: %10;
}

/* ========================================================================== */
/* QRadioButton                                                               */
/* ========================================================================== */

QRadioButton {
    spacing: 6px;
    color: %1;
}

QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid %5;
    border-radius: 8px;
    background-color: %9;
}

QRadioButton::indicator:hover {
    border-color: %3;
    background-color: %4;
}

QRadioButton::indicator:checked {
    background-color: %3;
    border-color: %3;
}

QRadioButton::indicator:checked:hover {
    background-color: %11;
    border-color: %11;
}

QRadioButton::indicator:disabled {
    background-color: %9;
    border-color: %5;
}

QRadioButton:disabled {
    color: %10;
}

/* ========================================================================== */
/* QProgressBar                                                               */
/* ========================================================================== */

QProgressBar {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 3px;
    text-align: center;
}

QProgressBar::chunk {
    background-color: %3;
    border-radius: 2px;
}

/* ========================================================================== */
/* QGroupBox                                                                  */
/* ========================================================================== */

QGroupBox {
    border: 1px solid %5;
    border-radius: 3px;
    margin-top: 6px;
    padding-top: 6px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 4px;
    color: %12;
}

/* ========================================================================== */
/* QToolTip                                                                   */
/* ========================================================================== */

QToolTip {
    background-color: %2;
    border: 1px solid %5;
    color: %1;
    padding: 3px 6px;
}

/* ========================================================================== */
/* QStatusBar                                                                 */
/* ========================================================================== */

QStatusBar {
    background-color: %2;
    border-top: 1px solid %5;
}

QStatusBar QLabel#StatusPlay[mode="playing"] {
    color: %14;
    font-weight: 600;
}

QStatusBar QLabel#StatusPlay[mode="paused"] {
    color: %13;
    font-weight: 600;
}

QStatusBar QLabel#StatusPlay[mode="stopped"] {
    color: %10;
}

QStatusBar QLabel#StatusUnsaved[status="dirty"] {
    color: %13;
    font-weight: 600;
}

QStatusBar QLabel#StatusUnsaved[status="clean"] {
    color: %14;
}

QStatusBar::item {
    border: none;
}

/* ========================================================================== */
/* Focus & Accent                                                             */
/* ========================================================================== */

QDockWidget[focusedDock="true"] {
    border: 1px solid %3;
    border-left: 4px solid %3;
}

QDockWidget[focusedDock="true"]::title {
    background-color: %9;
    border-left: 4px solid %3;
}

QDockWidget#SceneViewPanel::title {
    border-left: 4px solid #2ec4b6;
}

QDockWidget#StoryGraphPanel::title {
    border-left: 4px solid #6aa6ff;
}

QDockWidget#InspectorPanel::title {
    border-left: 4px solid #f0b24a;
}

QDockWidget#AssetBrowserPanel::title {
    border-left: 4px solid #5fd18a;
}

QDockWidget#ScriptEditorPanel::title {
    border-left: 4px solid #ff9b66;
}

QDockWidget#ConsolePanel::title {
    border-left: 4px solid #8ea1b5;
}

QDockWidget#PlayToolbarPanel::title {
    border-left: 4px solid #48c76e;
}

QToolBar#MainToolBar {
    background-color: %4;
    border-bottom: 1px solid %5;
    padding: 3px 4px;
}

QToolBar#MainToolBar QToolButton {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 4px;
    padding: 3px 6px;
}

QToolBar#MainToolBar QToolButton:hover {
    background-color: %6;
    border-color: %3;
}

QToolBar#SceneViewToolBar,
QToolBar#StoryGraphToolBar,
QToolBar#AssetBrowserToolBar,
QToolBar#HierarchyToolBar,
QToolBar#ConsoleToolBar,
QToolBar#DebugOverlayToolBar {
    background-color: %4;
    border-bottom: 1px solid %5;
}

QToolBar QLineEdit {
    background-color: %9;
    border-radius: 8px;
    padding: 3px 8px;
}

QToolBar#PlayControlBar {
    background-color: %4;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 3px;
}

QToolBar#PlayControlBar QPushButton {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 4px;
    padding: 3px 8px;
}

QPushButton#PlayButton,
QPushButton#PauseButton,
QPushButton#StopButton,
QPushButton#StepButton {
    font-weight: 600;
    color: %1;
}

QPushButton#PlayButton {
    background-color: #48c76e;
    border-color: #3aa65c;
}

QPushButton#PlayButton:hover {
    background-color: #5bdc7f;
}

QPushButton#PauseButton {
    background-color: #f0b24a;
    border-color: #d79a39;
}

QPushButton#PauseButton:hover {
    background-color: #f5c26a;
}

QPushButton#StopButton {
    background-color: #e3564a;
    border-color: #c9473e;
}

QPushButton#StopButton:hover {
    background-color: #ef6c62;
}

QPushButton#StepButton {
    background-color: #4f9df7;
    border-color: #3c7cc6;
}

QPushButton#StepButton:hover {
    background-color: #66b0ff;
}

QPushButton#PrimaryActionButton,
QPushButton#NMPrimaryButton {
    background-color: %3;
    border-color: %3;
    color: %1;
    font-weight: 600;
}

QPushButton#PrimaryActionButton:hover,
QPushButton#NMPrimaryButton:hover {
    background-color: %11;
    border-color: %11;
}

QPushButton#SecondaryActionButton,
QPushButton#NMSecondaryButton {
    background-color: %9;
    border-color: %5;
}

QLabel#SectionTitle {
    color: %10;
    font-weight: 600;
    padding: 3px 0;
}

QLabel#SceneViewFontWarning {
    background-color: %4;
    border: 1px solid %5;
    color: %13;
    padding: 6px 10px;
    border-radius: 6px;
}

NMPropertyGroup {
    background-color: %4;
    border: 1px solid %5;
    border-radius: 6px;
}

NMPropertyGroup #InspectorGroupHeader {
    background-color: %2;
    border-bottom: 1px solid %5;
    padding: 4px 6px;
}

NMPropertyGroup #InspectorGroupHeader:hover {
    background-color: %6;
}

NMPropertyGroup #InspectorGroupContent {
    background-color: %4;
}

QLabel#InspectorHeader {
    background-color: %2;
    border-bottom: 1px solid %5;
    padding: 6px;
}

QLabel#InspectorEmptyState {
    color: %10;
}

QFrame#AssetPreviewFrame {
    background-color: %4;
    border: 1px solid %5;
    border-radius: 6px;
}

QFrame#ScenePaletteDropArea {
    background-color: %4;
    border: 1px dashed %5;
    border-radius: 6px;
}

QDockWidget#ScenePalettePanel QToolButton {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 6px;
}

QDockWidget#ScenePalettePanel QToolButton:hover {
    background-color: %6;
    border-color: %3;
}

/* ========================================================================== */
/* Command Palette                                                           */
/* ========================================================================== */

QDialog#CommandPalette {
    background-color: %2;
    border: 1px solid %5;
    border-radius: 8px;
}

QLineEdit#CommandPaletteInput {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 6px;
    padding: 4px 8px;
}

QListWidget#CommandPaletteList {
    background-color: %4;
    border: 1px solid %5;
    border-radius: 6px;
}

QListWidget#CommandPaletteList::item {
    padding: 4px 6px;
    border-radius: 4px;
}

QListWidget#CommandPaletteList::item:selected {
    background-color: %3;
}

/* ========================================================================== */
/* Panel-Specific Accent Styling (Visual Identity)                            */
/* ========================================================================== */

/* Scene View Panel - Teal accent */
QDockWidget#SceneViewPanel {
    border-left: 3px solid %15;
}
QDockWidget#SceneViewPanel::title {
    border-left: 3px solid %15;
}

/* Story Graph Panel - Blue accent */
QDockWidget#StoryGraphPanel {
    border-left: 3px solid %16;
}
QDockWidget#StoryGraphPanel::title {
    border-left: 3px solid %16;
}

/* Inspector Panel - Orange/Gold accent */
QDockWidget#InspectorPanel {
    border-left: 3px solid %17;
}
QDockWidget#InspectorPanel::title {
    border-left: 3px solid %17;
}

/* Asset Browser Panel - Green accent */
QDockWidget#AssetBrowserPanel {
    border-left: 3px solid %18;
}
QDockWidget#AssetBrowserPanel::title {
    border-left: 3px solid %18;
}

/* Script Editor Panel - Coral accent */
QDockWidget#ScriptEditorPanel {
    border-left: 3px solid %19;
}
QDockWidget#ScriptEditorPanel::title {
    border-left: 3px solid %19;
}

/* Console Panel - Gray-blue accent */
QDockWidget#ConsolePanel {
    border-left: 3px solid %20;
}
QDockWidget#ConsolePanel::title {
    border-left: 3px solid %20;
}

/* Timeline Panel - Purple accent */
QDockWidget#TimelinePanel {
    border-left: 3px solid %21;
}
QDockWidget#TimelinePanel::title {
    border-left: 3px solid %21;
}

/* Curve Editor Panel - Pink accent */
QDockWidget#CurveEditorPanel {
    border-left: 3px solid %22;
}
QDockWidget#CurveEditorPanel::title {
    border-left: 3px solid %22;
}

/* Voice Manager Panel - Cyan accent */
QDockWidget#VoiceManagerPanel {
    border-left: 3px solid %23;
}
QDockWidget#VoiceManagerPanel::title {
    border-left: 3px solid %23;
}

/* Localization Panel - Yellow accent */
QDockWidget#LocalizationPanel {
    border-left: 3px solid %24;
}
QDockWidget#LocalizationPanel::title {
    border-left: 3px solid %24;
}

/* Diagnostics Panel - Red accent */
QDockWidget#DiagnosticsPanel {
    border-left: 3px solid %25;
}
QDockWidget#DiagnosticsPanel::title {
    border-left: 3px solid %25;
}

/* Hierarchy Panel - Lime accent */
QDockWidget#HierarchyPanel {
    border-left: 3px solid %26;
}
QDockWidget#HierarchyPanel::title {
    border-left: 3px solid %26;
}

/* Scene Palette Panel - Lavender accent */
QDockWidget#ScenePalettePanel {
    border-left: 3px solid %27;
}
QDockWidget#ScenePalettePanel::title {
    border-left: 3px solid %27;
}

/* Play Toolbar Panel - Bright green accent */
QDockWidget#PlayToolbarPanel {
    border-left: 3px solid %28;
}
QDockWidget#PlayToolbarPanel::title {
    border-left: 3px solid %28;
}

/* ========================================================================== */
/* Voice Manager Panel - Waveform & Recording Styling                         */
/* ========================================================================== */

QWidget#VoiceWaveform {
    background-color: %4;
    border: 1px solid %5;
    border-radius: 4px;
}

QWidget#VoiceWaveform[recording="true"] {
    border-color: %25;
    background-color: rgba(229, 77, 66, 30);
}

QPushButton#RecordButton {
    background-color: %25;
    border: none;
    border-radius: 20px;
    min-width: 40px;
    min-height: 40px;
}

QPushButton#RecordButton:hover {
    background-color: #ef6c62;
}

QPushButton#RecordButton[recording="true"] {
    background-color: #ff4444;
    animation: pulse 1s infinite;
}

/* ========================================================================== */
/* Timeline Panel - Track & Keyframe Styling                                  */
/* ========================================================================== */

QWidget#TimelineTrack {
    background-color: %4;
    border-bottom: 1px solid %5;
}

QWidget#TimelineTrack:alternate {
    background-color: %9;
}

QWidget#TimelineKeyframe {
    background-color: %3;
    border: 2px solid %11;
    border-radius: 3px;
}

QWidget#TimelineKeyframe:selected {
    background-color: %24;
    border-color: %24;
}

QWidget#TimelinePlayhead {
    background-color: %25;
    width: 2px;
}

/* ========================================================================== */
/* Localization Panel - Translation Table Styling                             */
/* ========================================================================== */

QTableWidget#LocalizationTable {
    background-color: %2;
    gridline-color: %5;
    alternate-background-color: %4;
}

QTableWidget#LocalizationTable::item {
    padding: 4px 8px;
    color: %1;
    background-color: transparent;
}

QTableWidget#LocalizationTable::item:selected {
    background-color: %3;
    color: %1;
}

QTableWidget#LocalizationTable::item[missing="true"] {
    background-color: rgba(245, 166, 35, 40);
    color: %13;
}

QLabel#LocaleKey {
    font-family: monospace;
    color: %10;
}

/* ========================================================================== */
/* Asset Browser - Grid & Preview Styling                                     */
/* ========================================================================== */

QListWidget#AssetGrid {
    background-color: %4;
    border: none;
}

QListWidget#AssetGrid::item {
    background-color: %9;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 8px;
    margin: 4px;
}

QListWidget#AssetGrid::item:hover {
    background-color: %6;
    border-color: %5;
}

QListWidget#AssetGrid::item:selected {
    background-color: %6;
    border-color: %3;
    border-width: 2px;
}

QWidget#AssetThumbnail {
    background-color: %2;
    border-radius: 4px;
}

QLabel#AssetTypeBadge {
    background-color: %3;
    color: %1;
    border-radius: 2px;
    padding: 2px 4px;
    font-size: 9px;
    font-weight: 600;
}

QLabel#AssetErrorBadge {
    background-color: %25;
    color: %1;
    border-radius: 2px;
    padding: 2px 4px;
    font-size: 9px;
}

/* ========================================================================== */
/* Diagnostics Panel - Issue List Styling                                     */
/* ========================================================================== */

QTreeWidget#DiagnosticsTree {
    background-color: %4;
    border: none;
}

QTreeWidget#DiagnosticsTree::item {
    padding: 4px 8px;
    border-radius: 2px;
}

QTreeWidget#DiagnosticsTree::item[severity="error"] {
    color: %25;
}

QTreeWidget#DiagnosticsTree::item[severity="warning"] {
    color: %13;
}

QTreeWidget#DiagnosticsTree::item[severity="info"] {
    color: %3;
}

/* ========================================================================== */
/* Story Graph - Node Styling                                                 */
/* ========================================================================== */

QWidget#GraphNode {
    background-color: %9;
    border: 2px solid %5;
    border-radius: 8px;
}

QWidget#GraphNode:hover {
    border-color: %6;
}

QWidget#GraphNode:selected {
    border-color: %3;
    border-width: 2px;
}

QWidget#GraphNode[type="dialogue"] {
    border-left: 4px solid %16;
}

QWidget#GraphNode[type="choice"] {
    border-left: 4px solid %17;
}

QWidget#GraphNode[type="event"] {
    border-left: 4px solid %21;
}

QWidget#GraphNode[type="condition"] {
    border-left: 4px solid %24;
}

QWidget#GraphNode[type="start"] {
    border-left: 4px solid %28;
}

QWidget#GraphNode[type="end"] {
    border-left: 4px solid %25;
}

/* ========================================================================== */
/* Curve Editor - Canvas Styling                                              */
/* ========================================================================== */

QWidget#CurveEditorCanvas {
    background-color: %4;
    border: 1px solid %5;
}

QWidget#CurvePoint {
    background-color: %3;
    border: 2px solid %1;
    border-radius: 4px;
}

QWidget#CurvePoint:selected {
    background-color: %24;
    border-color: %24;
}

QWidget#CurveTangent {
    background-color: %21;
    border-radius: 3px;
}

/* ========================================================================== */
/* Inspector Panel - Property Groups                                          */
/* ========================================================================== */

QGroupBox#InspectorPropertyGroup {
    background-color: %9;
    border: 1px solid %5;
    border-radius: 6px;
    margin-top: 8px;
    padding: 8px;
}

QGroupBox#InspectorPropertyGroup::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 6px;
    color: %10;
    font-weight: 600;
}

QLabel#PropertyLabel {
    color: %10;
    min-width: 100px;
}

QLabel#PropertyValue {
    color: %1;
}

QPushButton#ResetPropertyButton {
    background-color: transparent;
    border: none;
    padding: 2px;
}

QPushButton#ResetPropertyButton:hover {
    background-color: %6;
    border-radius: 2px;
}

/* ========================================================================== */
/* Empty State Styling                                                        */
/* ========================================================================== */

QLabel#EmptyStateTitle {
    color: %1;
    font-size: 14px;
    font-weight: 600;
}

QLabel#EmptyStateDescription {
    color: %10;
    font-size: 11px;
}

QLabel#EmptyStateIcon {
    color: %7;
}

/* ========================================================================== */
/* Drag & Drop Feedback                                                       */
/* ========================================================================== */

QWidget[dragOver="true"] {
    border: 2px dashed %3;
    background-color: rgba(59, 158, 255, 30);
}

QFrame#DropIndicator {
    background-color: %3;
    border-radius: 2px;
}

)")
      .arg(colorToStyleString(p.textPrimary))   // %1
      .arg(colorToStyleString(p.bgDark))        // %2
      .arg(colorToStyleString(p.accentPrimary)) // %3
      .arg(colorToStyleString(p.bgDarkest))     // %4
      .arg(colorToStyleString(p.borderDefault)) // %5
      .arg(colorToStyleString(p.bgLight))       // %6
      .arg(colorToStyleString(p.textDisabled))  // %7
      .arg(colorToStyleString(p.accentActive))  // %8
      .arg(colorToStyleString(p.bgMedium))      // %9
      .arg(colorToStyleString(p.textSecondary)) // %10
      .arg(colorToStyleString(p.accentHover))   // %11
      .arg(colorToStyleString(p.textSecondary)) // %12
      .arg(colorToStyleString(p.warning))       // %13
      .arg(colorToStyleString(p.success))       // %14
      // Panel accent colors
      .arg(colorToStyleString(pa.sceneView))    // %15
      .arg(colorToStyleString(pa.storyGraph))   // %16
      .arg(colorToStyleString(pa.inspector))    // %17
      .arg(colorToStyleString(pa.assetBrowser)) // %18
      .arg(colorToStyleString(pa.scriptEditor)) // %19
      .arg(colorToStyleString(pa.console))      // %20
      .arg(colorToStyleString(pa.timeline))     // %21
      .arg(colorToStyleString(pa.curveEditor))  // %22
      .arg(colorToStyleString(pa.voiceManager)) // %23
      .arg(colorToStyleString(pa.localization)) // %24
      .arg(colorToStyleString(pa.diagnostics))  // %25
      .arg(colorToStyleString(pa.hierarchy))    // %26
      .arg(colorToStyleString(pa.scenePalette)) // %27
      .arg(colorToStyleString(pa.playToolbar)); // %28
}

} // namespace NovelMind::editor::qt
