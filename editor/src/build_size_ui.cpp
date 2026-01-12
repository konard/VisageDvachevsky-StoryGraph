/**
 * @file build_size_ui.cpp
 * @brief UI components and visualization implementation
 */

#include "NovelMind/editor/build_size_ui.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace NovelMind::editor {

// ============================================================================
// SizeVisualization Helpers
// ============================================================================

namespace SizeVisualization {

std::string formatBytes(u64 bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  i32 unitIndex = 0;
  f64 size = static_cast<f64>(bytes);

  while (size >= 1024.0 && unitIndex < 4) {
    size /= 1024.0;
    unitIndex++;
  }

  std::ostringstream oss;
  if (unitIndex == 0) {
    oss << bytes << " " << units[unitIndex];
  } else {
    oss << std::fixed << std::setprecision(2) << size << " "
        << units[unitIndex];
  }
  return oss.str();
}

renderer::Color getCategoryColor(AssetCategory category) {
  switch (category) {
  case AssetCategory::Images:
    return renderer::Color{76, 178, 76, 255}; // Green
  case AssetCategory::Audio:
    return renderer::Color{76, 127, 229, 255}; // Blue
  case AssetCategory::Scripts:
    return renderer::Color{229, 178, 51, 255}; // Yellow/Orange
  case AssetCategory::Fonts:
    return renderer::Color{178, 76, 178, 255}; // Purple
  case AssetCategory::Video:
    return renderer::Color{229, 76, 76, 255}; // Red
  case AssetCategory::Data:
    return renderer::Color{127, 127, 127, 255}; // Gray
  case AssetCategory::Other:
  default:
    return renderer::Color{102, 102, 102, 255}; // Dark Gray
  }
}

std::string getCategoryIcon(AssetCategory category) {
  switch (category) {
  case AssetCategory::Images:
    return "image";
  case AssetCategory::Audio:
    return "audio";
  case AssetCategory::Scripts:
    return "code";
  case AssetCategory::Fonts:
    return "font";
  case AssetCategory::Video:
    return "video";
  case AssetCategory::Data:
    return "database";
  case AssetCategory::Other:
  default:
    return "file";
  }
}

renderer::Color getPriorityColor(OptimizationSuggestion::Priority priority) {
  switch (priority) {
  case OptimizationSuggestion::Priority::Critical:
    return renderer::Color{229, 51, 51, 255}; // Red
  case OptimizationSuggestion::Priority::High:
    return renderer::Color{229, 127, 51, 255}; // Orange
  case OptimizationSuggestion::Priority::Medium:
    return renderer::Color{229, 204, 51, 255}; // Yellow
  case OptimizationSuggestion::Priority::Low:
  default:
    return renderer::Color{102, 178, 102, 255}; // Green
  }
}

TreemapNode buildTreemap(const BuildSizeAnalysis &analysis) {
  TreemapNode root;
  root.label = "Build";
  root.size = analysis.totalOriginalSize;
  root.color = renderer::Color{76, 76, 76, 255};

  // Group assets by category
  std::unordered_map<AssetCategory, TreemapNode> categoryNodes;

  for (const auto &asset : analysis.assets) {
    if (categoryNodes.find(asset.category) == categoryNodes.end()) {
      TreemapNode categoryNode;
      switch (asset.category) {
      case AssetCategory::Images:
        categoryNode.label = "Images";
        break;
      case AssetCategory::Audio:
        categoryNode.label = "Audio";
        break;
      case AssetCategory::Scripts:
        categoryNode.label = "Scripts";
        break;
      case AssetCategory::Fonts:
        categoryNode.label = "Fonts";
        break;
      case AssetCategory::Video:
        categoryNode.label = "Video";
        break;
      case AssetCategory::Data:
        categoryNode.label = "Data";
        break;
      case AssetCategory::Other:
      default:
        categoryNode.label = "Other";
        break;
      }
      categoryNode.size = 0;
      categoryNode.color = getCategoryColor(asset.category);
      categoryNodes[asset.category] = categoryNode;
    }

    TreemapNode assetNode;
    assetNode.label = asset.name;
    assetNode.size = asset.originalSize;
    assetNode.color = getCategoryColor(asset.category);

    categoryNodes[asset.category].children.push_back(assetNode);
    categoryNodes[asset.category].size += asset.originalSize;
  }

  // Add category nodes to root
  for (auto &[category, node] : categoryNodes) {
    root.children.push_back(std::move(node));
  }

  return root;
}

void layoutTreemap(TreemapNode &root, f32 x, f32 y, f32 width, f32 height) {
  root.x = x;
  root.y = y;
  root.width = width;
  root.height = height;

  if (root.children.empty() || root.size == 0) {
    return;
  }

  // Simple squarified treemap layout
  bool horizontal = width >= height;
  f32 offset = 0.0f;

  for (auto &child : root.children) {
    f32 ratio = static_cast<f32>(child.size) / static_cast<f32>(root.size);

    if (horizontal) {
      f32 childWidth = width * ratio;
      layoutTreemap(child, x + offset, y, childWidth, height);
      offset += childWidth;
    } else {
      f32 childHeight = height * ratio;
      layoutTreemap(child, x, y + offset, width, childHeight);
      offset += childHeight;
    }
  }
}

} // namespace SizeVisualization

