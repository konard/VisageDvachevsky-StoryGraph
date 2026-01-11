/**
 * @file multi_pack_manager.cpp
 * @brief Multi-Pack Manager implementation
 */

#include "NovelMind/vfs/multi_pack_manager.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <utility>

namespace NovelMind::vfs {

namespace fs = std::filesystem;
namespace {

bool readFileToString(std::ifstream &file, std::string &out) {
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size == std::streampos(-1) || size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(out.data(), static_cast<std::streamsize>(out.size()));
  return static_cast<bool>(file);
}

Result<std::vector<u8>> readBinaryFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return Result<std::vector<u8>>::error("Failed to open file: " + path);
  }
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size == std::streampos(-1) || size < 0) {
    return Result<std::vector<u8>>::error("Failed to determine file size");
  }
  std::vector<u8> data(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(data.data()),
            static_cast<std::streamsize>(data.size()));
  if (!file) {
    return Result<std::vector<u8>>::error("Failed to read file: " + path);
  }
  return Result<std::vector<u8>>::ok(std::move(data));
}

std::optional<std::vector<u8>> decodeHex(const std::string &hex) {
  if (hex.size() % 2 != 0) {
    return std::nullopt;
  }
  std::vector<u8> bytes;
  bytes.reserve(hex.size() / 2);
  auto hexValue = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
      return 10 + (c - 'A');
    return -1;
  };

  for (size_t i = 0; i < hex.size(); i += 2) {
    int hi = hexValue(hex[i]);
    int lo = hexValue(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      return std::nullopt;
    }
    bytes.push_back(static_cast<u8>((hi << 4) | lo));
  }
  return bytes;
}

std::string getEnvValue(const char *name) {
#if defined(_WIN32)
  char *value = nullptr;
  size_t len = 0;
  if (_dupenv_s(&value, &len, name) != 0 || !value) {
    return {};
  }
  std::string out(value);
  std::free(value);
  return out;
#else
  const char *value = std::getenv(name);
  return value ? std::string(value) : std::string();
#endif
}

} // namespace

// Priority base values for each pack type
constexpr i32 PRIORITY_BASE = 1000;
constexpr i32 PRIORITY_PATCH = 2000;
constexpr i32 PRIORITY_DLC = 3000;
constexpr i32 PRIORITY_LANGUAGE = 4000;
constexpr i32 PRIORITY_MOD = 5000;

MultiPackManager::MultiPackManager() = default;

MultiPackManager::~MultiPackManager() {
  if (m_initialized) {
    shutdown();
  }
}

// =========================================================================
// Initialization
// =========================================================================

Result<void> MultiPackManager::initialize() {
  if (m_initialized) {
    return Result<void>::error("MultiPackManager already initialized");
  }

  m_packs.clear();
  m_packIdToIndex.clear();
  m_resourceIndex.clear();
  m_modLoadOrder.clear();

  auto envResult = configureKeysFromEnvironment();
  if (envResult.isError()) {
    return envResult;
  }

  m_initialized = true;
  return {};
}

void MultiPackManager::shutdown() {
  unloadAllPacks();
  m_initialized = false;
}

void MultiPackManager::setPackDirectory(const std::string &path) {
  m_packDirectory = path;
}

void MultiPackManager::setModsDirectory(const std::string &path) {
  m_modsDirectory = path;
}

void MultiPackManager::setPackPublicKeyPem(const std::string &pem) {
  m_publicKeyPem = pem;
}

void MultiPackManager::setPackPublicKeyPath(const std::string &path) {
  m_publicKeyPath = path;
}

void MultiPackManager::setPackDecryptionKey(const std::vector<u8> &key) {
  m_decryptionKey.assign(key.begin(), key.end());
}

Result<void>
MultiPackManager::setPackDecryptionKeyFromFile(const std::string &path) {
  auto keyResult = readBinaryFile(path);
  if (keyResult.isError()) {
    return Result<void>::error(keyResult.error());
  }
  if (keyResult.value().empty()) {
    return Result<void>::error("Key file is empty: " + path);
  }
  const auto& key = keyResult.value();
  m_decryptionKey.assign(key.begin(), key.end());
  return Result<void>::ok();
}

