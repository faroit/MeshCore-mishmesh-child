#include "TimerRequestApplet.h"

#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/ClockService.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {
namespace {

bool space(char c) { return c == ' ' || c == '\t'; }

// A single positive duration in seconds, minutes or hours.
bool parseRequest(const char* text, char (&prompt)[161], uint32_t& seconds) {
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

}  // namespace

bool TimerRequestApplet::enqueue(const ConvoKey& key, uint32_t senderTime, const char* text) {
  if (key.type != 0 || !text || *text != '%') return false;
  for (int i = 0; i < _count; ++i)
    if (_requests[i].key.equals(key) && _requests[i].senderTime == senderTime) return true;
  if (_count == 4) return false;
  Request& request = _requests[_count];
  if (!parseRequest(text, request.prompt, request.seconds)) return false;
  request.key = key;
  request.senderTime = senderTime;
  ++_count;
  return true;
}

void TimerRequestApplet::onStart(AppletContext& ctx) {
  _host = ctx.host;
  _messages = ctx.messages;
  _sound = ctx.sound;
  _open = true;
}

bool TimerRequestApplet::available() const {
  if (!_count || !_messages) return false;
  const Request& request = _requests[0];
  for (int i = _messages->messageCount(request.key) - 1; i >= 0; --i) {
    MessageView message;
    if (_messages->getMessage(request.key, i, message) &&
        message.kind == KIND_INBOUND && message.senderTime == request.senderTime &&
        message.text[0] == '%') return true;
  }
  return false;
}

void TimerRequestApplet::advance() {
  if (_count) {
    --_count;
    for (int i = 0; i < _count; ++i) _requests[i] = _requests[i + 1];
  }
  _announce = true;
  _prompt.reset();
  if (!_count) _host->pop();
  else _host->requestRender();
}

int TimerRequestApplet::onRender(Canvas& c) {
  if (!available()) { advance(); return 0; }
  if (_announce) {
    if (_sound) _sound->play(sound::SoundId::MsgChime);
    _announce = false;
  }

  const int center = c.width() / 2;
  const int height = c.height();
  const Request& request = _requests[0];
  _prompt.draw(c, fontBody(), 2, 2, c.width() - 4, 15,
               request.prompt, DisplayDriver::LIGHT, c.now());

  char time[12];
  snprintf(time, sizeof(time), "%lu:%02lu", (unsigned long)(request.seconds / 60),
           (unsigned long)(request.seconds % 60));
  const int bottom = height - c.fontHeight(fontCaption()) - 2;
  const int scale = c.fitScale(fontNum(), time, c.width() - 4, 2);
  const int timeHeight = c.fontHeightScaled(fontNum(), scale);
  const int timerY = 18 + (bottom - 18 - timeHeight) / 2;
  c.drawTextScaled(fontNum(), center, timerY, time, DisplayDriver::LIGHT,
                   scale, TextAlign::Center);
  c.drawText(fontCaption(), center, bottom, "OK Accept / Back Dismiss",
             DisplayDriver::LIGHT, TextAlign::Center);
  return 250;
}

bool TimerRequestApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::Back || ev == InputEvent::Cancel || ev == InputEvent::NavLeft) {
    advance();
  } else if (ev == InputEvent::Select) {
    if (!available()) { advance(); return true; }
    if (!clockService().tmStartOnce(_requests[0].seconds, _host->nowMs())) {
      _host->postToast("Timer already active");
      return true;
    }
    advance();
    _host->postToast("Timer started");
  }
  return true;
}

TimerRequestApplet& timerRequestApplet() {
  static TimerRequestApplet applet;
  return applet;
}

}  // namespace mishmesh
