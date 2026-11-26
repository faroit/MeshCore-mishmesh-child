#include "CallBellApplet.h"
#include <mishmesh/core/CallBell.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>


namespace mishmesh {
bool CallBellApplet::enqueue(const ConvoKey& key, uint32_t time, const char* name) {
  if (key.type != 0) return false;
  for (int i = 0; i < _count; ++i)
    if (_calls[i].key.equals(key) && _calls[i].time == time) return true;
  if (_count == 4) return false;
  Call& c = _calls[_count++]; c.key = key; c.time = time;
  copyStr(c.name, sizeof(c.name), name && *name ? name : "Parent");
  return true;
}
void CallBellApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _svc = ctx.messages; _sound = ctx.sound; _open = true;
  _started = _beeped = false; _ringing = true; _title.reset();
}
void CallBellApplet::onBackground() { if (_sound) _sound->stop(); }
void CallBellApplet::onStop() { onBackground(); _open = false; }
bool CallBellApplet::available(AlertSound* alert) const {
  if (!_count || !_svc) return false;
  const Call& call = _calls[0];
  for (int i = _svc->messageCount(call.key) - 1; i >= 0; --i) {
    MessageView m;
    if (_svc->getMessage(call.key, i, m) && m.kind == KIND_INBOUND &&
        m.senderTime == call.time) {
      AlertSound parsed;
      if (!parseCallBell(m.text, parsed)) continue;
      if (alert) *alert = parsed;
      return true;
    }
  }
  return false; // deleted or no longer permitted
}
void CallBellApplet::advance() {
  if (_sound) _sound->stop();
  if (_count) { --_count; for (int i = 0; i < _count; ++i) _calls[i] = _calls[i + 1]; }
  _started = _beeped = false; _ringing = _count != 0; _title.reset();
  if (!_count && _host && _host->foreground() == this) _host->pop();
  else if (_host) _host->requestRender();
}
int CallBellApplet::onRender(Canvas& c) {
  AlertSound alert;
  if (!available(&alert)) { advance(); return 0; }
  uint32_t now = c.now();
  if (!_started) { _raisedAt = now; _started = true; }
  _ringing = uint32_t(now - _raisedAt) < 45000;
  if (_ringing && _sound && !_sound->isPlaying() &&
      (!_beeped || uint32_t(now - _lastBeep) >= 3500)) {
    _lastBeep = now; _beeped = true;
    alert.play(*_sound, sound::SoundId::AlarmRing);
  }
  if (!_ringing && _sound && _sound->isPlaying()) _sound->stop();
  static const uint8_t bell[] = {
    0xC0, 0x03, 0xE0, 0x07, 0xF0, 0x0F, 0xF0, 0x0F,
    0xF0, 0x0F, 0xF0, 0x0F, 0xF8, 0x1F, 0xF8, 0x1F,
    0xFC, 0x3F, 0xFE, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0xC0, 0x03, 0x80, 0x01,
  };
  constexpr auto ink = DisplayDriver::LIGHT;
  const int center = c.width() / 2;
  const int width = c.width() - 6;
  const int divider = c.fontHeight(fontBody()) + 4;
  c.drawText(fontBody(), 3, 2, "Calling", ink);
  c.fillRect(3, divider, width, 1, ink);

  const int footer = c.height() - c.fontHeight(fontBody()) - 3;
  const int bellY = divider + 3;
  c.drawXbm(center - 8, bellY, bell, 16, 15, ink);
  const int nameY = bellY + 15;
  const int nameHeight = footer - nameY;
  const char* name = _calls[0].name;
  const auto* font = fontBody();
  const int maxScale = c.fontHeightScaled(font, 2) <= nameHeight ? 2 : 1;
  const int scale = c.fitScale(font, name, width, maxScale);
  if (c.textWidth(font, name) <= width) {
    const int y = nameY + (nameHeight - c.fontHeightScaled(font, scale)) / 2;
    c.drawTextScaled(font, center, y, name, ink, scale, TextAlign::Center);
  } else {
    _title.draw(c, font, 3, nameY, width, nameHeight, name, ink, now);
  }
  c.drawTextEllipsized(fontBody(), center, footer, width, "Press = I'm here",
                       ink, TextAlign::Center);
  return 250;
}
bool CallBellApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::None) return true;
  if (!available()) { advance(); return true; }
  // Freeze the target from the call, never from the most recent incoming DM.
  if (!_svc->sendText(_calls[0].key, "\xF0\x9F\x91\x8B here")) {
    if (_host) _host->postToast("Send failed - press again");
    return true;
  }
  advance();
  return true;
}
CallBellApplet& callBellApplet() { static CallBellApplet a; return a; }
}
