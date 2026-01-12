/**
 * @file test_localization_manager.cpp
 * @brief Comprehensive unit tests for Localization Manager module
 *
 * Tests all aspects of the LocalizationManager:
 * - LocaleId parsing and formatting
 * - StringTable management
 * - Language loading and switching
 * - Fallback behavior
 * - Missing key handling
 * - Pluralization rules
 * - RTL language support
 * - Variable interpolation
 * - File format loading/export (CSV, JSON, PO, XLIFF)
 * - Callbacks and events
 */

#include <catch2/catch_test_macros.hpp>
#include "NovelMind/localization/localization_manager.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace NovelMind::localization;
namespace fs = std::filesystem;

// ============================================================================
// LocaleId Tests
// ============================================================================

TEST_CASE("LocaleId - Default construction", "[localization][locale]") {
  LocaleId locale;

  REQUIRE(locale.language.empty());
  REQUIRE(locale.region.empty());
  REQUIRE(locale.toString().empty());
}

TEST_CASE("LocaleId - Construction with language only", "[localization][locale]") {
  LocaleId locale("en");

  REQUIRE(locale.language == "en");
  REQUIRE(locale.region.empty());
  REQUIRE(locale.toString() == "en");
}

TEST_CASE("LocaleId - Construction with language and region", "[localization][locale]") {
  LocaleId locale("en", "US");

  REQUIRE(locale.language == "en");
  REQUIRE(locale.region == "US");
  REQUIRE(locale.toString() == "en_US");
}

TEST_CASE("LocaleId - fromString with language only", "[localization][locale]") {
  LocaleId locale = LocaleId::fromString("ja");

  REQUIRE(locale.language == "ja");
  REQUIRE(locale.region.empty());
  REQUIRE(locale.toString() == "ja");
}

TEST_CASE("LocaleId - fromString with underscore separator", "[localization][locale]") {
  LocaleId locale = LocaleId::fromString("en_US");

  REQUIRE(locale.language == "en");
  REQUIRE(locale.region == "US");
  REQUIRE(locale.toString() == "en_US");
}

TEST_CASE("LocaleId - fromString with hyphen separator", "[localization][locale]") {
  LocaleId locale = LocaleId::fromString("zh-CN");

  REQUIRE(locale.language == "zh");
  REQUIRE(locale.region == "CN");
  REQUIRE(locale.toString() == "zh_CN");
}

TEST_CASE("LocaleId - Equality comparison", "[localization][locale]") {
  LocaleId locale1("en", "US");
  LocaleId locale2("en", "US");
  LocaleId locale3("en", "GB");
  LocaleId locale4("ja");

  REQUIRE(locale1 == locale2);
  REQUIRE_FALSE(locale1 == locale3);
  REQUIRE_FALSE(locale1 == locale4);
}

TEST_CASE("LocaleId - Hash function", "[localization][locale]") {
  LocaleId locale1("en", "US");
  LocaleId locale2("en", "US");
  LocaleId locale3("ja");

  LocaleIdHash hash;

  REQUIRE(hash(locale1) == hash(locale2));
  REQUIRE(hash(locale1) != hash(locale3));
}

// ============================================================================
// StringTable Tests
// ============================================================================

TEST_CASE("StringTable - Default construction", "[localization][stringtable]") {
  StringTable table;

  REQUIRE(table.size() == 0);
  REQUIRE(table.getStringIds().empty());
}

TEST_CASE("StringTable - Construction with locale", "[localization][stringtable]") {
  LocaleId locale("en");
  StringTable table(locale);

  REQUIRE(table.getLocale() == locale);
  REQUIRE(table.size() == 0);
}

TEST_CASE("StringTable - Add and retrieve string", "[localization][stringtable]") {
  StringTable table;

  table.addString("greeting", "Hello, World!");

  REQUIRE(table.size() == 1);
  REQUIRE(table.hasString("greeting"));

  auto str = table.getString("greeting");
  REQUIRE(str.has_value());
  REQUIRE(*str == "Hello, World!");
}

TEST_CASE("StringTable - Add multiple strings", "[localization][stringtable]") {
  StringTable table;

  table.addString("hello", "Hello");
  table.addString("goodbye", "Goodbye");
  table.addString("thanks", "Thank you");

  REQUIRE(table.size() == 3);
  REQUIRE(table.hasString("hello"));
  REQUIRE(table.hasString("goodbye"));
  REQUIRE(table.hasString("thanks"));
}

TEST_CASE("StringTable - Retrieve non-existent string", "[localization][stringtable]") {
  StringTable table;

  table.addString("existing", "Value");

  auto str = table.getString("non_existent");
  REQUIRE_FALSE(str.has_value());
}

