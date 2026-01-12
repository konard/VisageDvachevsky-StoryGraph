#pragma once

/**
 * @file asset_browser_preview.hpp
 * @brief Asset preview utilities for the Asset Browser
 *
 * Provides:
 * - Preview generation for images
 * - Preview metadata display
 * - Preview update and clearing
 */

#include <QFrame>
#include <QLabel>
#include <QString>

namespace NovelMind::editor::qt {

/**
 * @brief Preview frame widget for displaying asset previews
 */
class NMAssetPreviewFrame : public QFrame {
  Q_OBJECT

public:
  explicit NMAssetPreviewFrame(QWidget *parent = nullptr);
  ~NMAssetPreviewFrame() override = default;

  /**
   * @brief Update the preview with the given asset path
   */
  void updatePreview(const QString &path);

  /**
   * @brief Clear the current preview
   */
  void clearPreview();

  /**
   * @brief Get the currently previewed path
   */
  [[nodiscard]] QString previewPath() const { return m_previewPath; }

private:
  void setupUi();

  QLabel *m_previewImage = nullptr;
  QLabel *m_previewName = nullptr;
  QLabel *m_previewMeta = nullptr;
  QString m_previewPath;
};

} // namespace NovelMind::editor::qt
