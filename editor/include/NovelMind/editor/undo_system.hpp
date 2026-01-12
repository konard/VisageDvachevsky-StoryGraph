#pragma once

/**
 * @file undo_system.hpp
 * @brief Unified Undo/Redo System for NovelMind Editor
 *
 * Provides comprehensive undo/redo support for:
 * - StoryGraph operations (node add/delete/move/connect)
 * - SceneView operations (object manipulation)
 * - Asset operations (rename, move, delete)
 * - Editor Settings (layout, hotkeys, themes)
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/scripting/ir.hpp"
#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind::editor {

// Forward declarations
class UndoManager;

/**
 * @brief Base command interface for undo/redo operations
 */
class IEditorCommand {
public:
  virtual ~IEditorCommand() = default;

  /**
   * @brief Execute the command
   */
  virtual void execute() = 0;

  /**
   * @brief Undo the command
   */
  virtual void undo() = 0;

  /**
   * @brief Get human-readable description
   */
  [[nodiscard]] virtual std::string getDescription() const = 0;

  /**
   * @brief Get command category for grouping
   */
  [[nodiscard]] virtual std::string getCategory() const = 0;

  /**
   * @brief Check if command can be merged with another
   */
  [[nodiscard]] virtual bool canMergeWith(const IEditorCommand* /*other*/) const { return false; }

  /**
   * @brief Merge with another command (for continuous operations)
   */
  virtual void mergeWith(const IEditorCommand* /*other*/) {}
};

// ============================================================================
// StoryGraph Commands
// ============================================================================

/**
 * @brief Command for adding a node to the StoryGraph
 */
class StoryGraphAddNodeCommand : public IEditorCommand {
public:
  StoryGraphAddNodeCommand(scripting::VisualGraph* graph, scripting::IRNodeType nodeType, f32 x,
                           f32 y);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

  [[nodiscard]] scripting::NodeId getCreatedNodeId() const { return m_createdNodeId; }

private:
  scripting::VisualGraph* m_graph;
  scripting::IRNodeType m_nodeType;
  f32 m_x, m_y;
  scripting::NodeId m_createdNodeId = 0;
  std::unique_ptr<scripting::VisualGraphNode> m_savedNode;
};

/**
 * @brief Command for removing nodes from the StoryGraph
 */
class StoryGraphRemoveNodesCommand : public IEditorCommand {
public:
  StoryGraphRemoveNodesCommand(scripting::VisualGraph* graph,
                               const std::vector<scripting::NodeId>& nodeIds);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

private:
  struct SavedNode {
    scripting::NodeId id;
    std::unique_ptr<scripting::VisualGraphNode> node;
    f32 x, y;
  };

  struct SavedConnection {
    scripting::NodeId fromNode;
    std::string fromPort;
    scripting::NodeId toNode;
    std::string toPort;
  };

  scripting::VisualGraph* m_graph;
  std::vector<scripting::NodeId> m_nodeIds;
  std::vector<SavedNode> m_savedNodes;
  std::vector<SavedConnection> m_savedConnections;
};

/**
 * @brief Command for moving nodes in the StoryGraph
 */
class StoryGraphMoveNodesCommand : public IEditorCommand {
public:
  StoryGraphMoveNodesCommand(scripting::VisualGraph* graph,
                             const std::vector<scripting::NodeId>& nodeIds, f32 deltaX, f32 deltaY);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

  [[nodiscard]] bool canMergeWith(const IEditorCommand* other) const override;
  void mergeWith(const IEditorCommand* other) override;

private:
  scripting::VisualGraph* m_graph;
  std::vector<scripting::NodeId> m_nodeIds;
  f32 m_deltaX, m_deltaY;
  std::unordered_map<scripting::NodeId, std::pair<f32, f32>> m_originalPositions;
};

/**
 * @brief Command for connecting nodes in the StoryGraph
 */
class StoryGraphConnectCommand : public IEditorCommand {
public:
  StoryGraphConnectCommand(scripting::VisualGraph* graph, scripting::NodeId fromNode,
                           const std::string& fromPort, scripting::NodeId toNode,
                           const std::string& toPort);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

private:
  scripting::VisualGraph* m_graph;
  scripting::NodeId m_fromNode, m_toNode;
  std::string m_fromPort, m_toPort;
};