Result<void> MultiPackManager::configureKeysFromEnvironment() {
  const std::string hexKey = getEnvValue("NOVELMIND_PACK_AES_KEY_HEX");
  const std::string keyFile = getEnvValue("NOVELMIND_PACK_AES_KEY_FILE");
  const std::string pubKeyPath = getEnvValue("NOVELMIND_PACK_PUBLIC_KEY");

  if (!hexKey.empty()) {
    auto decoded = decodeHex(hexKey.c_str());
    if (!decoded.has_value() || decoded->empty()) {
      return Result<void>::error(
          "Invalid NOVELMIND_PACK_AES_KEY_HEX (must be even-length hex)");
    }
    m_decryptionKey.assign(decoded->begin(), decoded->end());
  } else if (!keyFile.empty()) {
    auto keyResult = setPackDecryptionKeyFromFile(keyFile);
    if (keyResult.isError()) {
      return keyResult;
    }
  }

  if (!pubKeyPath.empty()) {
    m_publicKeyPath = pubKeyPath;
  }

  return Result<void>::ok();
}

// =========================================================================
// Pack Loading
// =========================================================================

PackLoadResult MultiPackManager::loadBasePack(const std::string &path) {
  return loadPackInternal(path, PackType::Base, 0);
}

PackLoadResult MultiPackManager::loadPack(const std::string &path,
                                          PackType type, i32 priority) {
  return loadPackInternal(path, type, priority);
}

PackLoadResult MultiPackManager::loadPackInternal(const std::string &path,
                                                  PackType type, i32 priority) {
  PackLoadResult result;
  result.success = false;

  if (!fs::exists(path)) {
    result.errors.push_back("Pack file not found: " + path);
    return result;
  }

  // Read pack manifest/header
  PackInfo info;
  try {
    info = readPackManifest(path);
  } catch (const std::exception &e) {
    result.errors.push_back("Failed to read pack manifest: " +
                            std::string(e.what()));
    return result;
  }

  info.path = path;
  info.type = type;
  info.priority = priority;
  result.packId = info.id;

  // Check if already loaded
  if (isPackLoaded(info.id)) {
    result.errors.push_back("Pack already loaded: " + info.id);
    return result;
  }

  // Check dependencies
  auto missingDeps = getMissingDependencies(info);
  if (!missingDeps.empty()) {
    result.missingDependencies = missingDeps;
    for (const auto &dep : missingDeps) {
      result.warnings.push_back("Missing dependency: " + dep);
    }
    // Continue loading but warn about missing dependencies
  }

  // Create pack reader
  auto reader = std::make_unique<SecurePackFileSystem>();
  if (!m_publicKeyPem.empty()) {
    reader->setPublicKeyPem(m_publicKeyPem);
  }
  if (!m_publicKeyPath.empty()) {
    reader->setPublicKeyPath(m_publicKeyPath);
  }
  if (!m_decryptionKey.empty()) {
    reader->setDecryptionKey(m_decryptionKey);
  }

  auto openResult = reader->mount(path);
  if (openResult.isError()) {
    result.errors.push_back("Failed to open pack: " + openResult.error());
    return result;
  }

  // Create loaded pack entry
  auto loadedPack = std::make_unique<LoadedPack>();
  loadedPack->info = std::move(info);
  loadedPack->reader = std::move(reader);
  loadedPack->effectivePriority = calculateEffectivePriority(type, priority);

  // Collect provided resources
  auto resources = loadedPack->reader->listResources();
  for (const auto &resId : resources) {
    loadedPack->providedResources.insert(resId);
  }
  result.loadedResources = loadedPack->providedResources.size();

  // Add to packs list
  m_packIdToIndex[loadedPack->info.id] = m_packs.size();
  m_packs.push_back(std::move(loadedPack));

  // If this is a mod, add to load order
  if (type == PackType::Mod) {
    m_modLoadOrder.push_back(result.packId);
  }

  // Rebuild resource index
  rebuildResourceIndex();

  result.success = true;
  firePackLoaded(m_packs.back()->info);

  return result;
}

