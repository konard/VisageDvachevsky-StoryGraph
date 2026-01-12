#include "editor_runtime_host_detail.hpp"

namespace NovelMind::editor::detail {

namespace {

std::string escapeListValue(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out += c;
    }
  }
  return out;
}

std::string unescapeListValue(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    char c = value[i];
    if (c == '\\' && i + 1 < value.size()) {
      char next = value[i + 1];
      if (next == 'n') {
        out.push_back('\n');
        ++i;
        continue;
      }
      if (next == '\\') {
        out.push_back('\\');
        ++i;
        continue;
      }
    }
    out.push_back(c);
  }
  return out;
}

} // namespace

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

std::string encodeList(const std::vector<std::string>& items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      out.push_back('\n');
    }
    out += escapeListValue(items[i]);
  }
  return out;
}

std::vector<std::string> decodeList(const std::string& value) {
  std::vector<std::string> out;
  std::string current;
  for (char c : value) {
    if (c == '\n') {
      out.push_back(unescapeListValue(current));
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty() || (!value.empty() && value.back() == '\n')) {
    out.push_back(unescapeListValue(current));
  }
  return out;
}

bool startsWith(const std::string& value, const std::string& prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  return value.compare(0, prefix.size(), prefix) == 0;
}

} // namespace NovelMind::editor::detail
