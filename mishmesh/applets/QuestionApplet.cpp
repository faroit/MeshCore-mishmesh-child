#include "QuestionApplet.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/StrUtil.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {
bool QuestionApplet::setQuestion(const ConvoKey& key, const MessageView& m, const char* from) {
  if (key.type != 0 || m.outbound || m.kind != KIND_INBOUND || !parseQuestion(m.text, _question)) return false;
  _key = key; _senderTime = m.senderTime; _fingerprint = questionFingerprint(m.text);
  copyStr(_from, sizeof(_from), from ? from : "Parent");
  snprintf(_storageKey, sizeof(_storageKey), "q%02x%02x%02x%02x%02x%02x%08lx",
           key.id[0], key.id[1], key.id[2], key.id[3], key.id[4], key.id[5], (unsigned long)_senderTime);
  _answer = -1; _replyTime = 0; _replyStatus = ST_PENDING;
  return true;
}
void QuestionApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _app = ctx.app; _svc = ctx.messages; _storage = ctx.storage;
  _list.setModel(this); _list.setRowHeight(14); _list.resetSelection();
  _bar.setTitle(_from); _prompt.reset(); _answerText.reset();
  uint8_t saved[10];
  if (_storage && _storage->load(_storageKey, saved, sizeof(saved)) == sizeof(saved) && saved[0] == 1) {
    uint32_t fingerprint;
    memcpy(&fingerprint, saved + 2, 4);
    if (fingerprint == _fingerprint && saved[1] < _question.count) {
      _answer = saved[1]; memcpy(&_replyTime, saved + 6, 4);
    }
  }
}
bool QuestionApplet::stillAvailable() {
  if (!_svc) return false;
  bool found = false;
  int n = _svc->messageCount(_key);
  for (int i = n - 1; i >= 0; --i) {
    MessageView m;
    if (!_svc->getMessage(_key, i, m)) continue;
    if (m.kind == KIND_INBOUND && m.senderTime == _senderTime &&
        questionFingerprint(m.text) == _fingerprint) found = true;
    if (_answer >= 0 && m.kind == KIND_OUT_DM && m.senderTime == _replyTime)
      _replyStatus = m.status;
  }
  return found;
}
void QuestionApplet::saveAnswer() {
  uint8_t saved[10] = {1, (uint8_t)_answer};
  memcpy(saved + 2, &_fingerprint, 4); memcpy(saved + 6, &_replyTime, 4);
  if (_storage && !_storage->save(_storageKey, saved, sizeof(saved)) && _host)
    _host->postToast("Reply queued; not saved");
}
int QuestionApplet::onRender(Canvas& c) {
  int barH = c.fontHeight(fontBody()) + 3;
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, c.width(), barH);
  if (!stillAvailable()) {
    c.drawText(fontBody(), 2, barH + 4, "Question unavailable", DisplayDriver::LIGHT);
    return 500;
  }
  const int qh = c.lineHeight(fontBody()) + 4;
  _prompt.draw(c, fontBody(), 2, barH, c.width() - 4, qh,
               _question.prompt(), DisplayDriver::LIGHT, c.now());
  int y = barH + qh;
  if (_answer < 0) _list.draw(c, 0, y, c.width(), c.height() - y);
  else {
    const char* state = _replyStatus == ST_DELIVERED ? "Delivered" :
                        _replyStatus == ST_FAILED ? "Failed - press to retry" : "Reply queued";
    c.drawText(fontCaption(), 2, y, state, DisplayDriver::LIGHT);
    _answerText.draw(c, fontBody(), 2, y + 12, c.width() - 4, 14,
                     _question.answer(_answer), DisplayDriver::LIGHT, c.now());
  }
  return 250;
}
bool QuestionApplet::onInput(InputEvent ev) {
  if (ev == InputEvent::NavLeft) { if (_host) _host->pop(); return true; }
  if (ev == InputEvent::Back || ev == InputEvent::Cancel) return false;
  if (!stillAvailable()) return true;
  if (_answer < 0 && _list.onInput(ev)) return true;
  if (ev != InputEvent::Select && ev != InputEvent::NavRight) return true;
  if (_answer >= 0 && _replyStatus != ST_FAILED) return true;
  int choice = _answer >= 0 ? _answer : _list.selected();
  if (choice < 0 || choice >= _question.count) return true;
  if (!_svc->sendText(_key, _question.answer(choice))) {
    if (_host) _host->postToast("Send failed - try again");
    return true;
  }
  _answer = choice; _replyStatus = ST_PENDING;
  MessageView latest;
  int n = _svc->messageCount(_key);
  if (n > 0 && _svc->getMessage(_key, n - 1, latest) && latest.kind == KIND_OUT_DM)
    _replyTime = latest.senderTime;
  saveAnswer();
  return true;
}
QuestionApplet& questionApplet() { static QuestionApplet q; return q; }
}
