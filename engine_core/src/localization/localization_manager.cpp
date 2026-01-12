/**
 * @file localization_manager.cpp
 * @brief Localization Manager implementation
 */

#include "NovelMind/localization/localization_manager.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace NovelMind::localization {

namespace fs = std::filesystem;
namespace {

bool readFileToString(std::ifstream& file, std::string& out) {
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  file.read(out.data(), static_cast<std::streamsize>(out.size()));
  return static_cast<bool>(file);
}

} // namespace

// =========================================================================
// StringTable Implementation
// =========================================================================

StringTable::StringTable(const LocaleId& locale) : m_locale(locale) {}

void StringTable::addString(const std::string& id, const std::string& value) {
  LocalizedString str;
  str.id = id;
  str.forms[PluralCategory::Other] = value;
  m_strings[id] = std::move(str);
}

void StringTable::addPluralString(const std::string& id,
                                  const std::unordered_map<PluralCategory, std::string>& forms) {
  LocalizedString str;
  str.id = id;
  str.forms = forms;
  m_strings[id] = std::move(str);
}

std::optional<std::string> StringTable::getString(const std::string& id) const {
  auto it = m_strings.find(id);
  if (it != m_strings.end()) {
    auto formIt = it->second.forms.find(PluralCategory::Other);
    if (formIt != it->second.forms.end()) {
      return formIt->second;
    }
    // Return first available form
    if (!it->second.forms.empty()) {
      return it->second.forms.begin()->second;
    }
  }
  return std::nullopt;
}

std::optional<std::string> StringTable::getPluralString(const std::string& id, i64 count) const {
  auto it = m_strings.find(id);
  if (it == m_strings.end())
    return std::nullopt;

  // Determine plural category (English rules as default)
  PluralCategory category;
  if (count == 0)
    category = PluralCategory::Zero;
  else if (count == 1)
    category = PluralCategory::One;
  else
    category = PluralCategory::Other;

  // Try exact category first
  auto formIt = it->second.forms.find(category);
  if (formIt != it->second.forms.end()) {
    return formIt->second;
  }

  // Fall back to Other
  formIt = it->second.forms.find(PluralCategory::Other);
  if (formIt != it->second.forms.end()) {
    return formIt->second;
  }

  return std::nullopt;
}

bool StringTable::hasString(const std::string& id) const {
  return m_strings.find(id) != m_strings.end();
}

std::vector<std::string> StringTable::getStringIds() const {
  std::vector<std::string> ids;
  ids.reserve(m_strings.size());
  for (const auto& [id, str] : m_strings) {
    ids.push_back(id);
  }
  return ids;
}

void StringTable::clear() {
  m_strings.clear();
}

void StringTable::removeString(const std::string& id) {
  m_strings.erase(id);
}

// =========================================================================
// LocalizationManager Implementation
// =========================================================================

LocalizationManager::LocalizationManager() {
  // Set default English locale
  m_defaultLocale.language = "en";
  m_currentLocale = m_defaultLocale;
}

LocalizationManager::~LocalizationManager() = default;

// =========================================================================
// Locale Management
// =========================================================================

void LocalizationManager::setDefaultLocale(const LocaleId& locale) {
  m_defaultLocale = locale;
}

void LocalizationManager::setCurrentLocale(const LocaleId& locale) {
  if (m_currentLocale.toString() != locale.toString()) {
    m_currentLocale = locale;
    if (m_onLanguageChanged) {
      m_onLanguageChanged(locale);
    }
  }
}

std::vector<LocaleId> LocalizationManager::getAvailableLocales() const {
  std::vector<LocaleId> locales;
  locales.reserve(m_stringTables.size());
  for (const auto& [locale, table] : m_stringTables) {
    locales.push_back(locale);
  }
  return locales;
}

bool LocalizationManager::isLocaleAvailable(const LocaleId& locale) const {
  return m_stringTables.find(locale) != m_stringTables.end();
}

void LocalizationManager::registerLocale(const LocaleId& locale, const LocaleConfig& config) {
  m_localeConfigs[locale] = config;
}

