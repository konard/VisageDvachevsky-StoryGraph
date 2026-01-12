#pragma once

/**
 * @file localization_manager.hpp
 * @brief Localization System - Multi-language support for visual novels
 *
 * Provides comprehensive localization features:
 * - String table management
 * - Language switching at runtime
 * - Variable interpolation in localized strings
 * - Plural forms support
 * - Fallback to default locale
 * - CSV/JSON/PO import/export
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace NovelMind::localization {

/**
 * @brief Locale identifier
 */
struct LocaleId {
  LocaleId(std::string languageCode = {}, std::string regionCode = {})
      : language(std::move(languageCode)), region(std::move(regionCode)) {}

  std::string language; // ISO 639-1 code (e.g., "en", "ja", "ru")
  std::string region;   // ISO 3166-1 code (e.g., "US", "JP", "RU")

  [[nodiscard]] std::string toString() const {
    if (region.empty())
      return language;
    return language + "_" + region;
  }

  static LocaleId fromString(const std::string& str) {
    LocaleId id;
    auto pos = str.find('_');
    if (pos == std::string::npos) {
      pos = str.find('-');
    }

    if (pos != std::string::npos) {
      id.language = str.substr(0, pos);
      id.region = str.substr(pos + 1);
    } else {
      id.language = str;
    }
    return id;
  }

  bool operator==(const LocaleId& other) const {
    return language == other.language && region == other.region;
  }
};

/**
 * @brief Hash function for LocaleId
 */
struct LocaleIdHash {
  size_t operator()(const LocaleId& id) const { return std::hash<std::string>{}(id.toString()); }
};

/**
 * @brief Plural form category
 */
enum class PluralCategory : u8 { Zero, One, Two, Few, Many, Other };

/**
 * @brief Localized string with optional plural forms
 */
struct LocalizedString {
  std::string id;                                        // String ID
  std::unordered_map<PluralCategory, std::string> forms; // Plural forms
  std::string context;                                   // Optional context/notes
  std::string source;                                    // Source file reference
  u32 lineNumber = 0;                                    // Source line number
};

/**
 * @brief String table for a single locale
 */
class StringTable {
public:
  StringTable() = default;
  explicit StringTable(const LocaleId& locale);

  void setLocale(const LocaleId& locale) { m_locale = locale; }
  [[nodiscard]] const LocaleId& getLocale() const { return m_locale; }

  /**
   * @brief Add a string to the table
   */
  void addString(const std::string& id, const std::string& value);

  /**
   * @brief Add a string with plural forms
   */
  void addPluralString(const std::string& id,
                       const std::unordered_map<PluralCategory, std::string>& forms);

  /**
   * @brief Get a string by ID
   */
  [[nodiscard]] std::optional<std::string> getString(const std::string& id) const;

  /**
   * @brief Get a plural string by ID and count
   */
  [[nodiscard]] std::optional<std::string> getPluralString(const std::string& id, i64 count) const;

  /**
   * @brief Check if a string exists
   */
  [[nodiscard]] bool hasString(const std::string& id) const;

  /**
   * @brief Get all string IDs
   */
  [[nodiscard]] std::vector<std::string> getStringIds() const;

  /**
   * @brief Get all localized strings
   */
  [[nodiscard]] const std::unordered_map<std::string, LocalizedString>& getStrings() const {
    return m_strings;
  }

  /**
   * @brief Get number of strings
   */
  [[nodiscard]] size_t size() const { return m_strings.size(); }

  /**
   * @brief Clear all strings
   */
  void clear();

  /**
   * @brief Remove a string by ID
   */
  void removeString(const std::string& id);

private:
  LocaleId m_locale;
  std::unordered_map<std::string, LocalizedString> m_strings;
};

/**
 * @brief Import/export format for localization files
 */
enum class LocalizationFormat : u8 {
  CSV,  // Comma-separated values
  JSON, // JSON format
  PO,   // GNU Gettext PO format
  XLIFF // XML Localization Interchange File Format
};

/**
 * @brief Locale configuration
 */
struct LocaleConfig {
  std::string displayName;  // Human-readable name (e.g., "English", "日本語")
  std::string nativeName;   // Name in native language
  bool rightToLeft = false; // RTL text direction
  std::string fontOverride; // Optional font override for this locale
  std::string numberFormat; // Number formatting pattern
  std::string dateFormat;   // Date formatting pattern
};

