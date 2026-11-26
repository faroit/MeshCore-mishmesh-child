// mishmesh/applets/NotificationApplet.h
#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/MessagesService.h>
#include <mishmesh/widgets/ScrollText.h>

namespace mishmesh {

// Modal "new message" banner - an inset card over the dimmed underlying screen
// (isOverlay: the host keeps rendering what's beneath) - raised by
// the notification router when a
// message lands while the device is idle - asleep, or sitting on the home screen.
// Back clears it and Select opens the chat or question. Short ordinary previews
// auto-dismiss after a few seconds. A reused singleton (see notificationApplet()); the subject
// message is copied in via raise() so the banner survives later store mutations.
class NotificationApplet : public Applet {
public:
  NotificationApplet() : Applet("Notify") {}

  void raise(MessagesService* svc, const ConvoKey& key);

  void onStart(AppletContext& ctx) override;
  void onForeground() override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  bool isOpen() const { return _open; }
  bool showingQuestion() const { return _open && _question; }
  bool keepOnWake() const override { return _question || _previewScroll.overflows(); }
  bool wakePressCounts() const override { return _question; }
  void onStop() override { _open = false; }
  bool isOverlay() const override { return true; }

  // test seams
  const char* nameForTest() const { return _name; }
  const char* senderForTest() const { return _sender; }
  const char* previewForTest() const { return _preview; }
  uint16_t    otherUnreadForTest() const { return _otherUnread; }

private:
  AppletHost*      _host = nullptr;
  MessagesService* _svc  = nullptr;
  ConvoKey         _key{};
  bool             _open = false;
  bool             _question = false;
  uint32_t         _questionTime = 0;
  bool             _isChannel = false;
  uint16_t         _otherUnread = 0;     // other chats (not _key) still unread
  uint32_t         _raisedAt = 0;        // 0 => stamp on next render (auto-dismiss base)
  char             _name[34] = {0};      // contact (DM) or channel (group) name
  char             _sender[34] = {0};    // per-message sender, for channels
  char             _preview[MAX_TEXT + 1] = {0};
  ScrollableTextPreview _previewScroll;
};

NotificationApplet& notificationApplet();

}  // namespace mishmesh
