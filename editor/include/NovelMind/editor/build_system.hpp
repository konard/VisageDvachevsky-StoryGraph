#pragma once

/**
 * @file build_system.hpp
 * @brief Build System for NovelMind
 *
 * Complete build pipeline for visual novels:
 * - Script compilation to bytecode
 * - Asset processing and packing
 * - Executable generation
 * - Multi-platform support (Windows, Linux, macOS)
 * - Build logging and progress reporting
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/secure_memory.hpp"
#include "NovelMind/core/types.hpp"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace NovelMind::editor {

/**
 * @brief Target platform for build
 */
enum class BuildPlatform : u8 {
  Windows,
  Linux,
  MacOS,
  Web,     // WebAssembly/Emscripten
  Android, // Android APK
  iOS,     // iOS App
  All      // Build for all platforms
};

/**
 * @brief Build type (affects optimizations and debug info)
 */
enum class BuildType : u8 {
  Debug,       // Full debug info, no optimization
  Release,     // Optimized, minimal debug info
  Distribution // Fully optimized, no debug info, signed
};

/**
 * @brief Asset compression level
 */
enum class CompressionLevel : u8 {
  None,     // No compression
  Fast,     // Quick compression (zlib level 1)
  Balanced, // Balance speed and size (zlib level 6)
  Maximum   // Maximum compression (zlib level 9)
};

/**
 * @brief Resource type as per pack_file_format.md specification
 */
enum class ResourceType : u8 {
  Unknown = 0x00,      // Undefined type
  Texture = 0x01,      // Image data (PNG, etc.)
  Audio = 0x02,        // Sound effect
  Music = 0x03,        // Background music (streamable)
  Font = 0x04,         // Font file
  Script = 0x05,       // Compiled NM Script bytecode
  Scene = 0x06,        // Scene definition
  Localization = 0x07, // Translation strings
  Data = 0x08          // Generic data block
};

/**
 * @brief Resource flags as per pack_file_format.md specification
 */
enum class ResourceFlags : u32 {
  None = 0,
  Streamable = 1 << 0, // Resource should be streamed
  Preload = 1 << 1     // Resource should be preloaded
};

/**
 * @brief Pack type for multi-pack VFS (from multi_pack_manager.hpp)
 */
enum class PackTypeId : u8 {
  Base = 0,     // Core game content (lowest priority)
  Patch = 1,    // Official patches/updates
  DLC = 2,      // Downloadable content
  Language = 3, // Localization resources
  Mod = 4       // User mods (highest priority)
};

/**
 * @brief Build configuration
 */
struct BuildConfig {
  // Output settings
  std::string projectPath;
  std::string outputPath;
  std::string executableName;
  std::string version = "1.0.0";
  u32 buildNumber = 1;

  // Platform
  BuildPlatform platform = BuildPlatform::Windows;
  BuildType buildType = BuildType::Release;

  // Asset settings
  bool packAssets = true;
  bool encryptAssets = false;
  std::string encryptionKeyPath; // Path to key file (never the key itself)
  Core::SecureVector<u8> encryptionKey; // 32-byte AES-256 key (secure, zeroed on destruction)
  CompressionLevel compression = CompressionLevel::Balanced;

  // Signing (RSA)
  bool signPacks = false;            // Sign packs with RSA for integrity
  std::string signingPrivateKeyPath; // Path to RSA private key PEM file
  std::string signingPublicKeyPath;  // Path to RSA public key for bundling

  // Features
  bool includeDebugConsole = false;
  bool includeEditor = false;
  bool enableLogging = true;

  // Localization
  std::vector<std::string> includedLanguages;
  std::string defaultLanguage = "en";

  // Exclusions
  std::vector<std::string> excludePatterns;
  std::vector<std::string> excludeFolders;

  // Advanced
  bool stripUnusedAssets = true;
  bool generateSourceMap = false;

  // Code Signing (executable signing for distribution builds)
  bool signExecutable = false;
  std::string signingCertificate;  // Certificate path or identity
  std::string signingPassword;     // Password for certificate (Windows PFX)
  std::string signingEntitlements; // macOS entitlements plist path
  std::string signingTeamId;       // macOS team ID for notarization
  std::string signingTimestampUrl; // Timestamp server URL (optional)

  // Determinism - for reproducible builds
  bool deterministicBuild = true; // Enable deterministic ordering
  u64 fixedBuildTimestamp = 0;    // If non-zero, use this instead of current time
  u32 fixedRandomSeed = 0;        // If non-zero, use for any randomization
};

