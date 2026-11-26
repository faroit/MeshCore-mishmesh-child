#pragma once

#include <mishmesh/core/ClockService.h>
#include <string.h>

namespace mishmesh {

// A single positive duration in seconds, minutes or hours.
inline bool parseTimerRequest(const char* text, char (&prompt)[161], uint32_t& seconds) {
  const auto space = [](char c) { return c == ' ' || c == '\t'; };
  if (!text || *text != '%' || strlen(text) > 160) return false;
  const char* start = text + 1;
  const char* separator = strchr(start, '|');
  if (!separator) return false;
  const char* end = separator;
  while (start < end && space(*start)) ++start;
  while (end > start && space(end[-1])) --end;
  if (start == end) return false;

  const char* duration = separator + 1;
  while (space(*duration)) ++duration;
  uint32_t value = 0;
  while (*duration >= '0' && *duration <= '9') {
    value = value * 10 + (*duration++ - '0');
    if (value > ClockService::TIMER_MAX_SECS) return false;
  }
  if (!value) return false;
  uint32_t unit;
  switch (*duration++) {
    case 's': unit = 1; break;
    case 'm': unit = 60; break;
    case 'h': unit = 3600; break;
    default: return false;
  }
  while (space(*duration)) ++duration;
  if (*duration || value > ClockService::TIMER_MAX_SECS / unit) return false;
  seconds = value * unit;
  memcpy(prompt, start, end - start);
  prompt[end - start] = 0;
  return true;
}

}  // namespace mishmesh
