#pragma once

/**
 * @file multi_pack_manager.hpp
 * @brief Multi-Pack Manager - Layered pack file support for mods and DLC
 *
 * Provides a hierarchical pack mounting system:
 * - Base pack: Core game content
 * - Patch packs: Bug fixes and updates
 * - DLC packs: Additional content
 * - Mod packs: User-created content
 * - Language packs: Localization resources
 *
 * Resources are resolved by priority, allowing higher-priority packs
 * to override resources from lower-priority packs.
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/secure_memory.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/vfs/secure_pack_reader.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::vfs {

/**
 * @brief Pack type for priority ordering
 */
enum class PackType : u8 {
  Base = 0,     // Core game content (lowest priority)
  Patch = 1,    // Official patches/updates
  DLC = 2,      // Downloadable content
  Language = 3, // Localization resources
  Mod = 4       // User mods (highest priority)
};

/**
 * @brief Pack mount information
 */
struct PackInfo {
  std::string id;          // Unique pack identifier
  std::string path;        // File system path to pack
  std::string name;        // Display name
  std::string version;     // Pack version
  std::string author;      // Pack author
  std::string description; // Pack description
  PackType type = PackType::Base;
  i32 priority = 0;      // Priority within type (higher = override)
  bool enabled = true;   // Whether pack is active
  bool verified = false; // Whether signature is verified

  // Dependencies
  std::vector<std::string> dependencies; // Required pack IDs
  std::string minEngineVersion;          // Minimum engine version
  std::string targetGameVersion;         // Target game version

  // Metadata
  u64 fileSize = 0;
  u64 resourceCount = 0;
  u64 createdTimestamp = 0;
  std::string checksum;
};

/**
 * @brief Resource override entry
 */
struct ResourceOverride {
  std::string resourceId;
  std::string originalPackId; // Pack where resource was originally defined
  std::string overridePackId; // Pack that overrides it
  PackType overrideType;
};

/**
 * @brief Pack load result
 */
struct PackLoadResult {
  bool success = false;
  std::string packId;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
  std::vector<std::string> missingDependencies;
  u64 loadedResources = 0;
};

/**
 * @brief Pack discovery entry
 */
struct DiscoveredPack {
  std::string path;
  PackInfo info;
  bool canLoad = true;
  std::string loadError;
};

/**
 * @brief Callback types
 */
using OnPackLoaded = std::function<void(const PackInfo &)>;
using OnPackUnloaded = std::function<void(const std::string &packId)>;
using OnResourceOverridden = std::function<void(const ResourceOverride &)>;

/**
 * @brief Multi-Pack Manager - Layered pack file management
 *
 * The Multi-Pack Manager provides a sophisticated pack file system that
 * supports:
 *
 * 1. Hierarchical priority system:
 *    - Base pack (core game)
 *    - Patch packs (updates)
 *    - DLC packs (additional content)
 *    - Language packs (translations)
 *    - Mod packs (user content)
 *
 * 2. Resource overriding:
 *    - Higher priority packs override lower priority
 *    - Within same type, explicit priority ordering
 *
 * 3. Dependency management:
 *    - Packs can declare dependencies
 *    - Automatic dependency resolution
 *    - Version compatibility checking
 *
 * Example usage:
 * @code
 * MultiPackManager packManager;
 * packManager.setPackDirectory("/path/to/packs");
 *
 * // Load base game
 * packManager.loadBasePack("game.nmpack");
 *
 * // Discover and load available mods
 * auto mods = packManager.discoverPacks("/path/to/mods");
 * for (const auto& mod : mods) {
 *     if (mod.canLoad) {
 *         packManager.loadPack(mod.path, PackType::Mod);
 *     }
 * }
 *
 * // Check for resource
 * if (packManager.exists("sprites/hero.png")) {
 *     auto data = packManager.readResource("sprites/hero.png");
 * }
 * @endcode
 */
class MultiPackManager {
public:
  MultiPackManager();
  ~MultiPackManager();

