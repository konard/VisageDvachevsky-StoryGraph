#pragma once

/**
 * @file nm_keyframe_item.hpp
 * @brief Custom QGraphicsItem for interactive keyframe editing
 *
 * Provides:
 * - Drag and drop with snapping
 * - Selection support (single and multi-select)
 * - Double-click for easing editing
 * - Visual feedback for interaction states
 * - Easing type visual indicators
 */

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsObject>
#include <QPen>
#include <functional>

namespace NovelMind::editor::qt {

// Forward declaration
enum class EasingType;

struct KeyframeId {
  int trackIndex = -1;
  int frame = 0;

  bool operator==(const KeyframeId& other) const {
    return trackIndex == other.trackIndex && frame == other.frame;
  }

  bool operator<(const KeyframeId& other) const {
    if (trackIndex != other.trackIndex)
      return trackIndex < other.trackIndex;
    return frame < other.frame;
  }
};

/**
 * @brief Custom graphics item for keyframe editing
 *
 * Inherits from QGraphicsObject to support signals/slots.
 * Handles mouse interaction for dragging, selection, and editing.
 */
class NMKeyframeItem : public QGraphicsObject {
  Q_OBJECT

public:
  /**
   * @brief Construct keyframe item
   * @param trackIndex Index of the track this keyframe belongs to
   * @param frame Frame number of this keyframe
   * @param color Color of the keyframe
   * @param parent Parent graphics item
   */
  explicit NMKeyframeItem(int trackIndex, int frame, const QColor& color,
                          QGraphicsItem* parent = nullptr);

  /**
   * @brief Get the bounding rectangle
   */
  QRectF boundingRect() const override;

  /**
   * @brief Paint the keyframe
   */
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget = nullptr) override;

  /**
   * @brief Get keyframe ID
   */
  KeyframeId id() const { return m_id; }

  /**
   * @brief Get/set selection state
   */
  bool isSelected() const { return m_selected; }
  void setSelected(bool selected);

  /**
   * @brief Set the frame position
   */
  void setFrame(int frame);

  /**
   * @brief Get current frame
   */
  int frame() const { return m_id.frame; }

  /**
   * @brief Get track index
   */
  int trackIndex() const { return m_id.trackIndex; }

  /**
   * @brief Set if snapping is enabled
   */
  void setSnapToGrid(bool enabled) { m_snapToGrid = enabled; }

  /**
   * @brief Set grid size for snapping
   */
  void setGridSize(int size) { m_gridSize = size; }

  /**
   * @brief Set coordinate conversion functions
   */
  void setFrameConverter(std::function<int(int)> xToFrame, std::function<int(int)> frameToX) {
    m_xToFrame = xToFrame;
    m_frameToX = frameToX;
  }

  /**
   * @brief Set the easing type for visual indicator
   * @param easing The easing type to display
   */
  void setEasingType(int easing);

  /**
   * @brief Get the current easing type
   */
  int easingType() const { return m_easingType; }

signals:
  /**
   * @brief Emitted when keyframe is moved
   * @param oldFrame Previous frame position
   * @param newFrame New frame position
   * @param trackIndex Track index
   */
  void moved(int oldFrame, int newFrame, int trackIndex);

  /**
   * @brief Emitted when keyframe drag starts
   * @param id Keyframe ID
   */
  void dragStarted(const KeyframeId& id);

  /**
   * @brief Emitted when keyframe drag ends
   */
  void dragEnded();

  /**
   * @brief Emitted when keyframe is clicked
   * @param additiveSelection True if Ctrl is held (additive selection)
   * @param rangeSelection True if Shift is held (range selection)
   * @param id Keyframe ID
   */
  void clicked(bool additiveSelection, bool rangeSelection, const KeyframeId& id);

  /**
   * @brief Emitted when keyframe is double-clicked
   * @param trackIndex Track index
   * @param frame Frame number
   */
  void doubleClicked(int trackIndex, int frame);

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  int snapToGrid(int frame) const;

  KeyframeId m_id;
  QColor m_color;
  bool m_selected = false;
  bool m_hovered = false;
  bool m_dragging = false;
  QPointF m_dragStartPos;
  int m_dragStartFrame = 0;

  // Snapping
  bool m_snapToGrid = true;
  int m_gridSize = 5;

  // Coordinate conversion
  std::function<int(int)> m_xToFrame;
  std::function<int(int)> m_frameToX;

  // Easing type for visual indicator
  int m_easingType = 0; // 0 = Linear

  static constexpr qreal KEYFRAME_RADIUS = 4.0;
};

} // namespace NovelMind::editor::qt

// std::hash specialization for KeyframeId
namespace std {
template <> struct hash<NovelMind::editor::qt::KeyframeId> {
  std::size_t operator()(const NovelMind::editor::qt::KeyframeId& key) const noexcept {
    return std::hash<int>{}(key.trackIndex) ^ (std::hash<int>{}(key.frame) << 1);
  }
};
} // namespace std

// Hash function for KeyframeId to use in QSet (must be in global namespace)
inline uint qHash(const NovelMind::editor::qt::KeyframeId& key, uint seed = 0) {
  Q_UNUSED(seed);
  return static_cast<uint>(std::hash<NovelMind::editor::qt::KeyframeId>{}(key));
}
