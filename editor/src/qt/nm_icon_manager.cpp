#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include <QBuffer>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace NovelMind::editor::qt {

NMIconManager& NMIconManager::instance() {
  static NMIconManager instance;
  return instance;
}

NMIconManager::NMIconManager()
    : m_defaultColor(220, 220, 220) // Light gray for dark theme
{
  initializeIcons();
}

void NMIconManager::initializeIcons() {
  // Initialize icon mapping from current icon names to Lucide icon file paths
  // This allows the codebase to continue using existing icon names while
  // loading professional Lucide icons from the Qt resource system.
  //
  // Icon Pack: Lucide (https://lucide.dev)
  // License: ISC License
  // Total Icons: 146 mapped icons

  // Icon name mapping to Lucide icon files
  // Format: m_iconFilePaths["icon-name"] = ":/icons/lucide/icon-file.svg";

  // Arrow and Navigation
  m_iconFilePaths["arrow-down"] = ":/icons/lucide/arrow-down.svg";
  m_iconFilePaths["arrow-left"] = ":/icons/lucide/arrow-left.svg";
  m_iconFilePaths["arrow-right"] = ":/icons/lucide/arrow-right.svg";
  m_iconFilePaths["arrow-up"] = ":/icons/lucide/arrow-up.svg";
  m_iconFilePaths["chevron-down"] = ":/icons/lucide/chevron-down.svg";
  m_iconFilePaths["chevron-left"] = ":/icons/lucide/chevron-left.svg";
  m_iconFilePaths["chevron-right"] = ":/icons/lucide/chevron-right.svg";
  m_iconFilePaths["chevron-up"] = ":/icons/lucide/chevron-up.svg";

  // Sync Icons
  m_iconFilePaths["sync"] = ":/icons/lucide/arrow-left-right.svg";
  m_iconFilePaths["sync-to-script"] = ":/icons/lucide/arrow-right.svg";
  m_iconFilePaths["sync-to-graph"] = ":/icons/lucide/arrow-left.svg";

  // Asset Type Icons
  m_iconFilePaths["asset-audio"] = ":/icons/lucide/music.svg";
  m_iconFilePaths["asset-folder"] = ":/icons/lucide/folder.svg";
  m_iconFilePaths["asset-font"] = ":/icons/lucide/type.svg";
  m_iconFilePaths["asset-image"] = ":/icons/lucide/image.svg";
  m_iconFilePaths["asset-script"] = ":/icons/lucide/file-code.svg";
  m_iconFilePaths["asset-video"] = ":/icons/lucide/video.svg";

  // Audio Icons
  m_iconFilePaths["audio-mute"] = ":/icons/lucide/volume-x.svg";
  m_iconFilePaths["audio-record"] = ":/icons/lucide/circle.svg";
  m_iconFilePaths["audio-unmute"] = ":/icons/lucide/volume-2.svg";
  m_iconFilePaths["audio-volume-high"] = ":/icons/lucide/volume-2.svg";
  m_iconFilePaths["audio-volume-low"] = ":/icons/lucide/volume-1.svg";
  m_iconFilePaths["audio-waveform"] = ":/icons/lucide/audio-waveform.svg";
  m_iconFilePaths["microphone"] = ":/icons/lucide/mic.svg";
  m_iconFilePaths["microphone-off"] = ":/icons/lucide/mic-off.svg";

  // Debug and Script Inspector Icons
  m_iconFilePaths["check"] = ":/icons/lucide/check.svg";
  m_iconFilePaths["clear"] = ":/icons/lucide/x-circle.svg";
  m_iconFilePaths["flag"] = ":/icons/lucide/flag.svg";
  m_iconFilePaths["history"] = ":/icons/lucide/history.svg";
  m_iconFilePaths["step-into"] = ":/icons/lucide/arrow-down-to-line.svg";
  m_iconFilePaths["step-out"] = ":/icons/lucide/arrow-up-to-line.svg";
  m_iconFilePaths["step-over"] = ":/icons/lucide/arrow-right-to-line.svg";
  m_iconFilePaths["wait"] = ":/icons/lucide/loader.svg";

  // Edit Operations
  m_iconFilePaths["copy"] = ":/icons/lucide/copy.svg";
  m_iconFilePaths["delete"] = ":/icons/lucide/trash-2.svg";
  m_iconFilePaths["edit-copy"] = ":/icons/lucide/copy.svg";
  m_iconFilePaths["edit-cut"] = ":/icons/lucide/scissors.svg";
  m_iconFilePaths["edit-delete"] = ":/icons/lucide/trash-2.svg";
  m_iconFilePaths["edit-paste"] = ":/icons/lucide/clipboard.svg";
  m_iconFilePaths["edit-redo"] = ":/icons/lucide/redo.svg";
  m_iconFilePaths["edit-undo"] = ":/icons/lucide/undo.svg";
  m_iconFilePaths["replace"] = ":/icons/lucide/clipboard.svg";
  m_iconFilePaths["replace-all"] = ":/icons/lucide/clipboard.svg";

  // File Operations
  m_iconFilePaths["file-close"] = ":/icons/lucide/x.svg";
  m_iconFilePaths["file-new"] = ":/icons/lucide/file-plus.svg";
  m_iconFilePaths["file-open"] = ":/icons/lucide/folder-open.svg";
  m_iconFilePaths["file-save"] = ":/icons/lucide/save.svg";

  // Inspector Icons
  m_iconFilePaths["property-bool"] = ":/icons/lucide/toggle-right.svg";
  m_iconFilePaths["property-color"] = ":/icons/lucide/palette.svg";
  m_iconFilePaths["property-link"] = ":/icons/lucide/link.svg";
  m_iconFilePaths["property-number"] = ":/icons/lucide/hash.svg";
  m_iconFilePaths["property-override"] = ":/icons/lucide/circle-dot.svg";
  m_iconFilePaths["property-reset"] = ":/icons/lucide/rotate-ccw.svg";
  m_iconFilePaths["property-text"] = ":/icons/lucide/type.svg";
  m_iconFilePaths["property-vector"] = ":/icons/lucide/arrow-up-right.svg";

  // Layout Icons
  m_iconFilePaths["layout-grid"] = ":/icons/lucide/grid-3x3.svg";
  m_iconFilePaths["layout-list"] = ":/icons/lucide/list.svg";
  m_iconFilePaths["layout-tree"] = ":/icons/lucide/list-tree.svg";

  // Localization Icons
  m_iconFilePaths["language"] = ":/icons/lucide/languages.svg";
  m_iconFilePaths["locale-add"] = ":/icons/lucide/circle-plus.svg";
  m_iconFilePaths["locale-key"] = ":/icons/lucide/key.svg";
  m_iconFilePaths["locale-missing"] = ":/icons/lucide/circle-alert.svg";
  m_iconFilePaths["translate"] = ":/icons/lucide/languages.svg";

  // Node Type Icons
  m_iconFilePaths["node-choice"] = ":/icons/lucide/git-branch.svg";
  m_iconFilePaths["node-condition"] = ":/icons/lucide/diamond.svg";
  m_iconFilePaths["node-dialogue"] = ":/icons/lucide/message-square.svg";
  m_iconFilePaths["node-end"] = ":/icons/lucide/circle-stop.svg";
  m_iconFilePaths["node-event"] = ":/icons/lucide/zap.svg";
  m_iconFilePaths["node-jump"] = ":/icons/lucide/corner-down-right.svg";
  m_iconFilePaths["node-random"] = ":/icons/lucide/shuffle.svg";
  m_iconFilePaths["node-start"] = ":/icons/lucide/circle-play.svg";
  m_iconFilePaths["node-variable"] = ":/icons/lucide/variable.svg";

  // Panel Icons
  m_iconFilePaths["panel-assets"] = ":/icons/lucide/folder.svg";
  m_iconFilePaths["panel-build"] = ":/icons/lucide/hammer.svg";
  m_iconFilePaths["panel-console"] = ":/icons/lucide/terminal.svg";
  m_iconFilePaths["panel-curve"] = ":/icons/lucide/trending-up.svg";
  m_iconFilePaths["panel-diagnostics"] = ":/icons/lucide/activity.svg";
  m_iconFilePaths["panel-graph"] = ":/icons/lucide/git-graph.svg";
  m_iconFilePaths["panel-hierarchy"] = ":/icons/lucide/list-tree.svg";
  m_iconFilePaths["panel-inspector"] = ":/icons/lucide/sliders-horizontal.svg";
  m_iconFilePaths["panel-localization"] = ":/icons/lucide/globe.svg";
  m_iconFilePaths["panel-scene"] = ":/icons/lucide/image.svg";
  m_iconFilePaths["panel-timeline"] = ":/icons/lucide/film.svg";
  m_iconFilePaths["panel-voice"] = ":/icons/lucide/mic.svg";

  // Playback Controls
  m_iconFilePaths["pause"] = ":/icons/lucide/pause.svg";
  m_iconFilePaths["play"] = ":/icons/lucide/play.svg";
  m_iconFilePaths["step-backward"] = ":/icons/lucide/step-back.svg";
  m_iconFilePaths["step-forward"] = ":/icons/lucide/step-forward.svg";
  m_iconFilePaths["stop"] = ":/icons/lucide/square.svg";

  // Scene Object Icons
  m_iconFilePaths["object-background"] = ":/icons/lucide/image.svg";
  m_iconFilePaths["object-character"] = ":/icons/lucide/user.svg";
  m_iconFilePaths["object-effect"] = ":/icons/lucide/sparkles.svg";
  m_iconFilePaths["object-prop"] = ":/icons/lucide/box.svg";
  m_iconFilePaths["object-ui"] = ":/icons/lucide/layout-dashboard.svg";

  // Status Icons
  m_iconFilePaths["breakpoint"] = ":/icons/lucide/circle-dot.svg";
  m_iconFilePaths["execute"] = ":/icons/lucide/circle-play.svg";
  m_iconFilePaths["status-error"] = ":/icons/lucide/circle-x.svg";
  m_iconFilePaths["status-info"] = ":/icons/lucide/info.svg";
  m_iconFilePaths["status-success"] = ":/icons/lucide/circle-check.svg";
  m_iconFilePaths["status-warning"] = ":/icons/lucide/triangle-alert.svg";

  // System Icons
  m_iconFilePaths["export"] = ":/icons/lucide/upload.svg";
  m_iconFilePaths["external-link"] = ":/icons/lucide/external-link.svg";
  m_iconFilePaths["folder-open"] = ":/icons/lucide/folder-open.svg";
  m_iconFilePaths["import"] = ":/icons/lucide/download.svg";
  m_iconFilePaths["pin"] = ":/icons/lucide/pin.svg";
  m_iconFilePaths["unpin"] = ":/icons/lucide/pin-off.svg";

  // Template Icons
  m_iconFilePaths["template-blank"] = ":/icons/lucide/file.svg";
  m_iconFilePaths["template-dating-sim"] = ":/icons/lucide/heart.svg";
  m_iconFilePaths["template-horror"] = ":/icons/lucide/skull.svg";
  m_iconFilePaths["template-mystery"] = ":/icons/lucide/search.svg";
  m_iconFilePaths["template-rpg"] = ":/icons/lucide/swords.svg";
  m_iconFilePaths["template-visual-novel"] = ":/icons/lucide/book-open.svg";

  // Timeline Icons
  m_iconFilePaths["easing-ease-in"] = ":/icons/lucide/trending-up.svg";
  m_iconFilePaths["easing-ease-in-out"] = ":/icons/lucide/trending-up.svg";
  m_iconFilePaths["easing-ease-out"] = ":/icons/lucide/trending-up.svg";
  m_iconFilePaths["easing-linear"] = ":/icons/lucide/trending-up.svg";
  m_iconFilePaths["keyframe"] = ":/icons/lucide/diamond.svg";
  m_iconFilePaths["keyframe-add"] = ":/icons/lucide/square-plus.svg";
  m_iconFilePaths["keyframe-remove"] = ":/icons/lucide/square-minus.svg";
  m_iconFilePaths["loop"] = ":/icons/lucide/repeat.svg";
  m_iconFilePaths["snap"] = ":/icons/lucide/magnet.svg";
  m_iconFilePaths["snap-off"] = ":/icons/lucide/magnet.svg";

  // Tool Icons
  m_iconFilePaths["tool-frame"] = ":/icons/lucide/frame.svg";
  m_iconFilePaths["tool-hand"] = ":/icons/lucide/hand.svg";
  m_iconFilePaths["tool-select"] = ":/icons/lucide/mouse-pointer.svg";
  m_iconFilePaths["tool-zoom"] = ":/icons/lucide/zoom-in.svg";

  // Transform Icons
  m_iconFilePaths["transform-move"] = ":/icons/lucide/move.svg";
  m_iconFilePaths["transform-rotate"] = ":/icons/lucide/rotate-cw.svg";
  m_iconFilePaths["transform-scale"] = ":/icons/lucide/maximize-2.svg";

  // Utility Icons
  m_iconFilePaths["add"] = ":/icons/lucide/plus.svg";
  m_iconFilePaths["error"] = ":/icons/lucide/circle-x.svg";
  m_iconFilePaths["filter"] = ":/icons/lucide/list-filter.svg";
  m_iconFilePaths["help"] = ":/icons/lucide/circle-question-mark.svg";
  m_iconFilePaths["info"] = ":/icons/lucide/info.svg";
  m_iconFilePaths["refresh"] = ":/icons/lucide/refresh-cw.svg";
  m_iconFilePaths["remove"] = ":/icons/lucide/minus.svg";
  m_iconFilePaths["search"] = ":/icons/lucide/search.svg";
  m_iconFilePaths["settings"] = ":/icons/lucide/settings.svg";
  m_iconFilePaths["warning"] = ":/icons/lucide/triangle-alert.svg";

  // Visibility and Lock Icons
  m_iconFilePaths["hidden"] = ":/icons/lucide/eye-off.svg";
  m_iconFilePaths["locked"] = ":/icons/lucide/lock.svg";
  m_iconFilePaths["unlocked"] = ":/icons/lucide/lock-open.svg";
  m_iconFilePaths["visible"] = ":/icons/lucide/eye.svg";

  // Welcome Icons
  m_iconFilePaths["welcome-docs"] = ":/icons/lucide/book-open.svg";
  m_iconFilePaths["welcome-examples"] = ":/icons/lucide/layout-grid.svg";
  m_iconFilePaths["welcome-new"] = ":/icons/lucide/file-plus.svg";
  m_iconFilePaths["welcome-open"] = ":/icons/lucide/folder-open.svg";
  m_iconFilePaths["welcome-recent"] = ":/icons/lucide/clock.svg";

  // Tutorial Icons
  m_iconFilePaths["take-tour"] = ":/icons/lucide/graduation-cap.svg";
  m_iconFilePaths["quick-start"] = ":/icons/lucide/rocket.svg";

  // Zoom and View Controls
  m_iconFilePaths["zoom-fit"] = ":/icons/lucide/maximize.svg";
  m_iconFilePaths["zoom-in"] = ":/icons/lucide/zoom-in.svg";
  m_iconFilePaths["zoom-out"] = ":/icons/lucide/zoom-out.svg";
  m_iconFilePaths["zoom-reset"] = ":/icons/lucide/scan.svg";

  // Additional Panel Icons
  m_iconFilePaths["panel-assets"] = ":/icons/lucide/folder.svg";
  m_iconFilePaths["panel-asset"] = ":/icons/lucide/folder.svg";
  m_iconFilePaths["panel-scene-view"] = ":/icons/lucide/image.svg";
  m_iconFilePaths["panel-script-editor"] = ":/icons/lucide/file-code.svg";

  // Audio and Recording Icons
  m_iconFilePaths["audio-file"] = ":/icons/lucide/file-audio.svg";
  m_iconFilePaths["record"] = ":/icons/lucide/circle.svg";

  // Graph and Connection Icons
  m_iconFilePaths["connection"] = ":/icons/lucide/link.svg";
  m_iconFilePaths["layout-auto"] = ":/icons/lucide/layout-grid.svg";

  // Media Control Icons
  m_iconFilePaths["fast-forward"] = ":/icons/lucide/fast-forward.svg";

  // Edit Actions
  m_iconFilePaths["edit-rename"] = ":/icons/lucide/pencil.svg";

  // Total icons mapped: 158
}