void MultiPackManager::unloadPack(const std::string &packId) {
  auto it = m_packIdToIndex.find(packId);
  if (it == m_packIdToIndex.end())
    return;

  size_t index = it->second;

  // Close the pack reader
  if (m_packs[index]->reader) {
    m_packs[index]->reader->unmountAll();
  }

  // Remove from mod load order if applicable
  m_modLoadOrder.erase(
      std::remove(m_modLoadOrder.begin(), m_modLoadOrder.end(), packId),
      m_modLoadOrder.end());

  // Remove from packs list
  m_packs.erase(m_packs.begin() + static_cast<ptrdiff_t>(index));

  // Rebuild index maps
  m_packIdToIndex.clear();
  for (size_t i = 0; i < m_packs.size(); ++i) {
    m_packIdToIndex[m_packs[i]->info.id] = i;
  }

  rebuildResourceIndex();
  firePackUnloaded(packId);
}

void MultiPackManager::unloadPacksOfType(PackType type) {
  // Collect pack IDs to unload
  std::vector<std::string> toUnload;
  for (const auto &pack : m_packs) {
    if (pack->info.type == type) {
      toUnload.push_back(pack->info.id);
    }
  }

  // Unload each pack
  for (const auto &id : toUnload) {
    unloadPack(id);
  }
}

void MultiPackManager::unloadAllPacks() {
  for (auto &pack : m_packs) {
    if (pack->reader) {
      pack->reader->unmountAll();
    }
    firePackUnloaded(pack->info.id);
  }

  m_packs.clear();
  m_packIdToIndex.clear();
  m_resourceIndex.clear();
  m_modLoadOrder.clear();
}

PackLoadResult MultiPackManager::reloadPack(const std::string &packId) {
  auto it = m_packIdToIndex.find(packId);
  if (it == m_packIdToIndex.end()) {
    PackLoadResult result;
    result.success = false;
    result.errors.push_back("Pack not found: " + packId);
    return result;
  }

  std::string path = m_packs[it->second]->info.path;
  PackType type = m_packs[it->second]->info.type;
  i32 priority = m_packs[it->second]->info.priority;

  unloadPack(packId);
  return loadPack(path, type, priority);
}

void MultiPackManager::setPackEnabled(const std::string &packId, bool enabled) {
  auto it = m_packIdToIndex.find(packId);
  if (it != m_packIdToIndex.end()) {
    m_packs[it->second]->info.enabled = enabled;
    rebuildResourceIndex();
  }
}

// =========================================================================
// Pack Discovery
// =========================================================================

std::vector<DiscoveredPack>
MultiPackManager::discoverPacks(const std::string &directory) {
  std::vector<DiscoveredPack> discovered;

  if (!fs::exists(directory))
    return discovered;

  for (const auto &entry : fs::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file())
      continue;

    std::string ext = entry.path().extension().string();
    if (ext == ".nmpack" || ext == ".nmres") {
      DiscoveredPack pack;
      pack.path = entry.path().string();

      try {
        pack.info = readPackManifest(pack.path);
        pack.canLoad = true;

        // Check dependencies
        auto missing = getMissingDependencies(pack.info);
        if (!missing.empty()) {
          pack.loadError = "Missing dependencies: ";
          for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0)
              pack.loadError += ", ";
            pack.loadError += missing[i];
          }
        }
      } catch (const std::exception &e) {
        pack.canLoad = false;
        pack.loadError = e.what();
      }

      discovered.push_back(std::move(pack));
    }
  }

  return discovered;
}

std::vector<DiscoveredPack> MultiPackManager::discoverMods() {
  if (m_modsDirectory.empty())
    return {};
  return discoverPacks(m_modsDirectory);
}

std::vector<DiscoveredPack> MultiPackManager::discoverLanguagePacks() {
  std::vector<DiscoveredPack> langPacks;

  if (!m_packDirectory.empty()) {
    auto all = discoverPacks(m_packDirectory);
    for (auto &pack : all) {
      if (pack.info.type == PackType::Language) {
        langPacks.push_back(std::move(pack));
      }
    }
  }

  return langPacks;
}

bool MultiPackManager::areDependenciesSatisfied(const PackInfo &pack) const {
  return getMissingDependencies(pack).empty();
}

std::vector<std::string>
MultiPackManager::getMissingDependencies(const PackInfo &pack) const {
  std::vector<std::string> missing;

  for (const auto &dep : pack.dependencies) {
    if (!isPackLoaded(dep)) {
      missing.push_back(dep);
    }
  }

  return missing;
}

// =========================================================================
// Pack Information
// =========================================================================

const PackInfo *MultiPackManager::getPackInfo(const std::string &packId) const {
  auto it = m_packIdToIndex.find(packId);
  if (it != m_packIdToIndex.end()) {
    return &m_packs[it->second]->info;
  }
  return nullptr;
}

