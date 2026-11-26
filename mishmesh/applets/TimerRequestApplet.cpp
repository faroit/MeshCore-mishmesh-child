#include "TimerRequestApplet.h"

#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/TimerRequest.h>
#include <mishmesh/sound/SoundEngine.h>
#include <mishmesh/sound/Sounds.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {
bool TimerRequestApplet::enqueue(const ConvoKey& key, uint32_t senderTime, const char* text) {
  if (key.type != 0 || !text || *text != '%') return false;
  for (int i = 0; i < _count; ++i)
    if (_requests[i].key.equals(key) && _requests[i].senderTime == senderTime) return true;
  if (_count == 4) return false;
  Request& request = _requests[_count];
  if (!parseTimerRequest(text, request.prompt, request.seconds, &request.alert)) return false;
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

void TimerRequestApplet::removeRequest() {
  if (_count) {
    --_count;
    for (int i = 0; i < _count; ++i) _requests[i] = _requests[i + 1];
  }
  _announce = true;
  _prompt.reset();
}

void TimerRequestApplet::advance() {
  removeRequest();
  if (!_count) _host->pop();
  else _host->requestRender();
}

bool TimerRequestApplet::tick(uint32_t now) {
  if (_state != State::Running || int32_t(now - _deadline) < 0) return false;
  _state = State::Done;
  _ringed = false;
  _prompt.reset();
  return true;
}

int TimerRequestApplet::onRender(Canvas& c) {
  const uint32_t now = c.now();
  const char* name = _timerName;
  const char* footer = "Back: hide timer";
  uint32_t seconds = 0;
  if (_state == State::Idle) {
    if (!available()) {
      if (_host->foreground() == this) advance();
      return 0;
    }
    if (_announce && _host->foreground() == this) {
      if (_sound && _requests[0].alert.volume != 0) _sound->play(sound::SoundId::MsgChime);
      _announce = false;
    }
    name = _requests[0].prompt;
    seconds = _requests[0].seconds;
    footer = "OK Accept / Back Dismiss";
  } else if (_state == State::Running) {
    const int32_t remaining = int32_t(_deadline - now);
    if (remaining > 0) seconds = (uint32_t(remaining) + 999) / 1000;
  } else {
    footer = "OK or Back: dismiss";
    if (_sound && _host->foreground() == this && !_sound->isPlaying() &&
        (!_ringed || (now - _ringStarted < 60000 && now - _lastRing >= 4000))) {
      if (!_ringed) _ringStarted = now;
      _ringed = true;
      _lastRing = now;
      _timerSound.play(*_sound, sound::clockToneId(clockService().timerToneIdx()));
    }
  }

  const int center = c.width() / 2;
  _prompt.draw(c, fontBody(), 2, 2, c.width() - 4, 15,
               name, DisplayDriver::LIGHT, now);

  const auto* font = _state == State::Done ? fontSubtitle() : fontNum();
  char time[12];
  snprintf(time, sizeof(time), "%lu:%02lu", (unsigned long)(seconds / 60),
           (unsigned long)(seconds % 60));
  const char* headline = _state == State::Done ? "Time's up!" : time;
  const int bottom = c.height() - c.fontHeight(fontCaption()) - 2;
  const int maxScale = bottom - 18 >= c.fontHeightScaled(font, 2) ? 2 : 1;
  const int scale = c.fitScale(font, headline, c.width() - 4, maxScale);
  const int timerY = 18 + (bottom - 18 - c.fontHeightScaled(font, scale)) / 2;
  c.drawTextScaled(font, center, timerY, headline, DisplayDriver::LIGHT,
                   scale, TextAlign::Center);
  c.drawTextEllipsized(fontCaption(), 2, bottom, c.width() - 4, footer, DisplayDriver::LIGHT);
  return 250;
}

bool TimerRequestApplet::onInput(InputEvent ev) {
  if (_state == State::Running) {
    if (ev == InputEvent::Back) _host->pop();
    return true;
  }
  if (_state == State::Done) {
    if (ev == InputEvent::Select || ev == InputEvent::Back) {
      if (_sound && _ringed) _sound->stop();
      _state = State::Idle;
      _prompt.reset();
      _host->pop();
    }
    return true;
  }
  if (ev == InputEvent::Back || ev == InputEvent::Cancel || ev == InputEvent::NavLeft) {
    advance();
  } else if (ev == InputEvent::Select) {
    if (!available()) { advance(); return true; }
    memcpy(_timerName, _requests[0].prompt, sizeof(_timerName));
    _timerSound = _requests[0].alert;
    _deadline = _host->nowMs() + _requests[0].seconds * 1000u;
    _state = State::Running;
    removeRequest();
    _host->requestRender();
  }
  return true;
}

TimerRequestApplet& timerRequestApplet() {
  static TimerRequestApplet applet;
  return applet;
}

}  // namespace mishmesh
