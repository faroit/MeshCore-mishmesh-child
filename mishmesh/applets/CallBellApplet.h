#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AlertSound.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/Marquee.h>

namespace mishmesh {
class CallBellApplet : public Applet {
public:
  CallBellApplet() : Applet("Call bell") {}
  bool enqueue(const ConvoKey& key, uint32_t senderTime, const char* name);
  bool pending() const { return _count != 0; }
  bool isOpen() const { return _open; }
  void onStart(AppletContext& ctx) override;
  void onStop() override;
  void onBackground() override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool keepOnWake() const override { return true; }
  bool wakePressCounts() const override { return true; }
  bool blocksSleep() const override { return _ringing; }
private:
  struct Call { ConvoKey key; uint32_t time; char name[40]; };
  Call _calls[4]{};
  uint8_t _count = 0;
  bool _open = false, _started = false, _ringing = true, _beeped = false;
  uint32_t _raisedAt = 0, _lastBeep = 0;
  AppletHost* _host = nullptr;
  MessagesService* _svc = nullptr;
  sound::SoundEngine* _sound = nullptr;
  Marquee _title;
  bool available(AlertSound* alert = nullptr) const;
  void advance();
};
CallBellApplet& callBellApplet();
}