std::vector<const PackInfo *> MultiPackManager::getLoadedPacks() const {
  std::vector<const PackInfo *> result;
  result.reserve(m_packs.size());
  for (const auto &pack : m_packs) {
    result.push_back(&pack->info);
  }
  return result;
}

std::vector<const PackInfo *>
MultiPackManager::getPacksOfType(PackType type) const {
  std::vector<const PackInfo *> result;
  for (const auto &pack : m_packs) {
    if (pack->info.type == type) {
      result.push_back(&pack->info);
    }
  }
  return result;
}

bool MultiPackManager::isPackLoaded(const std::string &packId) const {
  return m_packIdToIndex.find(packId) != m_packIdToIndex.end();
}

bool MultiPackManager::isPackEnabled(const std::string &packId) const {
  auto it = m_packIdToIndex.find(packId);
  if (it != m_packIdToIndex.end()) {
    return m_packs[it->second]->info.enabled;
  }
  return false;
}

// =========================================================================
// Resource Access
// =========================================================================

Result<std::vector<u8>>
MultiPackManager::readResource(const std::string &resourceId) {
  auto it = m_resourceIndex.find(resourceId);
  if (it == m_resourceIndex.end()) {
    return Result<std::vector<u8>>::error("Resource not found: " + resourceId);
  }

  auto &pack = m_packs[it->second];
  if (!pack->info.enabled) {
    return Result<std::vector<u8>>::error("Pack is disabled: " + pack->info.id);
  }

  return pack->reader->readFile(resourceId);
}

bool MultiPackManager::exists(const std::string &resourceId) const {
  return m_resourceIndex.find(resourceId) != m_resourceIndex.end();
}

std::optional<ResourceInfo>
MultiPackManager::getResourceInfo(const std::string &resourceId) const {
  auto it = m_resourceIndex.find(resourceId);
  if (it == m_resourceIndex.end()) {
    return std::nullopt;
  }

  return m_packs[it->second]->reader->getInfo(resourceId);
}

std::string
MultiPackManager::getResourcePack(const std::string &resourceId) const {
  auto it = m_resourceIndex.find(resourceId);
  if (it != m_resourceIndex.end()) {
    return m_packs[it->second]->info.id;
  }
  return "";
}

std::vector<std::string>
MultiPackManager::listResources(ResourceType type) const {
  std::vector<std::string> result;

  for (const auto &[resourceId, packIndex] : m_resourceIndex) {
    if (type == ResourceType::Unknown) {
      result.push_back(resourceId);
    } else {
      auto info = m_packs[packIndex]->reader->getInfo(resourceId);
      if (info && info->type == type) {
        result.push_back(resourceId);
      }
    }
  }

  return result;
}

std::vector<ResourceOverride> MultiPackManager::getActiveOverrides() const {
  std::vector<ResourceOverride> overrides;

  // Track first occurrence of each resource
  std::unordered_map<std::string, std::pair<std::string, PackType>>
      firstOccurrence;

  // Process packs from lowest to highest priority
  std::vector<size_t> sortedIndices;
  for (size_t i = 0; i < m_packs.size(); ++i) {
    sortedIndices.push_back(i);
  }
  std::sort(
      sortedIndices.begin(), sortedIndices.end(), [this](size_t a, size_t b) {
        return m_packs[a]->effectivePriority < m_packs[b]->effectivePriority;
      });

  for (size_t idx : sortedIndices) {
    const auto &pack = m_packs[idx];
    if (!pack->info.enabled)
      continue;

    for (const auto &resourceId : pack->providedResources) {
      auto it = firstOccurrence.find(resourceId);
      if (it == firstOccurrence.end()) {
        firstOccurrence[resourceId] = {pack->info.id, pack->info.type};
      } else {
        // This is an override
        ResourceOverride override;
        override.resourceId = resourceId;
        override.originalPackId = it->second.first;
        override.overridePackId = pack->info.id;
        override.overrideType = pack->info.type;
        overrides.push_back(override);

        // Update first occurrence to current (since it's now the effective one)
        it->second = {pack->info.id, pack->info.type};
      }
    }
  }

  return overrides;
}