/**
 * @brief Command for disconnecting nodes in the StoryGraph
 */
class StoryGraphDisconnectCommand : public IEditorCommand {
public:
  StoryGraphDisconnectCommand(scripting::VisualGraph* graph, scripting::NodeId fromNode,
                              const std::string& fromPort, scripting::NodeId toNode,
                              const std::string& toPort);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

private:
  scripting::VisualGraph* m_graph;
  scripting::NodeId m_fromNode, m_toNode;
  std::string m_fromPort, m_toPort;
};

/**
 * @brief Command for modifying node properties
 */
class StoryGraphSetNodePropertyCommand : public IEditorCommand {
public:
  StoryGraphSetNodePropertyCommand(scripting::VisualGraph* graph, scripting::NodeId nodeId,
                                   const std::string& propertyName, const std::string& oldValue,
                                   const std::string& newValue);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "StoryGraph"; }

private:
  scripting::VisualGraph* m_graph;
  scripting::NodeId m_nodeId;
  std::string m_propertyName;
  std::string m_oldValue;
  std::string m_newValue;
};

// ============================================================================
// Asset Commands
// ============================================================================

/**
 * @brief Command for renaming an asset
 */
class AssetRenameCommand : public IEditorCommand {
public:
  AssetRenameCommand(const std::string& oldPath, const std::string& newPath);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Assets"; }

private:
  std::string m_oldPath;
  std::string m_newPath;
};

/**
 * @brief Command for moving an asset
 */
class AssetMoveCommand : public IEditorCommand {
public:
  AssetMoveCommand(const std::string& sourcePath, const std::string& destPath);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Assets"; }

private:
  std::string m_sourcePath;
  std::string m_destPath;
};

/**
 * @brief Command for deleting an asset (with backup)
 */
class AssetDeleteCommand : public IEditorCommand {
public:
  AssetDeleteCommand(const std::string& assetPath);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Assets"; }

private:
  std::string m_assetPath;
  std::vector<u8> m_backupData;
  std::string m_backupMetadata;
};

/**
 * @brief Command for creating a folder
 */
class AssetCreateFolderCommand : public IEditorCommand {
public:
  AssetCreateFolderCommand(const std::string& folderPath);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Assets"; }

private:
  std::string m_folderPath;
};

// ============================================================================
// Editor Settings Commands
// ============================================================================

/**
 * @brief Command for changing editor layout
 */
class EditorLayoutChangeCommand : public IEditorCommand {
public:
  EditorLayoutChangeCommand(const std::string& oldLayoutJson, const std::string& newLayoutJson);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Settings"; }

private:
  std::string m_oldLayoutJson;
  std::string m_newLayoutJson;
};

/**
 * @brief Command for changing a hotkey
 */
class EditorHotkeyChangeCommand : public IEditorCommand {
public:
  EditorHotkeyChangeCommand(const std::string& action, const std::string& oldHotkey,
                            const std::string& newHotkey);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Settings"; }

private:
  std::string m_action;
  std::string m_oldHotkey;
  std::string m_newHotkey;
};

/**
 * @brief Command for changing theme
 */
class EditorThemeChangeCommand : public IEditorCommand {
public:
  EditorThemeChangeCommand(const std::string& oldTheme, const std::string& newTheme);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Settings"; }

private:
  std::string m_oldTheme;
  std::string m_newTheme;
};

// ============================================================================
// Composite Commands
// ============================================================================

/**
 * @brief Groups multiple commands into a single undoable operation
 */
class CompositeCommand : public IEditorCommand {
public:
  explicit CompositeCommand(const std::string& description);

  void addCommand(std::unique_ptr<IEditorCommand> command);

  void execute() override;
  void undo() override;
  [[nodiscard]] std::string getDescription() const override;
  [[nodiscard]] std::string getCategory() const override { return "Composite"; }

