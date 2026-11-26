#pragma once
#include <stdint.h>

namespace mishmesh {
// Same wire format as MeshCore-child-companion: ?question | answer | answer.
// Keep complete UTF-8 segments in one bounded buffer; never truncate an answer.
struct Question {
  static const int MAX_OPTIONS = 6;
  char text[161] = {};
  uint8_t options[MAX_OPTIONS] = {};
  uint8_t count = 0;
  const char* prompt() const { return text; }
  const char* answer(int i) const { return i >= 0 && i < count ? text + options[i] : ""; }
};
bool parseQuestion(const char* text, Question& out);
uint32_t questionFingerprint(const char* text);
}
