#pragma once
#include <string.h>

namespace mishmesh {
inline bool isCallBell(const char* text) {
  if (!text) return false;
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
  if (strncmp(text, "\xF0\x9F\x94\x94", 4)) return false;
  text += 4;
  if (!strncmp(text, "\xEF\xB8\x8F", 3)) text += 3; // optional emoji presentation
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
  return !*text;
}
}