QIcon NMIconManager::getIcon(const QString& iconName, int size, const QColor& color) {
  QColor iconColor = color.isValid() ? color : m_defaultColor;

  QString cacheKey = QString("%1_%2_%3").arg(iconName).arg(size).arg(iconColor.name());

  if (m_iconCache.contains(cacheKey)) {
    return m_iconCache[cacheKey];
  }

  QPixmap pixmap = getPixmap(iconName, size, iconColor);
  QIcon icon(pixmap);
  m_iconCache[cacheKey] = icon;

  return icon;
}

QPixmap NMIconManager::getPixmap(const QString& iconName, int size, const QColor& color) {
  QString svgData = getSvgData(iconName);
  if (svgData.isEmpty()) {
    // Return empty pixmap if icon not found
    return QPixmap(size, size);
  }

  QColor iconColor = color.isValid() ? color : m_defaultColor;
  return renderSvg(svgData, size, iconColor);
}

void NMIconManager::clearCache() {
  m_iconCache.clear();
}

void NMIconManager::setDefaultColor(const QColor& color) {
  if (m_defaultColor != color) {
    m_defaultColor = color;
    clearCache(); // Clear cache when color changes
  }
}

QString NMIconManager::getSvgData(const QString& iconName) {
  // First, try to load from resource file (Lucide icons)
  if (m_iconFilePaths.contains(iconName)) {
    QString resourcePath = m_iconFilePaths[iconName];
    QString svgData = loadSvgFromResource(resourcePath);
    if (!svgData.isEmpty()) {
      return svgData;
    }
  }

  // Fallback to old hardcoded SVG data (for compatibility)
  return m_iconSvgData.value(iconName, QString());
}

