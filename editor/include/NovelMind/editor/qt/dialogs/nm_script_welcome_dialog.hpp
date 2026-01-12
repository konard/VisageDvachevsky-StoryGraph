#pragma once

/**
 * @file nm_script_welcome_dialog.hpp
 * @brief Welcome dialog for Script Editor first-time users
 *
 * Provides onboarding and feature discovery for new Script Editor users with:
 * - Interactive feature tour
 * - Quick start guide access
 * - Sample script loading
 * - Keyboard shortcut reference
 * - Feature highlights and tips
 */

#include <QDialog>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QCheckBox;
class QLabel;
class QScrollArea;

namespace NovelMind::editor::qt {

/**
 * @brief Welcome dialog shown on first launch of Script Editor
 *
 * This dialog provides a friendly introduction to the Script Editor's
 * powerful IDE features, helping users discover capabilities like:
 * - Minimap navigation
 * - Code folding and breadcrumbs
 * - Snippets (Ctrl+J) and Command Palette (Ctrl+Shift+P)
 * - Go-to-definition (F12) and Find References (Shift+F12)
 * - Find/Replace (Ctrl+F / Ctrl+H)
 * - And much more!
 */
class NMScriptWelcomeDialog : public QDialog {
  Q_OBJECT

public:
  /**
   * @brief Construct the Script Editor welcome dialog
   * @param parent Parent widget
   */
  explicit NMScriptWelcomeDialog(QWidget* parent = nullptr);

  /**
   * @brief Destructor
   */
  ~NMScriptWelcomeDialog() override;

  /**
   * @brief Check if user wants to skip the welcome screen in future
   */
  [[nodiscard]] bool shouldSkipInFuture() const { return m_skipInFuture; }

  /**
   * @brief Get the selected sample script to load
   * @return Empty string if no sample selected, otherwise sample script ID
   */
  [[nodiscard]] QString selectedSample() const { return m_selectedSample; }

signals:
  /**
   * @brief Emitted when user requests to take the feature tour
   */
  void takeTourRequested();

  /**
   * @brief Emitted when user requests to view the quick start guide
   */
  void viewQuickStartRequested();

  /**
   * @brief Emitted when user requests to open a sample script
   * @param sampleId ID of the sample script (e.g., "basic", "choices", "advanced")
   */
  void openSampleRequested(const QString& sampleId);

public slots:
  /**
   * @brief Show keyboard shortcuts reference
   */
  void showKeyboardShortcuts();

private slots:
  void onTakeTourClicked();
  void onQuickStartClicked();
  void onSampleClicked(const QString& sampleId);
  void onKeyboardShortcutsClicked();

private:
  void setupUI();
  void setupHeader();
  void setupMainContent();
  void setupFooter();
  void styleDialog();
  QWidget* createFeatureCard(const QString& icon, const QString& title, const QString& description,
                             const QString& shortcut = QString());
  QWidget* createSampleCard(const QString& title, const QString& description,
                            const QString& sampleId);
  QWidget* createShortcutRow(const QString& action, const QString& shortcut);

  // UI Components
  QWidget* m_headerWidget = nullptr;
  QWidget* m_contentWidget = nullptr;
  QScrollArea* m_scrollArea = nullptr;
  QWidget* m_footerWidget = nullptr;
  QCheckBox* m_skipCheckbox = nullptr;
  QPushButton* m_takeTourBtn = nullptr;
  QPushButton* m_quickStartBtn = nullptr;
  QPushButton* m_closeBtn = nullptr;

  // State
  bool m_skipInFuture = false;
  QString m_selectedSample;

  // Constants
  static constexpr int DIALOG_WIDTH = 900;
  static constexpr int DIALOG_HEIGHT = 700;
};

} // namespace NovelMind::editor::qt