std::optional<LocaleConfig> LocalizationManager::getLocaleConfig(const LocaleId& locale) const {
  auto it = m_localeConfigs.find(locale);
  if (it != m_localeConfigs.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool LocalizationManager::isRightToLeft(const LocaleId& locale) const {
  auto config = getLocaleConfig(locale);
  if (config.has_value()) {
    return config->rightToLeft;
  }

  static const std::vector<std::string> kRtlLanguages = {"ar", "he", "fa", "ur",
                                                         "dv", "ps", "ku", "yi"};

  return std::find(kRtlLanguages.begin(), kRtlLanguages.end(), locale.language) !=
         kRtlLanguages.end();
}

bool LocalizationManager::isCurrentLocaleRightToLeft() const {
  return isRightToLeft(m_currentLocale);
}

// =========================================================================
// String Loading
// =========================================================================

Result<void> LocalizationManager::loadStrings(const LocaleId& locale, const std::string& filePath,
                                              LocalizationFormat format) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open localization file: " + filePath);
  }

  std::string content;
  if (!readFileToString(file, content)) {
    return Result<void>::error("Failed to read localization file: " + filePath);
  }

  return loadStringsFromMemory(locale, content, format);
}

Result<void> LocalizationManager::loadStringsFromMemory(const LocaleId& locale,
                                                        const std::string& data,
                                                        LocalizationFormat format) {
  switch (format) {
  case LocalizationFormat::CSV:
    return loadCSV(locale, data);
  case LocalizationFormat::JSON:
    return loadJSON(locale, data);
  case LocalizationFormat::PO:
    return loadPO(locale, data);
  case LocalizationFormat::XLIFF:
    return loadXLIFF(locale, data);
  default:
    return Result<void>::error("Unknown localization format");
  }
}

Result<void> LocalizationManager::mergeStrings(const LocaleId& locale, const std::string& filePath,
                                               LocalizationFormat format) {
  // Load and merge into existing table
  return loadStrings(locale, filePath, format);
}

void LocalizationManager::unloadLocale(const LocaleId& locale) {
  m_stringTables.erase(locale);
}

void LocalizationManager::clearAll() {
  m_stringTables.clear();
}

// =========================================================================
// String Retrieval
// =========================================================================

std::string LocalizationManager::get(const std::string& id) const {
  // Try current locale first
  auto it = m_stringTables.find(m_currentLocale);
  if (it != m_stringTables.end()) {
    auto str = it->second.getString(id);
    if (str) {
      return *str;
    }
  }

  // Fall back to default locale
  if (m_currentLocale.toString() != m_defaultLocale.toString()) {
    it = m_stringTables.find(m_defaultLocale);
    if (it != m_stringTables.end()) {
      auto str = it->second.getString(id);
      if (str) {
        fireMissingString(id, m_currentLocale);
        return *str;
      }
    }
  }

  // Return the ID as fallback
  fireMissingString(id, m_currentLocale);
  return id;
}

std::string
LocalizationManager::get(const std::string& id,
                         const std::unordered_map<std::string, std::string>& variables) const {
  std::string text = get(id);
  return interpolate(text, variables);
}

std::string LocalizationManager::getPlural(const std::string& id, i64 count) const {
  // Try current locale first
  auto it = m_stringTables.find(m_currentLocale);
  if (it != m_stringTables.end()) {
    auto str = it->second.getPluralString(id, count);
    if (str) {
      return *str;
    }
  }

  // Fall back to default locale
  if (m_currentLocale.toString() != m_defaultLocale.toString()) {
    it = m_stringTables.find(m_defaultLocale);
    if (it != m_stringTables.end()) {
      auto str = it->second.getPluralString(id, count);
      if (str) {
        fireMissingString(id, m_currentLocale);
        return *str;
      }
    }
  }

  fireMissingString(id, m_currentLocale);
  return id;
}

std::string LocalizationManager::getPlural(
    const std::string& id, i64 count,
    const std::unordered_map<std::string, std::string>& variables) const {
  std::string text = getPlural(id, count);
  return interpolate(text, variables);
}

std::string LocalizationManager::getForLocale(const LocaleId& locale, const std::string& id) const {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    auto str = it->second.getString(id);
    if (str) {
      return *str;
    }
  }
  return id;
}

bool LocalizationManager::hasString(const std::string& id) const {
  return hasString(m_currentLocale, id);
}

bool LocalizationManager::hasString(const LocaleId& locale, const std::string& id) const {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    return it->second.hasString(id);
  }
  return false;
}

// =========================================================================
// String Editing
// =========================================================================

void LocalizationManager::setString(const LocaleId& locale, const std::string& id,
                                    const std::string& value) {
  getOrCreateTable(locale).addString(id, value);
}

void LocalizationManager::removeString(const LocaleId& locale, const std::string& id) {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    it->second.removeString(id);
  }
}

const StringTable* LocalizationManager::getStringTable(const LocaleId& locale) const {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    return &it->second;
  }
  return nullptr;
}

StringTable* LocalizationManager::getStringTableMutable(const LocaleId& locale) {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    return &it->second;
  }
  return nullptr;
}

// =========================================================================
// Export
// =========================================================================