QString NMIconManager::loadSvgFromResource(const QString& resourcePath) {
  QFile file(resourcePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }

  QString svgData = QString::fromUtf8(file.readAll());
  file.close();

  return svgData;
}

QPixmap NMIconManager::renderSvg(const QString& svgData, int size, const QColor& color) {
  // For Lucide icons, we need to modify the SVG to set the stroke color
  QString coloredSvg = svgData;

  // Replace stroke and fill attributes with the requested color
  // Lucide icons use 'currentColor' or explicit stroke colors
  coloredSvg.replace("currentColor", color.name());
  coloredSvg.replace("stroke=\"#000\"", QString("stroke=\"%1\"").arg(color.name()));
  coloredSvg.replace("stroke=\"#000000\"", QString("stroke=\"%1\"").arg(color.name()));
  coloredSvg.replace("stroke='#000'", QString("stroke='%1'").arg(color.name()));
  coloredSvg.replace("stroke='#000000'", QString("stroke='%1'").arg(color.name()));

  // Also handle the old %COLOR% placeholder for backward compatibility
  coloredSvg.replace("%COLOR%", color.name());

  // Create SVG renderer
  QByteArray svgBytes = coloredSvg.toUtf8();
  QSvgRenderer renderer(svgBytes);

  // Check if SVG renderer is valid to prevent rendering crashes
  if (!renderer.isValid()) {
    // Return a valid but empty pixmap if SVG rendering fails
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    return pixmap;
  }

  // Create pixmap
  QPixmap pixmap(size, size);
  pixmap.fill(Qt::transparent);

  // Render SVG with error handling
  QPainter painter(&pixmap);
  if (!painter.isActive()) {
    // If painter fails to initialize, return empty pixmap
    return pixmap;
  }

  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  renderer.render(&painter);
  painter.end();

  return pixmap;
}

} // namespace NovelMind::editor::qt