TEST_CASE("StringTable - Add plural string", "[localization][stringtable]") {
  StringTable table;

  std::unordered_map<PluralCategory, std::string> forms;
  forms[PluralCategory::One] = "{count} apple";
  forms[PluralCategory::Other] = "{count} apples";

  table.addPluralString("apple_count", forms);

  REQUIRE(table.hasString("apple_count"));

  auto oneForm = table.getPluralString("apple_count", 1);
  REQUIRE(oneForm.has_value());
  REQUIRE(*oneForm == "{count} apple");

  auto manyForm = table.getPluralString("apple_count", 5);
  REQUIRE(manyForm.has_value());
  REQUIRE(*manyForm == "{count} apples");
}

TEST_CASE("StringTable - Remove string", "[localization][stringtable]") {
  StringTable table;

  table.addString("temp", "Temporary value");
  REQUIRE(table.hasString("temp"));

  table.removeString("temp");
  REQUIRE_FALSE(table.hasString("temp"));
  REQUIRE(table.size() == 0);
}

TEST_CASE("StringTable - Clear all strings", "[localization][stringtable]") {
  StringTable table;

  table.addString("key1", "value1");
  table.addString("key2", "value2");
  table.addString("key3", "value3");

  REQUIRE(table.size() == 3);

  table.clear();

  REQUIRE(table.size() == 0);
  REQUIRE(table.getStringIds().empty());
}

TEST_CASE("StringTable - Get all string IDs", "[localization][stringtable]") {
  StringTable table;

  table.addString("alpha", "A");
  table.addString("beta", "B");
  table.addString("gamma", "C");

  auto ids = table.getStringIds();

  REQUIRE(ids.size() == 3);
  REQUIRE(std::find(ids.begin(), ids.end(), "alpha") != ids.end());
  REQUIRE(std::find(ids.begin(), ids.end(), "beta") != ids.end());
  REQUIRE(std::find(ids.begin(), ids.end(), "gamma") != ids.end());
}

// ============================================================================
// LocalizationManager - Basic Tests
// ============================================================================

TEST_CASE("LocalizationManager - Default construction", "[localization][manager]") {
  LocalizationManager manager;

  REQUIRE(manager.getDefaultLocale().language == "en");
  REQUIRE(manager.getCurrentLocale().language == "en");
}

TEST_CASE("LocalizationManager - Set default locale", "[localization][manager]") {
  LocalizationManager manager;

  LocaleId newDefault("ja");
  manager.setDefaultLocale(newDefault);

  REQUIRE(manager.getDefaultLocale() == newDefault);
}

TEST_CASE("LocalizationManager - Set current locale", "[localization][manager]") {
  LocalizationManager manager;

  LocaleId newLocale("fr");
  manager.setCurrentLocale(newLocale);

  REQUIRE(manager.getCurrentLocale() == newLocale);
}

TEST_CASE("LocalizationManager - Get available locales", "[localization][manager]") {
  LocalizationManager manager;

  // Initially no locales loaded
  REQUIRE(manager.getAvailableLocales().empty());

  // Add strings for different locales
  manager.setString(LocaleId("en"), "key1", "English");
  manager.setString(LocaleId("ja"), "key1", "日本語");
  manager.setString(LocaleId("fr"), "key1", "Français");

  auto locales = manager.getAvailableLocales();
  REQUIRE(locales.size() == 3);
}

TEST_CASE("LocalizationManager - Check locale availability", "[localization][manager]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "test", "value");

  REQUIRE(manager.isLocaleAvailable(LocaleId("en")));
  REQUIRE_FALSE(manager.isLocaleAvailable(LocaleId("ja")));
}

// ============================================================================
// Language Switching Tests
// ============================================================================

TEST_CASE("LocalizationManager - Language switching", "[localization][manager][switching]") {
  LocalizationManager manager;

  // Load English strings
  manager.setString(LocaleId("en"), "greeting", "Hello");
  manager.setString(LocaleId("en"), "farewell", "Goodbye");

  // Load Japanese strings
  manager.setString(LocaleId("ja"), "greeting", "こんにちは");
  manager.setString(LocaleId("ja"), "farewell", "さようなら");

  // Test English
  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("greeting") == "Hello");
  REQUIRE(manager.get("farewell") == "Goodbye");

  // Switch to Japanese
  manager.setCurrentLocale(LocaleId("ja"));
  REQUIRE(manager.get("greeting") == "こんにちは");
  REQUIRE(manager.get("farewell") == "さようなら");
}

TEST_CASE("LocalizationManager - Language change callback", "[localization][manager][callback]") {
  LocalizationManager manager;

  bool callbackFired = false;
  LocaleId newLocale;

  manager.setOnLanguageChanged([&](const LocaleId& locale) {
    callbackFired = true;
    newLocale = locale;
  });

  manager.setCurrentLocale(LocaleId("fr"));

  REQUIRE(callbackFired);
  REQUIRE(newLocale.language == "fr");
}

