#pragma once

#include <QColor>
#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QString>

namespace NovelMind::editor::qt {

/**
 * @brief Icon manager for the NovelMind Editor
 *
 * Provides centralized icon management with SVG support, caching,
 * and automatic color theming for dark/light modes.
 *
 * Icons are created as SVG strings and rendered at runtime with
 * proper color theming.
 */
class NMIconManager {
public:
  static NMIconManager &instance();

  /**
   * @brief Get an icon by name
   * @param iconName Name of the icon (e.g., "file-new", "edit-undo")
   * @param size Icon size in pixels (default: 16)
   * @param color Icon color (if empty, uses theme color)
   * @return QIcon instance
   */
  QIcon getIcon(const QString &iconName, int size = 16,
                const QColor &color = QColor());

  /**
   * @brief Get a pixmap by icon name
   * @param iconName Name of the icon
   * @param size Pixmap size in pixels
   * @param color Icon color (if empty, uses theme color)
   * @return QPixmap instance
   */
  QPixmap getPixmap(const QString &iconName, int size = 16,
                    const QColor &color = QColor());

  /**
   * @brief Clear icon cache (useful when theme changes)
   */
  void clearCache();

  /**
   * @brief Set default icon color for the current theme
   */
  void setDefaultColor(const QColor &color);

  /**
   * @brief Get default icon color
   */
  QColor getDefaultColor() const { return m_defaultColor; }

private:
  NMIconManager();
  ~NMIconManager() = default;

  NMIconManager(const NMIconManager &) = delete;
  NMIconManager &operator=(const NMIconManager &) = delete;

  QString getSvgData(const QString &iconName);
  QString loadSvgFromResource(const QString &resourcePath);
  QPixmap renderSvg(const QString &svgData, int size, const QColor &color);

  // DEPRECATED: Old hardcoded SVG data (kept for fallback compatibility)
  QMap<QString, QString> m_iconSvgData;

  // Icon name to resource file path mapping (Lucide icons)
  QMap<QString, QString> m_iconFilePaths;

  QMap<QString, QIcon> m_iconCache;
  QColor m_defaultColor;

  void initializeIcons();
};

} // namespace NovelMind::editor::qt
