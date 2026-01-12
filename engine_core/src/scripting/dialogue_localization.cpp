/**
 * @file dialogue_localization.cpp
 * @brief Implementation of dialogue localization helper
 *
 * Provides functionality for:
 * - Extracting localizable dialogue from IR graphs
 * - Generating localization keys for dialogue nodes
 * - Managing translation status for embedded dialogue
 */

#include "NovelMind/scripting/ir.hpp"
#include <algorithm>

namespace NovelMind::scripting {

std::vector<DialogueLocalizationEntry>
DialogueLocalizationHelper::collectDialogueEntries(const IRGraph& graph,
                                                   const std::string& sceneId) const {
  std::vector<DialogueLocalizationEntry> entries;

  // Get all nodes and filter for dialogue type
  auto nodes = graph.getNodes();
  for (const auto* node : nodes) {
    if (node->getType() != IRNodeType::Dialogue) {
      continue;
    }

    DialogueLocalizationEntry entry;
    entry.nodeId = node->getId();
    entry.sceneId = sceneId;
    entry.sourceText = node->getStringProperty("text", "");
    entry.speaker = node->getStringProperty("speaker", "");

    // Check if node has a custom key or generate one
    std::string existingKey = getLocalizationKey(*node);
    if (!existingKey.empty()) {
      entry.key = existingKey;
    } else {
      entry.key = DialogueLocalizationData::generateKey(sceneId, node->getId());
    }

    // Check translation status property
    auto statusProp = node->getProperty(PROP_TRANSLATION_STATUS);
    if (statusProp && std::holds_alternative<i64>(*statusProp)) {
      entry.status = static_cast<TranslationStatus>(std::get<i64>(*statusProp));
    } else {
      entry.status = TranslationStatus::Untranslated;
    }

    entries.push_back(std::move(entry));
  }

  // Sort by node ID for consistent ordering
  std::sort(entries.begin(), entries.end(),
            [](const DialogueLocalizationEntry& a, const DialogueLocalizationEntry& b) {
              return a.nodeId < b.nodeId;
            });

  return entries;
}

std::vector<DialogueLocalizationEntry>
DialogueLocalizationHelper::collectChoiceEntries(const IRGraph& graph,
                                                 const std::string& sceneId) const {
  std::vector<DialogueLocalizationEntry> entries;

  auto nodes = graph.getNodes();
  for (const auto* node : nodes) {
    if (node->getType() != IRNodeType::Choice && node->getType() != IRNodeType::ChoiceOption) {
      continue;
    }

    // For Choice nodes, get the options property (vector of strings)
    auto optionsProp = node->getProperty("options");
    if (optionsProp && std::holds_alternative<std::vector<std::string>>(*optionsProp)) {
      const auto& options = std::get<std::vector<std::string>>(*optionsProp);
      for (size_t i = 0; i < options.size(); ++i) {
        DialogueLocalizationEntry entry;
        entry.nodeId = node->getId();
        entry.sceneId = sceneId;
        entry.sourceText = options[i];
        entry.speaker = ""; // Choices don't have speakers
        entry.key = DialogueLocalizationData::generateChoiceKey(sceneId, node->getId(), i);
        entry.status = TranslationStatus::Untranslated;

        entries.push_back(std::move(entry));
      }
    }

    // For ChoiceOption nodes (single option)
    if (node->getType() == IRNodeType::ChoiceOption) {
      DialogueLocalizationEntry entry;
      entry.nodeId = node->getId();
      entry.sceneId = sceneId;
      entry.sourceText = node->getStringProperty("text", "");
      entry.speaker = "";
      entry.key = DialogueLocalizationData::generateKey(sceneId, node->getId());
      entry.status = TranslationStatus::Untranslated;

      entries.push_back(std::move(entry));
    }
  }

  return entries;
}

size_t DialogueLocalizationHelper::generateLocalizationKeys(IRGraph& graph,
                                                            const std::string& sceneId) {
  size_t keysGenerated = 0;

  auto nodes = graph.getNodes();
  for (auto* node : nodes) {
    // Process dialogue nodes
    if (node->getType() == IRNodeType::Dialogue) {
      if (!hasLocalizationKey(*node)) {
        std::string key = DialogueLocalizationData::generateKey(sceneId, node->getId());
        setLocalizationKey(*node, key);
        ++keysGenerated;
      }
    }

    // Process choice option nodes
    if (node->getType() == IRNodeType::ChoiceOption) {
      if (!hasLocalizationKey(*node)) {
        std::string key = DialogueLocalizationData::generateKey(sceneId, node->getId());
        setLocalizationKey(*node, key);
        ++keysGenerated;
      }
    }
  }

  return keysGenerated;
}

bool DialogueLocalizationHelper::hasLocalizationKey(const IRNode& node) const {
  // Check if using custom key
  bool useCustom = node.getBoolProperty(PROP_USE_CUSTOM_KEY, false);
  if (useCustom) {
    std::string customKey = node.getStringProperty(PROP_LOCALIZATION_KEY_CUSTOM, "");
    return !customKey.empty();
  }

  // Check auto-generated key
  std::string key = node.getStringProperty(PROP_LOCALIZATION_KEY, "");
  return !key.empty();
}

std::string DialogueLocalizationHelper::getLocalizationKey(const IRNode& node) const {
  // Check if using custom key
  bool useCustom = node.getBoolProperty(PROP_USE_CUSTOM_KEY, false);
  if (useCustom) {
    std::string customKey = node.getStringProperty(PROP_LOCALIZATION_KEY_CUSTOM, "");
    if (!customKey.empty()) {
      return customKey;
    }
  }

  // Return auto-generated key
  return node.getStringProperty(PROP_LOCALIZATION_KEY, "");
}

void DialogueLocalizationHelper::setLocalizationKey(IRNode& node, const std::string& key) {
  node.setProperty(PROP_LOCALIZATION_KEY, key);
}

std::vector<NodeId> DialogueLocalizationHelper::getLocalizableNodes(const IRGraph& graph) const {
  std::vector<NodeId> nodeIds;

  auto nodes = graph.getNodes();
  for (const auto* node : nodes) {
    IRNodeType type = node->getType();
    if (type == IRNodeType::Dialogue || type == IRNodeType::Choice ||
        type == IRNodeType::ChoiceOption) {
      nodeIds.push_back(node->getId());
    }
  }

  std::sort(nodeIds.begin(), nodeIds.end());
  return nodeIds;
}

std::vector<NodeId> DialogueLocalizationHelper::findMissingKeys(const IRGraph& graph) const {
  std::vector<NodeId> missingIds;

  auto nodes = graph.getNodes();
  for (const auto* node : nodes) {
    IRNodeType type = node->getType();
    if (type == IRNodeType::Dialogue || type == IRNodeType::ChoiceOption) {
      if (!hasLocalizationKey(*node)) {
        missingIds.push_back(node->getId());
      }
    }
  }

  std::sort(missingIds.begin(), missingIds.end());
  return missingIds;
}

} // namespace NovelMind::scripting