// ============================================================================
// BuildSizeAnalyzerPanel Implementation
// ============================================================================

BuildSizeAnalyzerPanel::BuildSizeAnalyzerPanel() = default;
BuildSizeAnalyzerPanel::~BuildSizeAnalyzerPanel() = default;

void BuildSizeAnalyzerPanel::update([[maybe_unused]] f64 deltaTime) {
  // Update panel state if needed
}

void BuildSizeAnalyzerPanel::render() {
  // TODO: Implement UI rendering
  // This class needs to be migrated to Qt6 (NMDockPanel) to match the
  // new editor architecture. The rendering should use Qt widgets instead
  // of the legacy rendering system.
  // For now, use BuildSizeAnalyzer methods to access analysis data.
}

void BuildSizeAnalyzerPanel::onResize([[maybe_unused]] i32 width,
                                      [[maybe_unused]] i32 height) {
  // Handle resize
}

void BuildSizeAnalyzerPanel::setAnalyzer(BuildSizeAnalyzer *analyzer) {
  m_analyzer = analyzer;
}

void BuildSizeAnalyzerPanel::refreshAnalysis() {
  if (m_analyzer) {
    m_analyzer->analyze();
  }
}

void BuildSizeAnalyzerPanel::exportReport(const std::string &path) {
  if (m_analyzer) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".json") {
      auto result = m_analyzer->exportAsJson();
      if (result.isOk()) {
        std::ofstream file(path);
        file << result.value();
        file.close();
      }
    } else if (ext == ".html") {
      m_analyzer->exportAsHtml(path);
    } else if (ext == ".csv") {
      m_analyzer->exportAsCsv(path);
    }
  }
}

void BuildSizeAnalyzerPanel::setFilter(const std::string &filter) {
  m_filter = filter;
}

void BuildSizeAnalyzerPanel::setCategoryFilter(AssetCategory category) {
  m_categoryFilter = category;
}

void BuildSizeAnalyzerPanel::setOnAssetSelected(
    std::function<void(const std::string &)> callback) {
  m_onAssetSelected = std::move(callback);
}

void BuildSizeAnalyzerPanel::setOnOptimizationApplied(
    std::function<void()> callback) {
  m_onOptimizationApplied = std::move(callback);
}

void BuildSizeAnalyzerPanel::renderOverview() {
  // TODO: Migrate to Qt6 - render overview with total size, file count, etc.
  // Use m_analyzer->getAnalysis() to access data
}

void BuildSizeAnalyzerPanel::renderCategoryBreakdown() {
  // TODO: Migrate to Qt6 - render category pie chart and list
  // Use m_analysis.categorySummaries for data
}

void BuildSizeAnalyzerPanel::renderSizeList() {
  // TODO: Migrate to Qt6 - render sortable list of all assets
  // Use m_analysis.assets with filtering based on m_filter and m_categoryFilter
}

void BuildSizeAnalyzerPanel::renderDuplicates() {
  // TODO: Migrate to Qt6 - render duplicate groups with ability to remove
  // Use m_analysis.duplicates for data
}

void BuildSizeAnalyzerPanel::renderUnused() {
  // TODO: Migrate to Qt6 - render list of unused assets
  // Use m_analysis.unusedAssets for data
}

void BuildSizeAnalyzerPanel::renderSuggestions() {
  // TODO: Migrate to Qt6 - render optimization suggestions with priority
  // Use m_analysis.suggestions for data
}

void BuildSizeAnalyzerPanel::renderToolbar() {
  // TODO: Migrate to Qt6 - render toolbar with refresh, export, filter buttons
}

void BuildSizeAnalyzerPanel::renderPieChart([[maybe_unused]] f32 x,
                                            [[maybe_unused]] f32 y,
                                            [[maybe_unused]] f32 radius) {
  // TODO: Migrate to Qt6 - use QPainter to draw pie chart
  // or use QtCharts module
}

void BuildSizeAnalyzerPanel::renderSizeBar([[maybe_unused]] f32 x,
                                           [[maybe_unused]] f32 y,
                                           [[maybe_unused]] f32 width,
                                           [[maybe_unused]] f32 height,
                                           [[maybe_unused]] u64 size,
                                           [[maybe_unused]] u64 total) {
  // TODO: Migrate to Qt6 - use QPainter to draw progress bar
  // showing size/total ratio
}

std::string BuildSizeAnalyzerPanel::formatSize(u64 bytes) const {
  return SizeVisualization::formatBytes(bytes);
}

} // namespace NovelMind::editor