/**
 * @brief Callback types
 */
using OnLanguageChanged = std::function<void(const LocaleId& newLocale)>;
using OnStringMissing = std::function<void(const std::string& stringId, const LocaleId& locale)>;

/**
 * @brief Localization Manager - Central localization management
 *
 * The Localization Manager provides a complete solution for multi-language
 * support in visual novels:
 *
 * 1. Load string tables for multiple languages
 * 2. Runtime language switching
 * 3. Variable interpolation: "Hello, {name}!"
 * 4. Plural forms: "{count} apple" vs "{count} apples"
 * 5. Fallback to default locale for missing strings
 *
 * Example usage:
 * @code
 * LocalizationManager loc;
 * loc.setDefaultLocale(LocaleId::fromString("en"));
 *
 * // Load English strings
 * loc.loadStrings("en", "locales/en.json", LocalizationFormat::JSON);
 *
 * // Load Japanese strings
 * loc.loadStrings("ja", "locales/ja.json", LocalizationFormat::JSON);
 *
 * // Get localized string
 * std::string greeting = loc.get("greeting"); // "Hello!"
 *
 * // Get with variable interpolation
 * std::string hello = loc.get("hello_name", {{"name", "Alex"}}); // "Hello,
 * Alex!"
 *
 * // Get plural form
 * std::string items = loc.getPlural("item_count", 5); // "5 items"
 *
 * // Switch language
 * loc.setCurrentLocale(LocaleId::fromString("ja"));
 * @endcode
 */
class LocalizationManager {
public:
  LocalizationManager();
  ~LocalizationManager();

  // Non-copyable
  LocalizationManager(const LocalizationManager&) = delete;
  LocalizationManager& operator=(const LocalizationManager&) = delete;

  // =========================================================================
  // Locale Management
  // =========================================================================

  /**
   * @brief Set the default/fallback locale
   */
  void setDefaultLocale(const LocaleId& locale);

  /**
   * @brief Get the default locale
   */
  [[nodiscard]] const LocaleId& getDefaultLocale() const { return m_defaultLocale; }

  /**
   * @brief Set the current locale
   */
  void setCurrentLocale(const LocaleId& locale);

  /**
   * @brief Get the current locale
   */
  [[nodiscard]] const LocaleId& getCurrentLocale() const { return m_currentLocale; }

  /**
   * @brief Get list of available locales
   */
  [[nodiscard]] std::vector<LocaleId> getAvailableLocales() const;

  /**
   * @brief Check if a locale is available
   */
  [[nodiscard]] bool isLocaleAvailable(const LocaleId& locale) const;

  /**
   * @brief Register locale configuration
   */
  void registerLocale(const LocaleId& locale, const LocaleConfig& config);

  /**
   * @brief Get locale configuration
   */
  [[nodiscard]] std::optional<LocaleConfig> getLocaleConfig(const LocaleId& locale) const;

  /**
   * @brief Check if locale uses right-to-left script
   */
  [[nodiscard]] bool isRightToLeft(const LocaleId& locale) const;

  /**
   * @brief Check if current locale uses right-to-left script
   */
  [[nodiscard]] bool isCurrentLocaleRightToLeft() const;

  // =========================================================================
  // String Loading
  // =========================================================================

  /**
   * @brief Load strings from file
   * @param locale Target locale
   * @param filePath Path to localization file
   * @param format File format
   * @return Success or error
   */
  Result<void> loadStrings(const LocaleId& locale, const std::string& filePath,
                           LocalizationFormat format);

  /**
   * @brief Load strings from memory
   * @param locale Target locale
   * @param data File content
   * @param format File format
   * @return Success or error
   */
  Result<void> loadStringsFromMemory(const LocaleId& locale, const std::string& data,
                                     LocalizationFormat format);

  /**
   * @brief Merge additional strings into existing table
   */
  Result<void> mergeStrings(const LocaleId& locale, const std::string& filePath,
                            LocalizationFormat format);

  /**
   * @brief Unload strings for a locale
   */
  void unloadLocale(const LocaleId& locale);

  /**
   * @brief Clear all loaded strings
   */
  void clearAll();

  // =========================================================================
  // String Retrieval
  // =========================================================================

