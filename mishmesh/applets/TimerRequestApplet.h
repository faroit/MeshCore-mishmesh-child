#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AlertSound.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

class TimerRequestApplet : public Applet {
public:
  TimerRequestApplet() : Applet("Timer request") {}
  bool enqueue(const ConvoKey& key, uint32_t senderTime, const char* text);
  bool pending() const { return _state == State::Done || (_state == State::Idle && _count != 0); }
  bool tick(uint32_t now);
  bool blocksSleep() const override { return _state == State::Running; }
  bool isOpen() const { return _open; }
  void onStart(AppletContext& ctx) override;
  void onStop() override { _open = false; }
  bool keepOnWake() const override { return true; }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;

private:
  struct Request {
    ConvoKey key;
    uint32_t senderTime;
    uint32_t seconds;
    char prompt[161];
    AlertSound alert;
  };
  struct StatusReply {
    ConvoKey key;
    uint32_t expiresAt;
    char text[MAX_TEXT + 1];
  };
  enum class State { Idle, Running, Done };
  State _state = State::Idle;
  ConvoKey _timerSender{};
  char _timerName[161]{};
  AlertSound _timerSound;
  uint32_t _deadline = 0;
  uint32_t _ringStarted = 0;
  uint32_t _lastRing = 0;
  bool _ringed = false;
  Request _requests[4]{};
  int _count = 0;
  StatusReply _replies[8]{};
  int _replyCount = 0;
  uint32_t _nextReplyAttempt = 0;
  bool _open = false;
  bool _announce = true;
  AppletHost* _host = nullptr;
  MessagesService* _messages = nullptr;
  sound::SoundEngine* _sound = nullptr;
  ScrollableTextPreview _prompt;

  bool available() const;
  void sendStatus(const ConvoKey& key, const char* name, const char* status, uint32_t now);
  void retryStatusReplies(uint32_t now);
  void removeRequest();
  void advance();
};

TimerRequestApplet& timerRequestApplet();

}  // namespace mishmesh