  // Non-copyable
  MultiPackManager(const MultiPackManager &) = delete;
  MultiPackManager &operator=(const MultiPackManager &) = delete;

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize the pack manager
   * @return Success or error
   */
  Result<void> initialize();

  /**
   * @brief Shutdown and unload all packs
   */
  void shutdown();

  /**
   * @brief Set the base directory for pack discovery
   */
  void setPackDirectory(const std::string &path);

  /**
   * @brief Set the mods directory for mod discovery
   */
  void setModsDirectory(const std::string &path);

  /**
   * @brief Set public key PEM for signed pack verification
   */
  void setPackPublicKeyPem(const std::string &pem);

  /**
   * @brief Set public key path for signed pack verification
   */
  void setPackPublicKeyPath(const std::string &path);

  /**
   * @brief Set decryption key for encrypted packs
   */
  void setPackDecryptionKey(const std::vector<u8> &key);
  /**
   * @brief Load decryption key from a binary file
   */
  Result<void> setPackDecryptionKeyFromFile(const std::string &path);
  /**
   * @brief Load decryption key from environment variables.
   *
   * Supported vars:
   * - NOVELMIND_PACK_AES_KEY_HEX: hex-encoded key (32 bytes for AES-256)
   * - NOVELMIND_PACK_AES_KEY_FILE: path to a binary key file
   * - NOVELMIND_PACK_PUBLIC_KEY: path to PEM public key
   */
  Result<void> configureKeysFromEnvironment();

  // =========================================================================
  // Pack Loading
  // =========================================================================

  /**
   * @brief Load the base game pack
   * @param path Path to base pack file
   * @return Load result
   */
  PackLoadResult loadBasePack(const std::string &path);

  /**
   * @brief Load a pack file
   * @param path Path to pack file
   * @param type Pack type for priority
   * @param priority Priority within type (default 0)
   * @return Load result
   */
  PackLoadResult loadPack(const std::string &path, PackType type,
                          i32 priority = 0);

  /**
   * @brief Unload a pack
   * @param packId Pack identifier to unload
   */
  void unloadPack(const std::string &packId);

  /**
   * @brief Unload all packs of a specific type
   */
  void unloadPacksOfType(PackType type);

  /**
   * @brief Unload all packs
   */
  void unloadAllPacks();

  /**
   * @brief Reload a pack (unload then load)
   */
  PackLoadResult reloadPack(const std::string &packId);

  /**
   * @brief Enable/disable a loaded pack
   */
  void setPackEnabled(const std::string &packId, bool enabled);

  // =========================================================================
  // Pack Discovery
  // =========================================================================

  /**
   * @brief Discover available packs in a directory
   * @param directory Directory to scan
   * @return List of discovered packs
   */
  std::vector<DiscoveredPack> discoverPacks(const std::string &directory);

  /**
   * @brief Discover available mods in the mods directory
   */
  std::vector<DiscoveredPack> discoverMods();

  /**
   * @brief Discover available language packs
   */
  std::vector<DiscoveredPack> discoverLanguagePacks();

  /**
   * @brief Check if a pack's dependencies are satisfied
   */
  bool areDependenciesSatisfied(const PackInfo &pack) const;

  /**
   * @brief Get missing dependencies for a pack
   */
  std::vector<std::string> getMissingDependencies(const PackInfo &pack) const;

  // =========================================================================
  // Pack Information
  // =========================================================================

  /**
   * @brief Get info for a loaded pack
   */
  [[nodiscard]] const PackInfo *getPackInfo(const std::string &packId) const;

  /**
   * @brief Get all loaded packs
   */
  [[nodiscard]] std::vector<const PackInfo *> getLoadedPacks() const;

  /**
   * @brief Get loaded packs of a specific type
   */
  [[nodiscard]] std::vector<const PackInfo *>
  getPacksOfType(PackType type) const;

  /**
   * @brief Check if a pack is loaded
   */
  [[nodiscard]] bool isPackLoaded(const std::string &packId) const;

  /**
   * @brief Check if a pack is enabled
   */
  [[nodiscard]] bool isPackEnabled(const std::string &packId) const;

