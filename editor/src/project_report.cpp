#include "NovelMind/editor/project_integrity.hpp"

#include <algorithm>

namespace NovelMind::editor {

std::vector<IntegrityIssue> IntegrityReport::getIssuesBySeverity(IssueSeverity severity) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [severity](const IntegrityIssue& issue) { return issue.severity == severity; });
  return result;
}

std::vector<IntegrityIssue> IntegrityReport::getIssuesByCategory(IssueCategory category) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [category](const IntegrityIssue& issue) { return issue.category == category; });
  return result;
}

std::vector<IntegrityIssue> IntegrityReport::getIssuesByFile(const std::string& filePath) const {
  std::vector<IntegrityIssue> result;
  std::copy_if(issues.begin(), issues.end(), std::back_inserter(result),
               [&filePath](const IntegrityIssue& issue) { return issue.filePath == filePath; });
  return result;
}
IntegritySummary
ProjectIntegrityChecker::calculateSummary(const std::vector<IntegrityIssue>& issues) {
  IntegritySummary summary;
  summary.totalIssues = static_cast<i32>(issues.size());

  for (const auto& issue : issues) {
    // Count by severity
    switch (issue.severity) {
    case IssueSeverity::Info:
      summary.infoCount++;
      break;
    case IssueSeverity::Warning:
      summary.warningCount++;
      break;
    case IssueSeverity::Error:
      summary.errorCount++;
      break;
    case IssueSeverity::Critical:
      summary.criticalCount++;
      break;
    }

    // Count by category
    switch (issue.category) {
    case IssueCategory::Scene:
      summary.sceneIssues++;
      break;
    case IssueCategory::Asset:
      summary.assetIssues++;
      break;
    case IssueCategory::VoiceLine:
      summary.voiceIssues++;
      break;
    case IssueCategory::Localization:
      summary.localizationIssues++;
      break;
    case IssueCategory::StoryGraph:
      summary.graphIssues++;
      break;
    case IssueCategory::Script:
      summary.scriptIssues++;
      break;
    case IssueCategory::Resource:
      summary.resourceIssues++;
      break;
    case IssueCategory::Configuration:
      summary.configIssues++;
      break;
    }
  }

  // Set asset statistics
  summary.totalAssets = static_cast<i32>(m_projectAssets.size());
  summary.referencedAssets = static_cast<i32>(m_referencedAssets.size());
  summary.unreferencedAssets = summary.totalAssets - summary.referencedAssets;

  return summary;
}


} // namespace NovelMind::editor