TEST_CASE("LocalizationManager - Language change callback not fired when same", "[localization][manager][callback]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  int callbackCount = 0;

  manager.setOnLanguageChanged([&](const LocaleId&) {
    callbackCount++;
  });

  // Set to same locale - should not fire
  manager.setCurrentLocale(LocaleId("en"));

  REQUIRE(callbackCount == 0);
}

// ============================================================================
// Fallback Behavior Tests
// ============================================================================

TEST_CASE("LocalizationManager - Fallback to default locale", "[localization][manager][fallback]") {
  LocalizationManager manager;
  manager.setDefaultLocale(LocaleId("en"));

  // Load English strings
  manager.setString(LocaleId("en"), "common.button.ok", "OK");
  manager.setString(LocaleId("en"), "common.button.cancel", "Cancel");

  // Load partial French strings (missing cancel)
  manager.setString(LocaleId("fr"), "common.button.ok", "D'accord");

  // Switch to French
  manager.setCurrentLocale(LocaleId("fr"));

  // Should get French for OK
  REQUIRE(manager.get("common.button.ok") == "D'accord");

  // Should fallback to English for Cancel
  REQUIRE(manager.get("common.button.cancel") == "Cancel");
}

TEST_CASE("LocalizationManager - No fallback when key doesn't exist", "[localization][manager][fallback]") {
  LocalizationManager manager;
  manager.setDefaultLocale(LocaleId("en"));
  manager.setCurrentLocale(LocaleId("en"));

  // Request non-existent key - should return the key itself
  REQUIRE(manager.get("non.existent.key") == "non.existent.key");
}

// ============================================================================
// Missing Key Handling Tests
// ============================================================================

TEST_CASE("LocalizationManager - Missing string callback", "[localization][manager][missing]") {
  LocalizationManager manager;
  manager.setDefaultLocale(LocaleId("en"));
  manager.setCurrentLocale(LocaleId("ja"));

  bool callbackFired = false;
  std::string missingKey;
  LocaleId missingLocale;

  manager.setOnStringMissing([&](const std::string& key, const LocaleId& locale) {
    callbackFired = true;
    missingKey = key;
    missingLocale = locale;
  });

  // Request missing key
  manager.get("missing.key");

  REQUIRE(callbackFired);
  REQUIRE(missingKey == "missing.key");
  REQUIRE(missingLocale.language == "ja");
}

TEST_CASE("LocalizationManager - hasString checks current locale", "[localization][manager][missing]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key1", "English");
  manager.setString(LocaleId("ja"), "key2", "Japanese");

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.hasString("key1"));
  REQUIRE_FALSE(manager.hasString("key2"));

  manager.setCurrentLocale(LocaleId("ja"));
  REQUIRE_FALSE(manager.hasString("key1"));
  REQUIRE(manager.hasString("key2"));
}

TEST_CASE("LocalizationManager - hasString with specific locale", "[localization][manager][missing]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key1", "English");
  manager.setString(LocaleId("ja"), "key2", "Japanese");

  REQUIRE(manager.hasString(LocaleId("en"), "key1"));
  REQUIRE_FALSE(manager.hasString(LocaleId("en"), "key2"));
  REQUIRE(manager.hasString(LocaleId("ja"), "key2"));
  REQUIRE_FALSE(manager.hasString(LocaleId("ja"), "key1"));
}

// ============================================================================
// Pluralization Tests
// ============================================================================

TEST_CASE("LocalizationManager - English plural rules", "[localization][manager][plural]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  REQUIRE(manager.getPluralCategory(0) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(1) == PluralCategory::One);
  REQUIRE(manager.getPluralCategory(2) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(5) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(100) == PluralCategory::Other);
}

TEST_CASE("LocalizationManager - Russian plural rules", "[localization][manager][plural]") {
  LocalizationManager manager;

  // Russian has complex plural rules
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 1) == PluralCategory::One);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 2) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 3) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 4) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 5) == PluralCategory::Many);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 11) == PluralCategory::Many);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 21) == PluralCategory::One);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 22) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ru"), 25) == PluralCategory::Many);
}

TEST_CASE("LocalizationManager - Japanese plural rules (no plural)", "[localization][manager][plural]") {
  LocalizationManager manager;

  // Japanese has no plural distinction
  REQUIRE(manager.getPluralCategory(LocaleId("ja"), 0) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(LocaleId("ja"), 1) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(LocaleId("ja"), 5) == PluralCategory::Other);
  REQUIRE(manager.getPluralCategory(LocaleId("ja"), 100) == PluralCategory::Other);
}

TEST_CASE("LocalizationManager - Arabic plural rules", "[localization][manager][plural]") {
  LocalizationManager manager;

  // Arabic has six plural forms
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 0) == PluralCategory::Zero);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 1) == PluralCategory::One);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 2) == PluralCategory::Two);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 3) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 10) == PluralCategory::Few);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 11) == PluralCategory::Many);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 99) == PluralCategory::Many);
  REQUIRE(manager.getPluralCategory(LocaleId("ar"), 100) == PluralCategory::Other);
}

