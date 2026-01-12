#pragma once

/**
 * @file build_size_ui.hpp
 * @brief UI components and visualization for Build Size Analyzer
 *
 * This module handles:
 * - Size visualization helpers
 * - Color and icon utilities
 * - Treemap generation
 * - BuildSizeAnalyzerPanel UI implementation
 */

#include "NovelMind/core/types.hpp"
#include "NovelMind/editor/build_size_analyzer.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Size visualization helpers
 */
namespace SizeVisualization {
/**
 * @brief Format bytes as human-readable string
 */
std::string formatBytes(u64 bytes);

/**
 * @brief Get color for asset category
 */
renderer::Color getCategoryColor(AssetCategory category);

/**
 * @brief Get icon name for asset category
 */
std::string getCategoryIcon(AssetCategory category);

/**
 * @brief Get color for optimization priority
 */
renderer::Color getPriorityColor(OptimizationSuggestion::Priority priority);

/**
 * @brief Generate treemap data for visualization
 */
struct TreemapNode {
  std::string label;
  u64 size;
  renderer::Color color;
  std::vector<TreemapNode> children;
  f32 x, y, width, height; // Calculated layout
};

TreemapNode buildTreemap(const BuildSizeAnalysis& analysis);
void layoutTreemap(TreemapNode& root, f32 x, f32 y, f32 width, f32 height);
} // namespace SizeVisualization

/**
 * @brief Build Size Analyzer Panel
 */
class BuildSizeAnalyzerPanel {
public:
  BuildSizeAnalyzerPanel();
  ~BuildSizeAnalyzerPanel();

  void update(f64 deltaTime);
  void render();
  void onResize(i32 width, i32 height);

  void setAnalyzer(BuildSizeAnalyzer* analyzer);

  // Actions
  void refreshAnalysis();
  void exportReport(const std::string& path);

  // View modes
  enum class ViewMode : u8 { Overview, ByCategory, BySize, Duplicates, Unused, Suggestions };
  void setViewMode(ViewMode mode) { m_viewMode = mode; }
  [[nodiscard]] ViewMode getViewMode() const { return m_viewMode; }

  // Filtering
  void setFilter(const std::string& filter);
  void setCategoryFilter(AssetCategory category);

  // Callbacks
  void setOnAssetSelected(std::function<void(const std::string&)> callback);
  void setOnOptimizationApplied(std::function<void()> callback);

private:
  void renderOverview();
  void renderCategoryBreakdown();
  void renderSizeList();
  void renderDuplicates();
  void renderUnused();
  void renderSuggestions();
  void renderToolbar();
  void renderPieChart(f32 x, f32 y, f32 radius);
  void renderSizeBar(f32 x, f32 y, f32 width, f32 height, u64 size, u64 total);

  std::string formatSize(u64 bytes) const;

  BuildSizeAnalyzer* m_analyzer = nullptr;

  ViewMode m_viewMode = ViewMode::Overview;

  std::string m_filter;
  std::optional<AssetCategory> m_categoryFilter;

  // Sorting
  enum class SortMode : u8 { Name, Size, Compression, Category };
  SortMode m_sortMode = SortMode::Size;
  bool m_sortAscending = false;

  // Selection
  std::string m_selectedAsset;
  std::vector<std::string> m_selectedDuplicateGroup;

  // UI state
  f32 m_scrollY = 0.0f;
  bool m_showDetails = true;

  // Callbacks
  std::function<void(const std::string&)> m_onAssetSelected;
  std::function<void()> m_onOptimizationApplied;
};

} // namespace NovelMind::editor
