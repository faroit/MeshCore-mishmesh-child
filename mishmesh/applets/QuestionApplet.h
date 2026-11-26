#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/core/Question.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/Marquee.h>
#include <mishmesh/widgets/StatusBar.h>

namespace mishmesh {
class QuestionApplet : public Applet, public ListModel {
public:
  QuestionApplet() : Applet("Question") {}
  bool setQuestion(const ConvoKey& key, const MessageView& message, const char* from);
  void onStart(AppletContext& ctx) override;
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool keepOnWake() const override { return true; }
  int count() const override { return _question.count; }
  const char* label(int i) const override { return _question.answer(i); }
private:
  AppletHost* _host = nullptr;
  AppServices* _app = nullptr;
  MessagesService* _svc = nullptr;
  AppletStorage* _storage = nullptr;
  Question _question;
  ConvoKey _key{};
  uint32_t _senderTime = 0, _fingerprint = 0, _replyTime = 0;
  uint8_t _replyStatus = ST_PENDING;
  int _answer = -1;
  char _from[40] = {}, _storageKey[32] = {};
  ListMenu _list;
  StatusBar _bar;
  Marquee _prompt, _answerText;
  bool stillAvailable();
  void saveAnswer();
};
QuestionApplet& questionApplet();
}