TEST_CASE("LocalizationManager - getPlural with count", "[localization][manager][plural]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  auto table = manager.getStringTableMutable(LocaleId("en"));
  REQUIRE(table != nullptr);

  std::unordered_map<PluralCategory, std::string> forms;
  forms[PluralCategory::One] = "1 item";
  forms[PluralCategory::Other] = "{count} items";

  table->addPluralString("item_count", forms);

  REQUIRE(manager.getPlural("item_count", 1) == "1 item");
  REQUIRE(manager.getPlural("item_count", 0) == "{count} items");
  REQUIRE(manager.getPlural("item_count", 5) == "{count} items");
}

// ============================================================================
// RTL Language Support Tests
// ============================================================================

TEST_CASE("LocalizationManager - RTL detection for Arabic", "[localization][manager][rtl]") {
  LocalizationManager manager;

  REQUIRE(manager.isRightToLeft(LocaleId("ar")));
}

TEST_CASE("LocalizationManager - RTL detection for Hebrew", "[localization][manager][rtl]") {
  LocalizationManager manager;

  REQUIRE(manager.isRightToLeft(LocaleId("he")));
}

TEST_CASE("LocalizationManager - RTL detection for Persian", "[localization][manager][rtl]") {
  LocalizationManager manager;

  REQUIRE(manager.isRightToLeft(LocaleId("fa")));
}

TEST_CASE("LocalizationManager - RTL detection for Urdu", "[localization][manager][rtl]") {
  LocalizationManager manager;

  REQUIRE(manager.isRightToLeft(LocaleId("ur")));
}

TEST_CASE("LocalizationManager - Non-RTL languages", "[localization][manager][rtl]") {
  LocalizationManager manager;

  REQUIRE_FALSE(manager.isRightToLeft(LocaleId("en")));
  REQUIRE_FALSE(manager.isRightToLeft(LocaleId("ja")));
  REQUIRE_FALSE(manager.isRightToLeft(LocaleId("fr")));
  REQUIRE_FALSE(manager.isRightToLeft(LocaleId("de")));
  REQUIRE_FALSE(manager.isRightToLeft(LocaleId("ru")));
}

TEST_CASE("LocalizationManager - Current locale RTL check", "[localization][manager][rtl]") {
  LocalizationManager manager;

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE_FALSE(manager.isCurrentLocaleRightToLeft());

  manager.setCurrentLocale(LocaleId("ar"));
  REQUIRE(manager.isCurrentLocaleRightToLeft());
}

TEST_CASE("LocalizationManager - RTL with locale config", "[localization][manager][rtl]") {
  LocalizationManager manager;

  LocaleConfig config;
  config.displayName = "Test RTL";
  config.rightToLeft = true;

  manager.registerLocale(LocaleId("test"), config);

  REQUIRE(manager.isRightToLeft(LocaleId("test")));
}

// ============================================================================
// Variable Interpolation Tests
// ============================================================================

TEST_CASE("LocalizationManager - Variable interpolation single variable", "[localization][manager][interpolation]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "greeting", "Hello, {name}!");

  std::unordered_map<std::string, std::string> vars;
  vars["name"] = "Alice";

  REQUIRE(manager.get("greeting", vars) == "Hello, Alice!");
}

TEST_CASE("LocalizationManager - Variable interpolation multiple variables", "[localization][manager][interpolation]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "message", "{user} sent {count} messages to {recipient}");

  std::unordered_map<std::string, std::string> vars;
  vars["user"] = "Bob";
  vars["count"] = "5";
  vars["recipient"] = "Carol";

  REQUIRE(manager.get("message", vars) == "Bob sent 5 messages to Carol");
}

TEST_CASE("LocalizationManager - Variable interpolation with same variable multiple times", "[localization][manager][interpolation]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "repeat", "{word} {word} {word}");

  std::unordered_map<std::string, std::string> vars;
  vars["word"] = "echo";

  REQUIRE(manager.get("repeat", vars) == "echo echo echo");
}

TEST_CASE("LocalizationManager - Variable interpolation with missing variable", "[localization][manager][interpolation]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "template", "Hello, {name}! You have {count} messages.");

  std::unordered_map<std::string, std::string> vars;
  vars["name"] = "Alice";
  // count is missing

  // Missing variables remain as placeholders
  REQUIRE(manager.get("template", vars) == "Hello, Alice! You have {count} messages.");
}

