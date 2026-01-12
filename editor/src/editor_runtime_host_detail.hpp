#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace NovelMind::editor::detail {

bool readFileToString(std::ifstream& file, std::string& out);
std::string encodeList(const std::vector<std::string>& items);
std::vector<std::string> decodeList(const std::string& value);
bool startsWith(const std::string& value, const std::string& prefix);

} // namespace NovelMind::editor::detail