Result<std::vector<u8>>
MultiPackManager::readResourceFromPack(const std::string &packId,
                                       const std::string &resourceId) {
  auto it = m_packIdToIndex.find(packId);
  if (it == m_packIdToIndex.end()) {
    return Result<std::vector<u8>>::error("Pack not found: " + packId);
  }

  return m_packs[it->second]->reader->readFile(resourceId);
}

// =========================================================================
// Mod Support
// =========================================================================

std::vector<std::string> MultiPackManager::getModLoadOrder() const {
  return m_modLoadOrder;
}

void MultiPackManager::setModLoadOrder(const std::vector<std::string> &order) {
  m_modLoadOrder = order;

  // Update priorities based on order
  for (size_t i = 0; i < m_modLoadOrder.size(); ++i) {
    auto it = m_packIdToIndex.find(m_modLoadOrder[i]);
    if (it != m_packIdToIndex.end()) {
      m_packs[it->second]->info.priority = static_cast<i32>(i);
      m_packs[it->second]->effectivePriority =
          calculateEffectivePriority(PackType::Mod, static_cast<i32>(i));
    }
  }

  rebuildResourceIndex();
}

void MultiPackManager::moveModUp(const std::string &packId) {
  auto it = std::find(m_modLoadOrder.begin(), m_modLoadOrder.end(), packId);
  if (it != m_modLoadOrder.end() && it != m_modLoadOrder.begin()) {
    std::iter_swap(it, it - 1);
    setModLoadOrder(m_modLoadOrder);
  }
}

void MultiPackManager::moveModDown(const std::string &packId) {
  auto it = std::find(m_modLoadOrder.begin(), m_modLoadOrder.end(), packId);
  if (it != m_modLoadOrder.end() && it + 1 != m_modLoadOrder.end()) {
    std::iter_swap(it, it + 1);
    setModLoadOrder(m_modLoadOrder);
  }
}

Result<void> MultiPackManager::saveModConfig(const std::string &path) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  file << "{\n";
  file << "  \"modLoadOrder\": [\n";

  for (size_t i = 0; i < m_modLoadOrder.size(); ++i) {
    file << "    \"" << m_modLoadOrder[i] << "\"";
    if (i < m_modLoadOrder.size() - 1)
      file << ",";
    file << "\n";
  }

  file << "  ],\n";
  file << "  \"enabledMods\": [\n";

  std::vector<std::string> enabledMods;
  for (const auto &pack : m_packs) {
    if (pack->info.type == PackType::Mod && pack->info.enabled) {
      enabledMods.push_back(pack->info.id);
    }
  }

  for (size_t i = 0; i < enabledMods.size(); ++i) {
    file << "    \"" << enabledMods[i] << "\"";
    if (i < enabledMods.size() - 1)
      file << ",";
    file << "\n";
  }

  file << "  ]\n";
  file << "}\n";

  return {};
}

Result<void> MultiPackManager::loadModConfig(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for reading: " + path);
  }

  std::string content;
  if (!readFileToString(file, content)) {
    return Result<void>::error("Failed to read file: " + path);
  }

  // Parse mod load order
  std::string loadOrderPattern = "\"modLoadOrder\"\\s*:\\s*\\[([^\\]]*)\\]";
  std::regex loadOrderRegex(loadOrderPattern);
  std::smatch loadOrderMatch;
  if (std::regex_search(content, loadOrderMatch, loadOrderRegex)) {
    std::string orderStr = loadOrderMatch[1].str();
    std::string idPattern = "\"([^\"]+)\"";
    std::regex idRegex(idPattern);
    auto beginIt =
        std::sregex_iterator(orderStr.begin(), orderStr.end(), idRegex);
    auto endIt = std::sregex_iterator();

    std::vector<std::string> order;
    for (auto it = beginIt; it != endIt; ++it) {
      order.push_back((*it)[1].str());
    }
    setModLoadOrder(order);
  }

  // Parse enabled mods
  std::string enabledPattern = "\"enabledMods\"\\s*:\\s*\\[([^\\]]*)\\]";
  std::regex enabledRegex(enabledPattern);
  std::smatch enabledMatch;
  if (std::regex_search(content, enabledMatch, enabledRegex)) {
    std::string enabledStr = enabledMatch[1].str();
    std::string idPattern2 = "\"([^\"]+)\"";
    std::regex idRegex2(idPattern2);
    auto beginIt2 =
        std::sregex_iterator(enabledStr.begin(), enabledStr.end(), idRegex2);
    auto endIt2 = std::sregex_iterator();

    std::set<std::string> enabledMods;
    for (auto it = beginIt2; it != endIt2; ++it) {
      enabledMods.insert((*it)[1].str());
    }

    // Update enabled state
    for (auto &pack : m_packs) {
      if (pack->info.type == PackType::Mod) {
        pack->info.enabled = enabledMods.count(pack->info.id) > 0;
      }
    }

    rebuildResourceIndex();
  }

  return {};
}