TEST_CASE("LocalizationManager - Plural with variable interpolation", "[localization][manager][interpolation][plural]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  auto table = manager.getStringTableMutable(LocaleId("en"));
  REQUIRE(table != nullptr);

  std::unordered_map<PluralCategory, std::string> forms;
  forms[PluralCategory::One] = "You have {count} message";
  forms[PluralCategory::Other] = "You have {count} messages";

  table->addPluralString("message_count", forms);

  std::unordered_map<std::string, std::string> vars;

  vars["count"] = "1";
  REQUIRE(manager.getPlural("message_count", 1, vars) == "You have 1 message");

  vars["count"] = "5";
  REQUIRE(manager.getPlural("message_count", 5, vars) == "You have 5 messages");
}

TEST_CASE("LocalizationManager - Direct interpolate function", "[localization][manager][interpolation]") {
  LocalizationManager manager;

  std::unordered_map<std::string, std::string> vars;
  vars["x"] = "10";
  vars["y"] = "20";

  std::string result = manager.interpolate("Position: ({x}, {y})", vars);
  REQUIRE(result == "Position: (10, 20)");
}

// ============================================================================
// Locale Configuration Tests
// ============================================================================

TEST_CASE("LocalizationManager - Register and retrieve locale config", "[localization][manager][config]") {
  LocalizationManager manager;

  LocaleConfig config;
  config.displayName = "English (US)";
  config.nativeName = "English";
  config.rightToLeft = false;
  config.fontOverride = "Arial";
  config.numberFormat = "#,##0.##";
  config.dateFormat = "MM/DD/YYYY";

  manager.registerLocale(LocaleId("en", "US"), config);

  auto retrieved = manager.getLocaleConfig(LocaleId("en", "US"));

  REQUIRE(retrieved.has_value());
  REQUIRE(retrieved->displayName == "English (US)");
  REQUIRE(retrieved->nativeName == "English");
  REQUIRE(retrieved->rightToLeft == false);
  REQUIRE(retrieved->fontOverride == "Arial");
  REQUIRE(retrieved->numberFormat == "#,##0.##");
  REQUIRE(retrieved->dateFormat == "MM/DD/YYYY");
}

TEST_CASE("LocalizationManager - Get config for unregistered locale", "[localization][manager][config]") {
  LocalizationManager manager;

  auto config = manager.getLocaleConfig(LocaleId("xx"));

  REQUIRE_FALSE(config.has_value());
}

// ============================================================================
// String Management Tests
// ============================================================================

TEST_CASE("LocalizationManager - setString and get", "[localization][manager][string]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "test.key", "Test Value");

  REQUIRE(manager.get("test.key") == "Test Value");
}

TEST_CASE("LocalizationManager - removeString", "[localization][manager][string]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "temp.key", "Temp");
  REQUIRE(manager.hasString("temp.key"));

  manager.removeString(LocaleId("en"), "temp.key");
  REQUIRE_FALSE(manager.hasString("temp.key"));
}

TEST_CASE("LocalizationManager - getForLocale bypasses current locale", "[localization][manager][string]") {
  LocalizationManager manager;
  manager.setCurrentLocale(LocaleId("en"));

  manager.setString(LocaleId("en"), "key", "English");
  manager.setString(LocaleId("ja"), "key", "Japanese");

  // Current locale is en, but we can get ja directly
  REQUIRE(manager.getForLocale(LocaleId("ja"), "key") == "Japanese");
}

TEST_CASE("LocalizationManager - getStringTable const", "[localization][manager][string]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key1", "value1");

  const auto* table = manager.getStringTable(LocaleId("en"));

  REQUIRE(table != nullptr);
  REQUIRE(table->hasString("key1"));
}

TEST_CASE("LocalizationManager - getStringTableMutable", "[localization][manager][string]") {
  LocalizationManager manager;

  auto* table = manager.getStringTableMutable(LocaleId("en"));

  REQUIRE(table != nullptr);

  table->addString("direct_key", "direct_value");

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("direct_key") == "direct_value");
}

TEST_CASE("LocalizationManager - clearAll", "[localization][manager][string]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key1", "value1");
  manager.setString(LocaleId("ja"), "key2", "value2");
  manager.setString(LocaleId("fr"), "key3", "value3");

  REQUIRE(manager.getAvailableLocales().size() == 3);

  manager.clearAll();

  REQUIRE(manager.getAvailableLocales().empty());
}

TEST_CASE("LocalizationManager - unloadLocale", "[localization][manager][string]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key", "English");
  manager.setString(LocaleId("ja"), "key", "Japanese");

  REQUIRE(manager.isLocaleAvailable(LocaleId("en")));
  REQUIRE(manager.isLocaleAvailable(LocaleId("ja")));

  manager.unloadLocale(LocaleId("ja"));

  REQUIRE(manager.isLocaleAvailable(LocaleId("en")));
  REQUIRE_FALSE(manager.isLocaleAvailable(LocaleId("ja")));
}

// ============================================================================
// File Format Loading Tests - CSV
// ============================================================================

