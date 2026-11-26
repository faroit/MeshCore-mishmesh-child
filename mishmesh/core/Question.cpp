#include "Question.h"
#include <string.h>

namespace mishmesh {
namespace {
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
}
bool parseQuestion(const char* text, Question& out) {
  out = Question{};
  if (!text) return false;
  size_t length = 0;
  while (length <= 160 && text[length]) ++length;
  if (length > 160) return false;
  while (space(*text)) ++text;
  if (*text++ != '?') return false;
  size_t used = 0;
  bool prompt = true;
  for (;;) {
    const char* end = text;
    while (*end && *end != '|') ++end;
    const char* start = text;
    const char* trimmed = end;
    while (start < trimmed && space(*start)) ++start;
    while (trimmed > start && space(trimmed[-1])) --trimmed;
    size_t len = trimmed - start;
    if (len) {
      if (!prompt && out.count == Question::MAX_OPTIONS) { out = Question{}; return false; }
      if (used + len + 1 > sizeof(out.text)) { out = Question{}; return false; }
      if (!prompt) out.options[out.count++] = (uint8_t)used;
      memcpy(out.text + used, start, len);
      used += len; out.text[used++] = 0;
    } else if (prompt) return false;
    prompt = false;
    if (!*end) break;
    text = end + 1;
  }
  if (out.count < 2) { out = Question{}; return false; }
  return true;
}
uint32_t questionFingerprint(const char* text) {
  uint32_t h = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); p && *p; ++p)
    h = (h ^ *p) * 16777619u;
  return h;
}
}
