#include "NovelMind/editor/qt/navigation_location.hpp"

namespace NovelMind::editor::qt {

NavigationLocation NavigationLocation::makeStoryGraph(const QString& nodeId) {
  NavigationLocation loc;
  loc.m_isValid = !nodeId.isEmpty();
  loc.m_sourceType = NavigationSourceType::StoryGraph;
  loc.m_nodeId = nodeId;
  return loc;
}

NavigationLocation NavigationLocation::makeScript(const QString& filePath, int lineNumber) {
  NavigationLocation loc;
  loc.m_isValid = !filePath.isEmpty();
  loc.m_sourceType = NavigationSourceType::Script;
  loc.m_filePath = filePath;
  loc.m_lineNumber = lineNumber;
  return loc;
}

std::optional<NavigationLocation> NavigationLocation::parse(const QString& locationString) {
  if (locationString.isEmpty()) {
    return std::nullopt;
  }

  // Split by colon
  QStringList parts = locationString.split(':');

  if (parts.size() < 2) {
    return std::nullopt;
  }

  QString typeStr = parts[0].trimmed();

  // Parse StoryGraph format: "StoryGraph:node_id"
  if (typeStr.compare("StoryGraph", Qt::CaseInsensitive) == 0) {
    if (parts.size() != 2) {
      return std::nullopt;
    }
    QString nodeId = parts[1].trimmed();
    if (nodeId.isEmpty()) {
      return std::nullopt;
    }
    return makeStoryGraph(nodeId);
  }

  // Parse Script format: "Script:path:line" or "Script:path"
  if (typeStr.compare("Script", Qt::CaseInsensitive) == 0) {
    if (parts.size() < 2 || parts.size() > 3) {
      return std::nullopt;
    }

    QString filePath = parts[1].trimmed();
    if (filePath.isEmpty()) {
      return std::nullopt;
    }

    int lineNumber = -1;
    if (parts.size() == 3) {
      bool ok = false;
      lineNumber = parts[2].trimmed().toInt(&ok);
      if (!ok || lineNumber < 1) {
        return std::nullopt;
      }
    }

    return makeScript(filePath, lineNumber);
  }

  // Unknown type
  return std::nullopt;
}

QString NavigationLocation::nodeId() const {
  if (m_sourceType == NavigationSourceType::StoryGraph) {
    return m_nodeId;
  }
  return QString();
}

QString NavigationLocation::filePath() const {
  if (m_sourceType == NavigationSourceType::Script) {
    return m_filePath;
  }
  return QString();
}

int NavigationLocation::lineNumber() const {
  if (m_sourceType == NavigationSourceType::Script) {
    return m_lineNumber;
  }
  return -1;
}

QString NavigationLocation::toString() const {
  if (!m_isValid) {
    return QString();
  }

  if (m_sourceType == NavigationSourceType::StoryGraph) {
    return QString("StoryGraph:%1").arg(m_nodeId);
  }

  if (m_sourceType == NavigationSourceType::Script) {
    if (m_lineNumber > 0) {
      return QString("Script:%1:%2").arg(m_filePath).arg(m_lineNumber);
    }
    return QString("Script:%1").arg(m_filePath);
  }

  return QString();
}

QString NavigationLocation::description() const {
  if (!m_isValid) {
    return QString("Invalid location");
  }

  if (m_sourceType == NavigationSourceType::StoryGraph) {
    return QString("Story Graph node '%1'").arg(m_nodeId);
  }

  if (m_sourceType == NavigationSourceType::Script) {
    if (m_lineNumber > 0) {
      return QString("Script '%1' line %2").arg(m_filePath).arg(m_lineNumber);
    }
    return QString("Script '%1'").arg(m_filePath);
  }

  return QString("Unknown location");
}

} // namespace NovelMind::editor::qt