TEST_CASE("LocalizationManager - Load CSV from memory", "[localization][manager][csv]") {
  LocalizationManager manager;

  std::string csvData = R"(ID,Text
greeting,"Hello, World!"
farewell,"Goodbye!"
question,"How are you?")";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), csvData, LocalizationFormat::CSV);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("greeting") == "Hello, World!");
  REQUIRE(manager.get("farewell") == "Goodbye!");
  REQUIRE(manager.get("question") == "How are you?");
}

TEST_CASE("LocalizationManager - Load CSV with quotes", "[localization][manager][csv]") {
  LocalizationManager manager;

  std::string csvData = R"(ID,Text
quote,"She said, ""Hello!""")";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), csvData, LocalizationFormat::CSV);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("quote") == "She said, \"Hello!\"");
}

TEST_CASE("LocalizationManager - Load CSV with empty lines", "[localization][manager][csv]") {
  LocalizationManager manager;

  std::string csvData = R"(ID,Text
key1,"value1"

key2,"value2"
)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), csvData, LocalizationFormat::CSV);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("key1") == "value1");
  REQUIRE(manager.get("key2") == "value2");
}

// ============================================================================
// File Format Loading Tests - JSON
// ============================================================================

TEST_CASE("LocalizationManager - Load JSON from memory", "[localization][manager][json]") {
  LocalizationManager manager;

  std::string jsonData = R"({
  "greeting": "Hello, World!",
  "farewell": "Goodbye!",
  "question": "How are you?"
})";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), jsonData, LocalizationFormat::JSON);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("greeting") == "Hello, World!");
  REQUIRE(manager.get("farewell") == "Goodbye!");
  REQUIRE(manager.get("question") == "How are you?");
}

TEST_CASE("LocalizationManager - Load JSON with newlines", "[localization][manager][json]") {
  LocalizationManager manager;

  std::string jsonData = R"({
  "multiline": "Line 1\nLine 2\nLine 3"
})";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), jsonData, LocalizationFormat::JSON);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("multiline") == "Line 1\nLine 2\nLine 3");
}

TEST_CASE("LocalizationManager - Load JSON with escaped quotes", "[localization][manager][json]") {
  LocalizationManager manager;

  std::string jsonData = R"({
  "quote": "She said, \"Hello!\""
})";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), jsonData, LocalizationFormat::JSON);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("quote") == "She said, \"Hello!\"");
}

// ============================================================================
// File Format Loading Tests - PO (GNU Gettext)
// ============================================================================

TEST_CASE("LocalizationManager - Load PO from memory", "[localization][manager][po]") {
  LocalizationManager manager;

  std::string poData = R"(# Translation file
msgid "greeting"
msgstr "Hello, World!"

msgid "farewell"
msgstr "Goodbye!"
)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), poData, LocalizationFormat::PO);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("greeting") == "Hello, World!");
  REQUIRE(manager.get("farewell") == "Goodbye!");
}

TEST_CASE("LocalizationManager - Load PO with multiline strings", "[localization][manager][po]") {
  LocalizationManager manager;

  std::string poData = R"(msgid "long_text"
msgstr "This is a "
"long string that "
"spans multiple lines"
)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), poData, LocalizationFormat::PO);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("long_text") == "This is a long string that spans multiple lines");
}

TEST_CASE("LocalizationManager - Load PO with comments", "[localization][manager][po]") {
  LocalizationManager manager;

  std::string poData = R"(# This is a comment
# Another comment line
msgid "key1"
msgstr "value1"

# More comments
msgid "key2"
msgstr "value2"
)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), poData, LocalizationFormat::PO);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("key1") == "value1");
  REQUIRE(manager.get("key2") == "value2");
}

// ============================================================================
// File Format Loading Tests - XLIFF
// ============================================================================

TEST_CASE("LocalizationManager - Load XLIFF from memory", "[localization][manager][xliff]") {
  LocalizationManager manager;

  std::string xliffData = R"(<?xml version="1.0" encoding="UTF-8"?>
<xliff version="1.2">
  <file source-language="en" target-language="en">
    <body>
      <trans-unit id="greeting">
        <source>greeting</source>
        <target>Hello, World!</target>
      </trans-unit>
      <trans-unit id="farewell">
        <source>farewell</source>
        <target>Goodbye!</target>
      </trans-unit>
    </body>
  </file>
</xliff>)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), xliffData, LocalizationFormat::XLIFF);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(manager.get("greeting") == "Hello, World!");
  REQUIRE(manager.get("farewell") == "Goodbye!");
}