/**
 * @brief Build step information
 */
struct BuildStep {
  std::string name;
  std::string description;
  f32 progressWeight = 1.0f;
  bool completed = false;
  bool success = true;
  std::string errorMessage;
  f64 durationMs = 0.0;
};

/**
 * @brief Build progress information
 */
struct BuildProgress {
  // Overall progress
  f32 progress = 0.0f; // 0.0 - 1.0
  std::string currentStep;
  std::string currentTask;

  // Steps
  std::vector<BuildStep> steps;
  i32 currentStepIndex = 0;

  // Statistics
  i32 filesProcessed = 0;
  i32 totalFiles = 0;
  i64 bytesProcessed = 0;
  i64 totalBytes = 0;

  // Timing
  f64 elapsedMs = 0.0;
  f64 estimatedRemainingMs = 0.0;

  // Messages
  std::vector<std::string> infoMessages;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;

  // Status
  bool isRunning = false;
  bool isComplete = false;
  bool wasSuccessful = false;
  bool wasCancelled = false;
};

/**
 * @brief Build result summary
 */
struct BuildResult {
  bool success;
  std::string outputPath;
  std::string errorMessage;

  // Statistics
  i32 scriptsCompiled = 0;
  i32 assetsProcessed = 0;
  i64 totalSize = 0;
  i64 compressedSize = 0;
  f64 buildTimeMs = 0.0;

  // Output files
  std::vector<std::string> outputFiles;
  std::vector<std::string> warnings;
};

/**
 * @brief Asset processing result
 */
struct AssetProcessResult {
  std::string sourcePath;
  std::string outputPath;
  i64 originalSize;
  i64 processedSize;
  bool success;
  std::string errorMessage;
};

/**
 * @brief Script compilation result
 */
struct ScriptCompileResult {
  std::string sourcePath;
  bool success;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
  i32 bytecodeSize = 0;
};

/**
 * @brief Build System - Main build coordinator
 */
class BuildSystem {
public:
  BuildSystem();
  ~BuildSystem();

  /**
   * @brief Configure the build system with settings
   *
   * This stores the configuration for later use (e.g., for getBuildTimestamp()
   * before actually starting a build). Call startBuild() to begin the build.
   */
  void configure(const BuildConfig& config);

  /**
   * @brief Start a build with the given configuration
   */
  Result<void> startBuild(const BuildConfig& config);

  /**
   * @brief Cancel the current build
   */
  void cancelBuild();

  /**
   * @brief Check if a build is in progress
   */
  [[nodiscard]] bool isBuildInProgress() const { return m_buildInProgress; }

  /**
   * @brief Get current build progress
   */
  [[nodiscard]] const BuildProgress& getProgress() const { return m_progress; }

  /**
   * @brief Get last build result
   */
  [[nodiscard]] const BuildResult& getLastResult() const { return m_lastResult; }

  /**
   * @brief Validate project before building
   */
  Result<std::vector<std::string>> validateProject(const std::string& projectPath);

  /**
   * @brief Estimate build time
   */
  [[nodiscard]] f64 estimateBuildTime(const BuildConfig& config) const;

  // Callbacks
  void setOnProgressUpdate(std::function<void(const BuildProgress&)> callback);
  void setOnStepComplete(std::function<void(const BuildStep&)> callback);
  void setOnBuildComplete(std::function<void(const BuildResult&)> callback);
  void setOnLogMessage(std::function<void(const std::string&, bool isError)> callback);

  // ==========================================================================
  // Public utilities (for testing and external use)
  // ==========================================================================

  // Crypto helpers (static, can be used without instance)
  [[nodiscard]] static u32 calculateCrc32(const u8* data, usize size);
  [[nodiscard]] static std::array<u8, 32> calculateSha256(const u8* data, usize size);
  [[nodiscard]] static Result<std::vector<u8>> compressData(const std::vector<u8>& data,
                                                            CompressionLevel level);
  [[nodiscard]] static Result<std::vector<u8>>
  encryptData(const std::vector<u8>& data, const Core::SecureVector<u8>& key, std::array<u8, 12>& ivOut);

  // Resource type detection
  [[nodiscard]] static ResourceType getResourceTypeFromExtension(const std::string& path);

  // VFS path normalization
  [[nodiscard]] static std::string normalizeVfsPath(const std::string& path);