Result<void> LocalizationManager::exportStrings(const LocaleId& locale, const std::string& filePath,
                                                LocalizationFormat format) const {
  auto it = m_stringTables.find(locale);
  if (it == m_stringTables.end()) {
    return Result<void>::error("Locale not found: " + locale.toString());
  }

  switch (format) {
  case LocalizationFormat::CSV:
    return exportCSV(it->second, filePath);
  case LocalizationFormat::JSON:
    return exportJSON(it->second, filePath);
  case LocalizationFormat::PO:
    return exportPO(it->second, filePath);
  case LocalizationFormat::XLIFF:
    return exportXLIFF(it->second, filePath);
  default:
    return Result<void>::error("Unsupported export format");
  }
}

Result<void> LocalizationManager::exportMissingStrings(const LocaleId& locale,
                                                       const std::string& filePath,
                                                       LocalizationFormat format) const {
  auto defaultIt = m_stringTables.find(m_defaultLocale);
  if (defaultIt == m_stringTables.end()) {
    return Result<void>::error("Default locale not loaded");
  }

  StringTable missingTable(locale);
  auto targetIt = m_stringTables.find(locale);

  for (const auto& id : defaultIt->second.getStringIds()) {
    bool hasinTarget = targetIt != m_stringTables.end() && targetIt->second.hasString(id);
    if (!hasinTarget) {
      auto str = defaultIt->second.getString(id);
      if (str) {
        missingTable.addString(id, *str);
      }
    }
  }

  switch (format) {
  case LocalizationFormat::CSV:
    return exportCSV(missingTable, filePath);
  case LocalizationFormat::JSON:
    return exportJSON(missingTable, filePath);
  case LocalizationFormat::PO:
    return exportPO(missingTable, filePath);
  case LocalizationFormat::XLIFF:
    return exportXLIFF(missingTable, filePath);
  default:
    return Result<void>::error("Unsupported export format");
  }
}

// =========================================================================
// Callbacks
// =========================================================================

void LocalizationManager::setOnLanguageChanged(OnLanguageChanged callback) {
  m_onLanguageChanged = std::move(callback);
}

void LocalizationManager::setOnStringMissing(OnStringMissing callback) {
  m_onStringMissing = std::move(callback);
}

// =========================================================================
// Utility
// =========================================================================

PluralCategory LocalizationManager::getPluralCategory(i64 count) const {
  return getPluralCategory(m_currentLocale, count);
}

PluralCategory LocalizationManager::getPluralCategory(const LocaleId& locale, i64 count) const {
  // Simplified plural rules (would need CLDR data for full support)
  // English-style rules as default
  const std::string& lang = locale.language;

  if (lang == "ru" || lang == "uk" || lang == "be") {
    // East Slavic rules
    i64 mod10 = count % 10;
    i64 mod100 = count % 100;

    if (mod10 == 1 && mod100 != 11)
      return PluralCategory::One;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
      return PluralCategory::Few;
    return PluralCategory::Many;
  } else if (lang == "ja" || lang == "zh" || lang == "ko") {
    // East Asian (no plural)
    return PluralCategory::Other;
  } else if (lang == "ar") {
    // Arabic
    if (count == 0)
      return PluralCategory::Zero;
    if (count == 1)
      return PluralCategory::One;
    if (count == 2)
      return PluralCategory::Two;
    i64 mod100 = count % 100;
    if (mod100 >= 3 && mod100 <= 10)
      return PluralCategory::Few;
    if (mod100 >= 11)
      return PluralCategory::Many;
    return PluralCategory::Other;
  } else {
    // English and most Western European
    if (count == 1)
      return PluralCategory::One;
    return PluralCategory::Other;
  }
}

std::string LocalizationManager::interpolate(
    const std::string& text, const std::unordered_map<std::string, std::string>& variables) const {
  std::string result = text;

  // Replace {variable} patterns
  for (const auto& [name, value] : variables) {
    std::string pattern = "{" + name + "}";
    size_t pos = 0;
    while ((pos = result.find(pattern, pos)) != std::string::npos) {
      result.replace(pos, pattern.length(), value);
      pos += value.length();
    }
  }

  return result;
}

// =========================================================================
// Private Helpers
// =========================================================================

StringTable& LocalizationManager::getOrCreateTable(const LocaleId& locale) {
  auto it = m_stringTables.find(locale);
  if (it != m_stringTables.end()) {
    return it->second;
  }

  m_stringTables[locale] = StringTable(locale);
  return m_stringTables[locale];
}

void LocalizationManager::fireMissingString(const std::string& id, const LocaleId& locale) const {
  if (m_onStringMissing) {
    m_onStringMissing(id, locale);
  }
}