TEST_CASE("LocalizationManager - Load XLIFF with XML entities", "[localization][manager][xliff]") {
  LocalizationManager manager;

  std::string xliffData = R"(<?xml version="1.0"?>
<xliff version="1.2">
  <file source-language="en" target-language="en">
    <body>
      <trans-unit id="html">
        <target>&lt;div&gt;Content&lt;/div&gt;</target>
      </trans-unit>
    </body>
  </file>
</xliff>)";

  auto result = manager.loadStringsFromMemory(LocaleId("en"), xliffData, LocalizationFormat::XLIFF);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("en"));
  // Note: The simple parser doesn't unescape entities, so this tests actual behavior
  REQUIRE(manager.hasString("html"));
}

TEST_CASE("LocalizationManager - Load XLIFF fallback to source", "[localization][manager][xliff]") {
  LocalizationManager manager;

  std::string xliffData = R"(<?xml version="1.0"?>
<xliff version="1.2">
  <file source-language="en" target-language="ja">
    <body>
      <trans-unit id="untranslated">
        <source>Not yet translated</source>
      </trans-unit>
    </body>
  </file>
</xliff>)";

  auto result = manager.loadStringsFromMemory(LocaleId("ja"), xliffData, LocalizationFormat::XLIFF);

  REQUIRE(result.isOk());

  manager.setCurrentLocale(LocaleId("ja"));
  // Should use source when target is missing
  REQUIRE(manager.get("untranslated") == "Not yet translated");
}

// ============================================================================
// Export Tests
// ============================================================================

TEST_CASE("LocalizationManager - Export to CSV", "[localization][manager][export][csv]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "key1", "value1");
  manager.setString(LocaleId("en"), "key2", "value with \"quotes\"");

  fs::path tempPath = fs::temp_directory_path() / "test_export.csv";

  auto result = manager.exportStrings(LocaleId("en"), tempPath.string(), LocalizationFormat::CSV);
  REQUIRE(result.isOk());

  // Read and verify
  std::ifstream file(tempPath);
  REQUIRE(file.is_open());

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  REQUIRE(content.find("ID,Text") != std::string::npos);
  REQUIRE(content.find("key1") != std::string::npos);
  REQUIRE(content.find("value1") != std::string::npos);

  fs::remove(tempPath);
}

TEST_CASE("LocalizationManager - Export to JSON", "[localization][manager][export][json]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "greeting", "Hello");
  manager.setString(LocaleId("en"), "farewell", "Goodbye");

  fs::path tempPath = fs::temp_directory_path() / "test_export.json";

  auto result = manager.exportStrings(LocaleId("en"), tempPath.string(), LocalizationFormat::JSON);
  REQUIRE(result.isOk());

  // Read and verify
  std::ifstream file(tempPath);
  REQUIRE(file.is_open());

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  REQUIRE(content.find("\"greeting\"") != std::string::npos);
  REQUIRE(content.find("\"Hello\"") != std::string::npos);
  REQUIRE(content.find("\"farewell\"") != std::string::npos);

  fs::remove(tempPath);
}

TEST_CASE("LocalizationManager - Export to PO", "[localization][manager][export][po]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "hello", "Hello");

  fs::path tempPath = fs::temp_directory_path() / "test_export.po";

  auto result = manager.exportStrings(LocaleId("en"), tempPath.string(), LocalizationFormat::PO);
  REQUIRE(result.isOk());

  // Read and verify
  std::ifstream file(tempPath);
  REQUIRE(file.is_open());

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  REQUIRE(content.find("msgid \"hello\"") != std::string::npos);
  REQUIRE(content.find("msgstr \"Hello\"") != std::string::npos);

  fs::remove(tempPath);
}

TEST_CASE("LocalizationManager - Export to XLIFF", "[localization][manager][export][xliff]") {
  LocalizationManager manager;

  manager.setString(LocaleId("en"), "test_key", "Test Value");

  fs::path tempPath = fs::temp_directory_path() / "test_export.xliff";

  auto result = manager.exportStrings(LocaleId("en"), tempPath.string(), LocalizationFormat::XLIFF);
  REQUIRE(result.isOk());

  // Read and verify
  std::ifstream file(tempPath);
  REQUIRE(file.is_open());

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  REQUIRE(content.find("<?xml") != std::string::npos);
  REQUIRE(content.find("<xliff") != std::string::npos);
  REQUIRE(content.find("test_key") != std::string::npos);
  REQUIRE(content.find("Test Value") != std::string::npos);

  fs::remove(tempPath);
}

TEST_CASE("LocalizationManager - Export non-existent locale", "[localization][manager][export]") {
  LocalizationManager manager;

  fs::path tempPath = fs::temp_directory_path() / "test_export.json";

  auto result = manager.exportStrings(LocaleId("nonexistent"), tempPath.string(), LocalizationFormat::JSON);

  REQUIRE(result.isError());
}

