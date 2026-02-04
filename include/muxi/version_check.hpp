#pragma once

#include <string>
#include <unordered_map>

namespace muxi {
namespace internal {

void check_for_updates(const std::unordered_map<std::string, std::string>& headers);

} // namespace internal
} // namespace muxi