Result<void> LocalizationManager::loadCSV(const LocaleId& locale, const std::string& content) {
  StringTable& table = getOrCreateTable(locale);

  std::istringstream stream(content);
  std::string line;
  bool headerSkipped = false;

  while (std::getline(stream, line)) {
    // Skip header
    if (!headerSkipped) {
      headerSkipped = true;
      continue;
    }

    // Skip empty lines
    if (line.empty())
      continue;

    // Simple CSV parsing (ID,Text format)
    size_t commaPos = line.find(',');
    if (commaPos != std::string::npos) {
      std::string id = line.substr(0, commaPos);
      std::string value = line.substr(commaPos + 1);

      // Remove quotes if present
      if (!id.empty() && id.front() == '"' && id.back() == '"') {
        id = id.substr(1, id.size() - 2);
      }
      if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
      }

      // Unescape double quotes
      size_t pos = 0;
      while ((pos = value.find("\"\"", pos)) != std::string::npos) {
        value.replace(pos, 2, "\"");
        pos += 1;
      }

      table.addString(id, value);
    }
  }

  return {};
}

Result<void> LocalizationManager::loadJSON(const LocaleId& locale, const std::string& content) {
  StringTable& table = getOrCreateTable(locale);

  // Simple JSON parsing for {"key": "value"} format
  // Pattern: "key" : "value"
  std::string pattern = "\"([^\"]+)\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"";
  std::regex keyValueRegex(pattern);
  auto begin = std::sregex_iterator(content.begin(), content.end(), keyValueRegex);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it) {
    std::string id = (*it)[1].str();
    std::string value = (*it)[2].str();

    // Unescape JSON strings
    size_t pos = 0;
    while ((pos = value.find("\\n", pos)) != std::string::npos) {
      value.replace(pos, 2, "\n");
      pos += 1;
    }
    pos = 0;
    while ((pos = value.find("\\\"", pos)) != std::string::npos) {
      value.replace(pos, 2, "\"");
      pos += 1;
    }

    table.addString(id, value);
  }

  return {};
}

Result<void> LocalizationManager::loadPO(const LocaleId& locale, const std::string& content) {
  StringTable& table = getOrCreateTable(locale);

  std::istringstream stream(content);
  std::string line;
  std::string currentMsgid;
  std::string currentMsgstr;
  bool inMsgstr = false;

  auto commitEntry = [&]() {
    if (!currentMsgid.empty() && !currentMsgstr.empty()) {
      table.addString(currentMsgid, currentMsgstr);
    }
    currentMsgid.clear();
    currentMsgstr.clear();
    inMsgstr = false;
  };

  while (std::getline(stream, line)) {
    // Remove leading/trailing whitespace
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    line = line.substr(start);

    // Skip comments
    if (line[0] == '#')
      continue;

    if (line.substr(0, 5) == "msgid") {
      commitEntry();
      // Extract string
      size_t quoteStart = line.find('"');
      size_t quoteEnd = line.rfind('"');
      if (quoteStart != std::string::npos && quoteEnd > quoteStart) {
        currentMsgid = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
      }
    } else if (line.substr(0, 6) == "msgstr") {
      inMsgstr = true;
      size_t quoteStart = line.find('"');
      size_t quoteEnd = line.rfind('"');
      if (quoteStart != std::string::npos && quoteEnd > quoteStart) {
        currentMsgstr = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
      }
    } else if (line[0] == '"' && !currentMsgid.empty()) {
      // Continuation line
      size_t quoteEnd = line.rfind('"');
      if (quoteEnd > 0) {
        std::string cont = line.substr(1, quoteEnd - 1);
        if (inMsgstr) {
          currentMsgstr += cont;
        } else {
          currentMsgid += cont;
        }
      }
    }
  }

  commitEntry();
  return {};
}

Result<void> LocalizationManager::loadXLIFF(const LocaleId& locale, const std::string& content) {
  StringTable& table = getOrCreateTable(locale);

  // Simple XLIFF parsing for trans-unit elements
  std::string transUnitPattern = "<trans-unit[^>]*id=\"([^\"]*)\"[^>]*>";
  std::string sourcePattern = "<source>([^<]*)</source>";
  std::string targetPattern = "<target>([^<]*)</target>";

  std::regex transUnitRegex(transUnitPattern);
  std::regex sourceRegex(sourcePattern);
  std::regex targetRegex(targetPattern);

  auto unitBegin = std::sregex_iterator(content.begin(), content.end(), transUnitRegex);
  auto unitEnd = std::sregex_iterator();

  for (auto it = unitBegin; it != unitEnd; ++it) {
    std::string id = (*it)[1].str();
    size_t searchPos = static_cast<size_t>(it->position()) + static_cast<size_t>(it->length());

    // Find the closing </trans-unit> to scope the search
    size_t endPos = content.find("</trans-unit>", searchPos);
    if (endPos == std::string::npos)
      break;

    std::string unitContent = content.substr(searchPos, endPos - searchPos);

    std::smatch targetMatch;
    if (std::regex_search(unitContent, targetMatch, targetRegex)) {
      std::string value = targetMatch[1].str();
      table.addString(id, value);
    } else {
      // Fall back to source if no target
      std::smatch sourceMatch;
      if (std::regex_search(unitContent, sourceMatch, sourceRegex)) {
        std::string value = sourceMatch[1].str();
        table.addString(id, value);
      }
    }
  }

  return {};
}

