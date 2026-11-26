#pragma once

#include <mishmesh/sound/Sounds.h>
#include <string.h>

namespace mishmesh {

struct AlertSound {
  sound::SoundId tone = sound::SoundId::COUNT;
  int volume = -1;

  void play(sound::SoundEngine& engine, sound::SoundId fallback) const {
    if (engine.volume() == sound::VolumeLevel::Mute || volume == 0) return;
    const auto id = tone == sound::SoundId::COUNT ? fallback : tone;
    if (volume < 0) engine.play(id);
    else engine.play(id, static_cast<sound::VolumeLevel>(volume));
  }
};

inline bool alertSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Optional pipe-delimited tone and volume, in either order.
inline bool parseAlertSound(const char* tail, AlertSound& out) {
  AlertSound parsed;
  while (alertSpace(*tail)) ++tail;
  while (*tail) {
    if (*tail++ != '|') return false;
    while (alertSpace(*tail)) ++tail;
    const char* end = tail;
    while (*end && *end != '|') ++end;
    const char* trimmed = end;
    while (trimmed > tail && alertSpace(trimmed[-1])) --trimmed;
    const auto matches = [&](const char* token) {
      const size_t length = trimmed - tail;
      return strlen(token) == length && memcmp(tail, token, length) == 0;
    };
    static const char* const levels[] = {"mute", "low", "mid", "high"};
    using sound::SoundId;
    static const struct { const char* name; SoundId id; } tones[] = {
      {"ring", SoundId::AlarmRing}, {"chime", SoundId::MsgChime},
      {"ding", SoundId::MsgDing}, {"arpeggio", SoundId::TimerDone},
      {"nokia", SoundId::NokiaTune}, {"bigben", SoundId::BigBen},
    };
    bool found = false;
    if (parsed.volume < 0) {
      for (int i = 0; i < 4; ++i) {
        if (matches(levels[i])) { parsed.volume = i; found = true; break; }
      }
    }
    if (!found && parsed.tone == SoundId::COUNT) {
      for (const auto& tone : tones) {
        if (matches(tone.name)) { parsed.tone = tone.id; found = true; break; }
      }
    }
    if (!found) return false;
    tail = end;
  }
  out = parsed;
  return true;
}

}  // namespace mishmesh
