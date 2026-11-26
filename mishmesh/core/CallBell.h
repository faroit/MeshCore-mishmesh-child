#pragma once
#include <mishmesh/core/AlertSound.h>

namespace mishmesh {
inline bool parseCallBell(const char* text, AlertSound& alert) {
  if (!text) return false;
  while (alertSpace(*text)) ++text;
  if (strncmp(text, "\xF0\x9F\x94\x94", 4)) return false;
  text += 4;
  if (!strncmp(text, "\xEF\xB8\x8F", 3)) text += 3; // optional emoji presentation
  return parseAlertSound(text, alert);
}

inline bool isCallBell(const char* text) {
  AlertSound alert;
  return parseCallBell(text, alert);
}
}
