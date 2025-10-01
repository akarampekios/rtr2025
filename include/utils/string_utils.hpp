#pragma once

#include <string>
#include <vector>

namespace StringUtils {
inline auto toCStrings(const std::vector<std::string>& strings)
    -> std::vector<const char*> {
  std::vector<const char*> cStrings;
  cStrings.reserve(strings.size());

  for (const auto& str : strings) {
    cStrings.push_back(str.c_str());
  }

  return cStrings;
}
}  // namespace StringUtils