  // Key management
  [[nodiscard]] static Result<Core::SecureVector<u8>>
  loadEncryptionKeyFromEnv(); // Load from NOVELMIND_PACK_AES_KEY_HEX or _FILE
  [[nodiscard]] static Result<Core::SecureVector<u8>> loadEncryptionKeyFromFile(const std::string& path);
  [[nodiscard]] static Result<std::vector<u8>> signData(const std::vector<u8>& data,
                                                        const std::string& privateKeyPath);

  // Deterministic timestamp (uses config if set)
  [[nodiscard]] u64 getBuildTimestamp() const;

  // Pack building (public for testing)
  Result<void> buildPack(const std::string& outputPath, const std::vector<std::string>& files,
                         bool encrypt, bool compress);

private:
  // Build pipeline
  void runBuildPipeline();

  // Build steps
  Result<void> prepareOutputDirectory();
  Result<void> compileScripts();
  Result<void> processAssets();
  Result<void> packResources();
  Result<void> generateExecutable();
  Result<void> signAndFinalize();
  Result<void> cleanup();

  // Helpers
  void updateProgress(f32 stepProgress, const std::string& task);
  void logMessage(const std::string& message, bool isError = false);
  void beginStep(const std::string& name, const std::string& description);
  void endStep(bool success, const std::string& errorMessage = "");

  // Script compilation
  ScriptCompileResult compileScript(const std::string& scriptPath);
  Result<void> compileBytecode(const std::string& outputPath);

  // Asset processing
  AssetProcessResult processImage(const std::string& sourcePath, const std::string& outputPath);
  AssetProcessResult processAudio(const std::string& sourcePath, const std::string& outputPath);
  AssetProcessResult processFont(const std::string& sourcePath, const std::string& outputPath);
  AssetProcessResult processData(const std::string& sourcePath, const std::string& outputPath);

  // Platform-specific bundling
  Result<void> buildWindowsExecutable(const std::string& outputPath);
  Result<void> buildLinuxExecutable(const std::string& outputPath);
  Result<void> buildMacOSBundle(const std::string& outputPath);
  Result<void> buildWebBundle(const std::string& outputPath);
  Result<void> buildAndroidBundle(const std::string& outputPath);
  Result<void> buildIOSBundle(const std::string& outputPath);

  // Code Signing
  Result<void> signExecutableForPlatform(const std::string& executablePath);
  Result<void> signWindowsExecutable(const std::string& executablePath);
  Result<void> signMacOSBundle(const std::string& bundlePath);
  Result<i32> executeCommand(const std::string& command, std::string& output) const;

  BuildConfig m_config;
  BuildProgress m_progress;
  BuildResult m_lastResult;

  std::atomic<bool> m_buildInProgress{false};
  std::atomic<bool> m_cancelRequested{false};
  std::unique_ptr<std::thread> m_buildThread;

  // Callbacks
  std::function<void(const BuildProgress&)> m_onProgressUpdate;
  std::function<void(const BuildStep&)> m_onStepComplete;
  std::function<void(const BuildResult&)> m_onBuildComplete;
  std::function<void(const std::string&, bool)> m_onLogMessage;

  // Build state
  std::vector<std::string> m_scriptFiles;
  std::vector<std::string> m_assetFiles;
  std::unordered_map<std::string, std::string> m_assetMapping;
};

/**
 * @brief Asset Processor - Handles asset optimization
 */
class AssetProcessor {
public:
  AssetProcessor();
  ~AssetProcessor();

  /**
   * @brief Process an image file
   */
  Result<AssetProcessResult> processImage(const std::string& sourcePath,
                                          const std::string& outputPath, bool optimize = true);

  /**
   * @brief Process an audio file
   */
  Result<AssetProcessResult> processAudio(const std::string& sourcePath,
                                          const std::string& outputPath, bool compress = true);

  /**
   * @brief Process a font file
   */
  Result<AssetProcessResult> processFont(const std::string& sourcePath,
                                         const std::string& outputPath);

  /**
   * @brief Generate texture atlas from multiple images
   */
  Result<std::string> generateTextureAtlas(const std::vector<std::string>& images,
                                           const std::string& outputPath, i32 maxSize = 4096);

  /**
   * @brief Get asset type from file extension
   */
  [[nodiscard]] static std::string getAssetType(const std::string& path);