Result<void> LocalizationManager::exportCSV(const StringTable& table,
                                            const std::string& path) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  file << "ID,Text\n";

  for (const auto& id : table.getStringIds()) {
    auto str = table.getString(id);
    if (str) {
      // Escape quotes and wrap in quotes
      std::string escaped = *str;
      size_t pos = 0;
      while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\"\"");
        pos += 2;
      }
      file << "\"" << id << "\",\"" << escaped << "\"\n";
    }
  }

  return {};
}

Result<void> LocalizationManager::exportJSON(const StringTable& table,
                                             const std::string& path) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  file << "{\n";

  auto ids = table.getStringIds();
  for (size_t i = 0; i < ids.size(); ++i) {
    const auto& id = ids[i];
    auto str = table.getString(id);
    if (str) {
      // Escape JSON special characters
      std::string escaped = *str;
      size_t pos = 0;
      while ((pos = escaped.find('\\', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\\");
        pos += 2;
      }
      pos = 0;
      while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
      }
      pos = 0;
      while ((pos = escaped.find('\n', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\n");
        pos += 2;
      }

      file << "  \"" << id << "\": \"" << escaped << "\"";
      if (i < ids.size() - 1)
        file << ",";
      file << "\n";
    }
  }

  file << "}\n";

  return {};
}

Result<void> LocalizationManager::exportPO(const StringTable& table,
                                           const std::string& path) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  // PO file header
  file << "# NovelMind Localization File\n";
  file << "# Language: " << table.getLocale().toString() << "\n";
  file << "\n";

  for (const auto& id : table.getStringIds()) {
    auto str = table.getString(id);
    if (str) {
      file << "msgid \"" << id << "\"\n";
      file << "msgstr \"" << *str << "\"\n";
      file << "\n";
    }
  }

  return {};
}

Result<void> LocalizationManager::exportXLIFF(const StringTable& table,
                                              const std::string& path) const {
  std::ofstream file(path);
  if (!file.is_open()) {
    return Result<void>::error("Failed to open file for writing: " + path);
  }

  const std::string locale = table.getLocale().toString();

  // XLIFF 1.2 format
  file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  file << "<xliff version=\"1.2\" "
          "xmlns=\"urn:oasis:names:tc:xliff:document:1.2\">\n";
  file << "  <file source-language=\"en\" target-language=\"" << locale
       << "\" datatype=\"plaintext\" original=\"NovelMind\">\n";
  file << "    <body>\n";

  for (const auto& id : table.getStringIds()) {
    auto str = table.getString(id);
    if (str) {
      // XML escape special characters
      std::string escapedId = id;
      std::string escapedStr = *str;

      auto escapeXml = [](std::string& text) {
        size_t pos = 0;
        while ((pos = text.find('&', pos)) != std::string::npos) {
          text.replace(pos, 1, "&amp;");
          pos += 5;
        }
        pos = 0;
        while ((pos = text.find('<', pos)) != std::string::npos) {
          text.replace(pos, 1, "&lt;");
          pos += 4;
        }
        pos = 0;
        while ((pos = text.find('>', pos)) != std::string::npos) {
          text.replace(pos, 1, "&gt;");
          pos += 4;
        }
        pos = 0;
        while ((pos = text.find('"', pos)) != std::string::npos) {
          text.replace(pos, 1, "&quot;");
          pos += 6;
        }
      };

      escapeXml(escapedId);
      escapeXml(escapedStr);

      file << "      <trans-unit id=\"" << escapedId << "\">\n";
      file << "        <source>" << escapedId << "</source>\n";
      file << "        <target>" << escapedStr << "</target>\n";
      file << "      </trans-unit>\n";
    }
  }

  file << "    </body>\n";
  file << "  </file>\n";
  file << "</xliff>\n";

  return {};
}

} // namespace NovelMind::localization