  [[nodiscard]] bool isEmpty() const { return m_commands.empty(); }
  [[nodiscard]] size_t getCommandCount() const { return m_commands.size(); }

private:
  std::string m_description;
  std::vector<std::unique_ptr<IEditorCommand>> m_commands;
};

// ============================================================================
// Undo Manager
// ============================================================================

/**
 * @brief Listener for undo manager events
 */
class IUndoListener {
public:
  virtual ~IUndoListener() = default;
  virtual void onUndoStackChanged(bool canUndo, bool canRedo) = 0;
  virtual void onCommandExecuted(const std::string& description) = 0;
  virtual void onUndoPerformed(const std::string& description) = 0;
  virtual void onRedoPerformed(const std::string& description) = 0;
};

/**
 * @brief Central undo manager for the entire editor
 *
 * Features:
 * - Unified undo/redo across all editor systems
 * - Command merging for continuous operations
 * - Transaction support for grouping operations
 * - Configurable history size
 * - Memory-efficient command storage
 */
class UndoManager {
public:
  UndoManager(size_t maxHistorySize = 200);
  ~UndoManager() = default;

  /**
   * @brief Execute a command and add it to the undo stack
   */
  void executeCommand(std::unique_ptr<IEditorCommand> command);

  /**
   * @brief Undo the last command
   */
  bool undo();

  /**
   * @brief Redo the last undone command
   */
  bool redo();

  /**
   * @brief Check if undo is available
   */
  [[nodiscard]] bool canUndo() const;

  /**
   * @brief Check if redo is available
   */
  [[nodiscard]] bool canRedo() const;

  /**
   * @brief Clear all history
   */
  void clearHistory();

  /**
   * @brief Get undo history descriptions
   */
  [[nodiscard]] std::vector<std::string> getUndoHistory() const;

  /**
   * @brief Get redo history descriptions
   */
  [[nodiscard]] std::vector<std::string> getRedoHistory() const;

  /**
   * @brief Get description of next undo operation
   */
  [[nodiscard]] std::string getNextUndoDescription() const;

  /**
   * @brief Get description of next redo operation
   */
  [[nodiscard]] std::string getNextRedoDescription() const;

  /**
   * @brief Begin a transaction (group multiple commands)
   */
  void beginTransaction(const std::string& description);

  /**
   * @brief Commit the current transaction
   */
  void commitTransaction();

  /**
   * @brief Rollback the current transaction
   */
  void rollbackTransaction();

  /**
   * @brief Check if a transaction is in progress
   */
  [[nodiscard]] bool isInTransaction() const { return m_transactionInProgress; }

  /**
   * @brief Set maximum history size
   */
  void setMaxHistorySize(size_t size);

  /**
   * @brief Get current history size
   */
  [[nodiscard]] size_t getHistorySize() const;

  /**
   * @brief Mark the current state as saved
   */
  void markSaved();

  /**
   * @brief Check if document has unsaved changes
   */
  [[nodiscard]] bool hasUnsavedChanges() const;

  /**
   * @brief Add a listener for undo events
   */
  void addListener(IUndoListener* listener);

  /**
   * @brief Remove a listener
   */
  void removeListener(IUndoListener* listener);

private:
  void notifyListeners();
  void notifyCommandExecuted(const std::string& description);
  void notifyUndoPerformed(const std::string& description);
  void notifyRedoPerformed(const std::string& description);
  void trimHistory();

  std::vector<std::unique_ptr<IEditorCommand>> m_undoStack;
  std::vector<std::unique_ptr<IEditorCommand>> m_redoStack;
  size_t m_maxHistorySize;

  // Transaction support
  bool m_transactionInProgress = false;
  std::unique_ptr<CompositeCommand> m_currentTransaction;

  // Saved state tracking
  size_t m_savedAtIndex = 0;

  // Listeners
  std::vector<IUndoListener*> m_listeners;
};

/**
 * @brief RAII helper for transactions
 */
class UndoTransaction {
public:
  UndoTransaction(UndoManager* manager, const std::string& description);
  ~UndoTransaction();

  void commit();
  void rollback();

private:
  UndoManager* m_manager;
  bool m_completed = false;
};

} // namespace NovelMind::editor