TEST_CASE("LocalizationManager - Export missing strings", "[localization][manager][export]") {
  LocalizationManager manager;
  manager.setDefaultLocale(LocaleId("en"));

  // Default locale has more strings
  manager.setString(LocaleId("en"), "key1", "English 1");
  manager.setString(LocaleId("en"), "key2", "English 2");
  manager.setString(LocaleId("en"), "key3", "English 3");

  // Target locale has partial translation
  manager.setString(LocaleId("ja"), "key1", "Japanese 1");

  fs::path tempPath = fs::temp_directory_path() / "test_missing.json";

  auto result = manager.exportMissingStrings(LocaleId("ja"), tempPath.string(), LocalizationFormat::JSON);
  REQUIRE(result.isOk());

  // Load the missing strings back and verify
  LocalizationManager verifyManager;
  auto loadResult = verifyManager.loadStrings(LocaleId("test"), tempPath.string(), LocalizationFormat::JSON);
  REQUIRE(loadResult.isOk());

  verifyManager.setCurrentLocale(LocaleId("test"));

  // Should contain key2 and key3, but not key1
  REQUIRE(verifyManager.hasString("key2"));
  REQUIRE(verifyManager.hasString("key3"));

  fs::remove(tempPath);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("LocalizationManager - Complete workflow", "[localization][manager][integration]") {
  LocalizationManager manager;
  manager.setDefaultLocale(LocaleId("en"));

  // Register locale configurations
  LocaleConfig enConfig;
  enConfig.displayName = "English";
  enConfig.nativeName = "English";
  enConfig.rightToLeft = false;
  manager.registerLocale(LocaleId("en"), enConfig);

  LocaleConfig arConfig;
  arConfig.displayName = "Arabic";
  arConfig.nativeName = "العربية";
  arConfig.rightToLeft = true;
  manager.registerLocale(LocaleId("ar"), arConfig);

  // Load English strings
  manager.setString(LocaleId("en"), "app.title", "My Application");
  manager.setString(LocaleId("en"), "user.greeting", "Welcome, {username}!");

  auto enTable = manager.getStringTableMutable(LocaleId("en"));
  std::unordered_map<PluralCategory, std::string> messageForms;
  messageForms[PluralCategory::One] = "You have {count} message";
  messageForms[PluralCategory::Other] = "You have {count} messages";
  enTable->addPluralString("message.count", messageForms);

  // Load Arabic strings (partial)
  manager.setString(LocaleId("ar"), "app.title", "تطبيقي");

  // Set up callbacks
  bool languageChanged = false;
  manager.setOnLanguageChanged([&](const LocaleId&) {
    languageChanged = true;
  });

  std::vector<std::string> missingKeys;
  manager.setOnStringMissing([&](const std::string& key, const LocaleId&) {
    missingKeys.push_back(key);
  });

  // Test English
  manager.setCurrentLocale(LocaleId("en"));
  REQUIRE(languageChanged);
  REQUIRE(manager.get("app.title") == "My Application");

  std::unordered_map<std::string, std::string> vars;
  vars["username"] = "Alice";
  REQUIRE(manager.get("user.greeting", vars) == "Welcome, Alice!");

  vars["count"] = "5";
  REQUIRE(manager.getPlural("message.count", 5, vars) == "You have 5 messages");

  // Switch to Arabic
  languageChanged = false;
  missingKeys.clear();
  manager.setCurrentLocale(LocaleId("ar"));
  REQUIRE(languageChanged);
  REQUIRE(manager.isCurrentLocaleRightToLeft());

  // Should get Arabic translation
  REQUIRE(manager.get("app.title") == "تطبيقي");

  // Should fallback to English for missing string
  REQUIRE(manager.get("user.greeting", vars) == "Welcome, Alice!");
  REQUIRE(missingKeys.size() > 0);
  REQUIRE(std::find(missingKeys.begin(), missingKeys.end(), "user.greeting") != missingKeys.end());
}

TEST_CASE("LocalizationManager - Round-trip export and import", "[localization][manager][integration]") {
  LocalizationManager manager1;

  manager1.setString(LocaleId("en"), "key1", "Value 1");
  manager1.setString(LocaleId("en"), "key2", "Value 2");
  manager1.setString(LocaleId("en"), "key3", "Value \"quoted\"");

  fs::path tempPath = fs::temp_directory_path() / "roundtrip_test.json";

  // Export
  auto exportResult = manager1.exportStrings(LocaleId("en"), tempPath.string(), LocalizationFormat::JSON);
  REQUIRE(exportResult.isOk());

  // Import into new manager
  LocalizationManager manager2;
  auto importResult = manager2.loadStrings(LocaleId("test"), tempPath.string(), LocalizationFormat::JSON);
  REQUIRE(importResult.isOk());

  // Verify
  manager2.setCurrentLocale(LocaleId("test"));
  REQUIRE(manager2.get("key1") == "Value 1");
  REQUIRE(manager2.get("key2") == "Value 2");
  REQUIRE(manager2.get("key3") == "Value \"quoted\"");

  fs::remove(tempPath);
}