  /**
   * @brief Get localized string
   * @param id String ID
   * @return Localized string or ID if not found
   */
  [[nodiscard]] std::string get(const std::string& id) const;

  /**
   * @brief Get localized string with variable interpolation
   * @param id String ID
   * @param variables Map of variable names to values
   * @return Interpolated localized string
   */
  [[nodiscard]] std::string
  get(const std::string& id, const std::unordered_map<std::string, std::string>& variables) const;

  /**
   * @brief Get localized plural string
   * @param id String ID
   * @param count Count for plural selection
   * @return Plural-aware localized string
   */
  [[nodiscard]] std::string getPlural(const std::string& id, i64 count) const;

  /**
   * @brief Get localized plural string with variables
   */
  [[nodiscard]] std::string
  getPlural(const std::string& id, i64 count,
            const std::unordered_map<std::string, std::string>& variables) const;

  /**
   * @brief Get string for specific locale (bypassing current locale)
   */
  [[nodiscard]] std::string getForLocale(const LocaleId& locale, const std::string& id) const;

  /**
   * @brief Check if string exists in current locale
   */
  [[nodiscard]] bool hasString(const std::string& id) const;

  /**
   * @brief Check if string exists in specific locale
   */
  [[nodiscard]] bool hasString(const LocaleId& locale, const std::string& id) const;

  // =========================================================================
  // String Editing (for Editor)
  // =========================================================================

  /**
   * @brief Add or update a string
   */
  void setString(const LocaleId& locale, const std::string& id, const std::string& value);

  /**
   * @brief Remove a string
   */
  void removeString(const LocaleId& locale, const std::string& id);

  /**
   * @brief Get string table for a locale
   */
  [[nodiscard]] const StringTable* getStringTable(const LocaleId& locale) const;

  /**
   * @brief Get mutable string table for a locale
   */
  StringTable* getStringTableMutable(const LocaleId& locale);

  // =========================================================================
  // Export
  // =========================================================================

  /**
   * @brief Export strings to file
   */
  Result<void> exportStrings(const LocaleId& locale, const std::string& filePath,
                             LocalizationFormat format) const;

  /**
   * @brief Export missing strings (strings in default but not in target locale)
   */
  Result<void> exportMissingStrings(const LocaleId& locale, const std::string& filePath,
                                    LocalizationFormat format) const;

  // =========================================================================
  // Callbacks
  // =========================================================================

  void setOnLanguageChanged(OnLanguageChanged callback);
  void setOnStringMissing(OnStringMissing callback);

  // =========================================================================
  // Utility
  // =========================================================================

  /**
   * @brief Get plural category for a count in current locale
   */
  [[nodiscard]] PluralCategory getPluralCategory(i64 count) const;

  /**
   * @brief Get plural category for a specific locale
   */
  [[nodiscard]] PluralCategory getPluralCategory(const LocaleId& locale, i64 count) const;

  /**
   * @brief Interpolate variables into a string
   */
  [[nodiscard]] std::string
  interpolate(const std::string& text,
              const std::unordered_map<std::string, std::string>& variables) const;

private:
  // Internal helpers
  Result<void> loadCSV(const LocaleId& locale, const std::string& content);
  Result<void> loadJSON(const LocaleId& locale, const std::string& content);
  Result<void> loadPO(const LocaleId& locale, const std::string& content);
  Result<void> loadXLIFF(const LocaleId& locale, const std::string& content);

  Result<void> exportCSV(const StringTable& table, const std::string& path) const;
  Result<void> exportJSON(const StringTable& table, const std::string& path) const;
  Result<void> exportPO(const StringTable& table, const std::string& path) const;
  Result<void> exportXLIFF(const StringTable& table, const std::string& path) const;

  StringTable& getOrCreateTable(const LocaleId& locale);
  void fireMissingString(const std::string& id, const LocaleId& locale) const;

  // Locale data
  LocaleId m_defaultLocale;
  LocaleId m_currentLocale;
  std::unordered_map<LocaleId, StringTable, LocaleIdHash> m_stringTables;
  std::unordered_map<LocaleId, LocaleConfig, LocaleIdHash> m_localeConfigs;

  // Callbacks
  OnLanguageChanged m_onLanguageChanged;
  mutable OnStringMissing m_onStringMissing;
};

} // namespace NovelMind::localization
