#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

namespace crosscap {

inline bool DebugLogEnabled() {
  const char* value = std::getenv("CROSSCAP_DEBUG_NATIVE");
  return value != nullptr && std::string(value) == "1";
}

inline void DebugLog(const char* scope, const char* message) {
  if (!DebugLogEnabled()) {
    return;
  }

  std::fprintf(stderr, "[crosscap][%s] %s\n", scope, message);
  std::fflush(stderr);
}

}  // namespace crosscap