  // =========================================================================
  // Resource Access
  // =========================================================================

  /**
   * @brief Read a resource (respecting priority)
   * @param resourceId Resource identifier
   * @return Resource data or error
   */
  Result<std::vector<u8>> readResource(const std::string &resourceId);

  /**
   * @brief Check if a resource exists in any loaded pack
   */
  [[nodiscard]] bool exists(const std::string &resourceId) const;

  /**
   * @brief Get resource info
   */
  [[nodiscard]] std::optional<ResourceInfo>
  getResourceInfo(const std::string &resourceId) const;

  /**
   * @brief Get the pack that provides a resource (with overrides resolved)
   */
  [[nodiscard]] std::string
  getResourcePack(const std::string &resourceId) const;

  /**
   * @brief List all resources of a type
   */
  [[nodiscard]] std::vector<std::string>
  listResources(ResourceType type = ResourceType::Unknown) const;

  /**
   * @brief Get all overrides currently in effect
   */
  [[nodiscard]] std::vector<ResourceOverride> getActiveOverrides() const;

  /**
   * @brief Read resource from a specific pack (bypassing priority)
   */
  Result<std::vector<u8>> readResourceFromPack(const std::string &packId,
                                               const std::string &resourceId);

  // =========================================================================
  // Mod Support
  // =========================================================================

  /**
   * @brief Get the mod load order
   */
  [[nodiscard]] std::vector<std::string> getModLoadOrder() const;

  /**
   * @brief Set the mod load order
   */
  void setModLoadOrder(const std::vector<std::string> &order);

  /**
   * @brief Move a mod up in load order
   */
  void moveModUp(const std::string &packId);

  /**
   * @brief Move a mod down in load order
   */
  void moveModDown(const std::string &packId);

  /**
   * @brief Save mod configuration to file
   */
  Result<void> saveModConfig(const std::string &path);

  /**
   * @brief Load mod configuration from file
   */
  Result<void> loadModConfig(const std::string &path);

  // =========================================================================
  // Statistics
  // =========================================================================

  /**
   * @brief Get total number of loaded packs
   */
  [[nodiscard]] size_t getPackCount() const;

  /**
   * @brief Get total number of available resources
   */
  [[nodiscard]] size_t getResourceCount() const;

  /**
   * @brief Get number of resource overrides
   */
  [[nodiscard]] size_t getOverrideCount() const;

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnPackLoaded(OnPackLoaded callback);
  void setOnPackUnloaded(OnPackUnloaded callback);
  void setOnResourceOverridden(OnResourceOverridden callback);

private:
  // Internal helpers
  PackLoadResult loadPackInternal(const std::string &path, PackType type,
                                  i32 priority);
  PackInfo readPackManifest(const std::string &path);
  void rebuildResourceIndex();
  i32 calculateEffectivePriority(PackType type, i32 basePriority) const;
  void firePackLoaded(const PackInfo &info);
  void firePackUnloaded(const std::string &packId);
  void fireResourceOverridden(const ResourceOverride &override);

  // State
  bool m_initialized = false;
  std::string m_packDirectory;
  std::string m_modsDirectory;

  // Loaded packs (ordered by effective priority)
  struct LoadedPack {
    PackInfo info;
    std::unique_ptr<IVirtualFileSystem> reader;
    i32 effectivePriority = 0;
    std::set<std::string> providedResources;
  };

  std::vector<std::unique_ptr<LoadedPack>> m_packs;
  std::unordered_map<std::string, size_t> m_packIdToIndex;

  // Resource index: resource ID -> pack index
  std::unordered_map<std::string, size_t> m_resourceIndex;

  // Mod load order
  std::vector<std::string> m_modLoadOrder;

  Core::SecureVector<u8> m_decryptionKey; // Secure storage, zeroed on destruction
  std::string m_publicKeyPem;
  std::string m_publicKeyPath;

  // Callbacks
  OnPackLoaded m_onPackLoaded;
  OnPackUnloaded m_onPackUnloaded;
  OnResourceOverridden m_onResourceOverridden;
};

} // namespace NovelMind::vfs