// =========================================================================
// Statistics
// =========================================================================

size_t MultiPackManager::getPackCount() const { return m_packs.size(); }

size_t MultiPackManager::getResourceCount() const {
  return m_resourceIndex.size();
}

size_t MultiPackManager::getOverrideCount() const {
  return getActiveOverrides().size();
}

// =========================================================================
// Callbacks
// =========================================================================

void MultiPackManager::setOnPackLoaded(OnPackLoaded callback) {
  m_onPackLoaded = std::move(callback);
}

void MultiPackManager::setOnPackUnloaded(OnPackUnloaded callback) {
  m_onPackUnloaded = std::move(callback);
}

void MultiPackManager::setOnResourceOverridden(OnResourceOverridden callback) {
  m_onResourceOverridden = std::move(callback);
}

// =========================================================================
// Private Helpers
// =========================================================================

PackInfo MultiPackManager::readPackManifest(const std::string &path) {
  PackInfo info;

  // Generate ID from filename if no manifest
  info.id = fs::path(path).stem().string();
  info.name = info.id;
  info.path = path;
  info.fileSize = fs::file_size(path);

  // Try to read manifest from pack (if supported by pack format)
  // For now, use filename-based identification
  std::string filename = fs::path(path).filename().string();

  // Detect type from filename prefix
  if (filename.find("base_") == 0 || filename.find("game_") == 0) {
    info.type = PackType::Base;
  } else if (filename.find("patch_") == 0 || filename.find("update_") == 0) {
    info.type = PackType::Patch;
  } else if (filename.find("dlc_") == 0 || filename.find("expansion_") == 0) {
    info.type = PackType::DLC;
  } else if (filename.find("lang_") == 0 || filename.find("locale_") == 0) {
    info.type = PackType::Language;
  } else if (filename.find("mod_") == 0) {
    info.type = PackType::Mod;
  }

  return info;
}

void MultiPackManager::rebuildResourceIndex() {
  m_resourceIndex.clear();

  // Sort packs by effective priority (descending for override)
  std::vector<std::pair<size_t, i32>> sorted;
  for (size_t i = 0; i < m_packs.size(); ++i) {
    if (m_packs[i]->info.enabled) {
      sorted.emplace_back(i, m_packs[i]->effectivePriority);
    }
  }

  std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
    return a.second > b.second; // Higher priority first
  });

  // Build index (first occurrence wins due to descending sort)
  for (const auto &[idx, priority] : sorted) {
    for (const auto &resourceId : m_packs[idx]->providedResources) {
      if (m_resourceIndex.find(resourceId) == m_resourceIndex.end()) {
        m_resourceIndex[resourceId] = idx;
      }
    }
  }
}

i32 MultiPackManager::calculateEffectivePriority(PackType type,
                                                 i32 basePriority) const {
  i32 typeBase = 0;
  switch (type) {
  case PackType::Base:
    typeBase = PRIORITY_BASE;
    break;
  case PackType::Patch:
    typeBase = PRIORITY_PATCH;
    break;
  case PackType::DLC:
    typeBase = PRIORITY_DLC;
    break;
  case PackType::Language:
    typeBase = PRIORITY_LANGUAGE;
    break;
  case PackType::Mod:
    typeBase = PRIORITY_MOD;
    break;
  }

  return typeBase + basePriority;
}

void MultiPackManager::firePackLoaded(const PackInfo &info) {
  if (m_onPackLoaded) {
    m_onPackLoaded(info);
  }
}

void MultiPackManager::firePackUnloaded(const std::string &packId) {
  if (m_onPackUnloaded) {
    m_onPackUnloaded(packId);
  }
}

void MultiPackManager::fireResourceOverridden(
    const ResourceOverride &override) {
  if (m_onResourceOverridden) {
    m_onResourceOverridden(override);
  }
}

} // namespace NovelMind::vfs