  /**
   * @brief Check if asset needs processing
   */
  [[nodiscard]] bool needsProcessing(const std::string& sourcePath,
                                     const std::string& outputPath) const;

private:
  // Image processing
  Result<void> resizeImage(const std::string& input, const std::string& output, i32 maxWidth,
                           i32 maxHeight);
  Result<void> compressImage(const std::string& input, const std::string& output, i32 quality = 85);
  Result<void> convertImageFormat(const std::string& input, const std::string& output,
                                  const std::string& format);

  // Audio processing
  Result<void> convertAudioFormat(const std::string& input, const std::string& output,
                                  const std::string& format);
  Result<void> normalizeAudio(const std::string& input, const std::string& output);
};

/**
 * @brief Pack Builder - Creates encrypted/compressed resource packs
 */
class PackBuilder {
public:
  PackBuilder();
  ~PackBuilder();

  /**
   * @brief Begin a new pack
   */
  Result<void> beginPack(const std::string& outputPath);

  /**
   * @brief Add a file to the pack
   */
  Result<void> addFile(const std::string& sourcePath, const std::string& packPath);

  /**
   * @brief Add raw data to the pack
   */
  Result<void> addData(const std::string& packPath, const std::vector<u8>& data);

  /**
   * @brief Finalize and write the pack
   */
  Result<void> finalizePack();

  /**
   * @brief Set encryption key (secure, will be zeroed on destruction)
   */
  void setEncryptionKey(const Core::SecureVector<u8>& key);

  /**
   * @brief Set compression level
   */
  void setCompressionLevel(CompressionLevel level);

  /**
   * @brief Get pack statistics
   */
  struct PackStats {
    i32 fileCount;
    i64 uncompressedSize;
    i64 compressedSize;
    f32 compressionRatio;
  };
  [[nodiscard]] PackStats getStats() const;

private:
  Result<std::vector<u8>> compressData(const std::vector<u8>& data);
  Result<std::vector<u8>> encryptData(const std::vector<u8>& data);

  std::string m_outputPath;
  Core::SecureVector<u8> m_encryptionKey; // Secure storage, zeroed on destruction
  CompressionLevel m_compressionLevel = CompressionLevel::Balanced;

  struct PackEntry {
    std::string path;
    std::vector<u8> data;
    i64 originalSize;
  };
  std::vector<PackEntry> m_entries;
};

/**
 * @brief Integrity Checker - Validates project before build
 */
class IntegrityChecker {
public:
  IntegrityChecker();
  ~IntegrityChecker();

  struct Issue {
    enum class Severity { Info, Warning, Error };

    Severity severity;
    std::string message;
    std::string file;
    i32 line = 0;
  };

  /**
   * @brief Run all integrity checks
   */
  Result<std::vector<Issue>> checkProject(const std::string& projectPath);

  /**
   * @brief Check for missing assets
   */
  std::vector<Issue> checkMissingAssets(const std::string& projectPath);

  /**
   * @brief Check script validity
   */
  std::vector<Issue> checkScripts(const std::string& projectPath);

  /**
   * @brief Check localization completeness
   */
  std::vector<Issue> checkLocalization(const std::string& projectPath);

  /**
   * @brief Check for unreachable scenes
   */
  std::vector<Issue> checkUnreachableContent(const std::string& projectPath);

  /**
   * @brief Check for circular references
   */
  std::vector<Issue> checkCircularReferences(const std::string& projectPath);

private:
  std::vector<std::string> m_referencedAssets;
  std::vector<std::string> m_existingAssets;
};

/**
 * @brief Build Utilities
 */
namespace BuildUtils {
/**
 * @brief Get platform name string
 */
std::string getPlatformName(BuildPlatform platform);

/**
 * @brief Get executable extension for platform
 */
std::string getExecutableExtension(BuildPlatform platform);

/**
 * @brief Get current platform
 */
BuildPlatform getCurrentPlatform();

/**
 * @brief Format file size for display
 */
std::string formatFileSize(i64 bytes);

/**
 * @brief Format duration for display
 */
std::string formatDuration(f64 milliseconds);

/**
 * @brief Calculate directory size
 */
i64 calculateDirectorySize(const std::string& path);

/**
 * @brief Copy directory recursively
 */
Result<void> copyDirectory(const std::string& source, const std::string& destination);

/**
 * @brief Delete directory recursively
 */
Result<void> deleteDirectory(const std::string& path);

/**
 * @brief Create directory structure
 */
Result<void> createDirectories(const std::string& path);
} // namespace BuildUtils

} // namespace NovelMind::editor
