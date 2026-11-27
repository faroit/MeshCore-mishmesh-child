#include <gtest/gtest.h>
#include <mishmesh/core/Question.h>
#include <mishmesh/core/CallBell.h>
#include <mishmesh/applets/QuestionApplet.h>
#include <mishmesh/applets/CallBellApplet.h>
#include <mishmesh/applets/TimerRequestApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
#include <mishmesh/applets/NotificationApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeMessagesService.h"
#include "FakeDisplayDriver.h"
#include <map>
#include <vector>
using namespace mishmesh;

namespace {
struct Storage : AppletStorage {
  std::map<std::string, std::vector<uint8_t>> data;
  uint8_t load(const char* key, uint8_t* dst, uint8_t cap) override {
    auto& v = data[key]; int n = std::min<int>(cap, v.size());
    if (n) memcpy(dst, v.data(), n); return n;
  }
  bool save(const char* key, const uint8_t* src, uint8_t len) override {
    data[key] = std::vector<uint8_t>(src, src + len); return true;
  }
};
struct TestRoot : Applet {
  TestRoot() : Applet("Test root") {}
  int onRender(Canvas&) override { return 0; }
};
struct Messages : FakeMessagesService {
  bool fail = false;
  bool sendText(const ConvoKey& k, const char* text) override {
    return !fail && FakeMessagesService::sendText(k, text);
  }
};
const ConvoKey dad = directKey((const uint8_t*)"DAD123");
const ConvoKey mom = directKey((const uint8_t*)"MOM123");
void receive(Messages& s, const ConvoKey& k, const char* text, uint32_t time = 123) {
  s.store.appendInbound(k, text, strlen(text), time, time, 0, nullptr, 0);
}
}

TEST(Questions, ParseTrimsAndPreservesUtf8) {
  Question q;
  ASSERT_TRUE(parseQuestion(" ?Dinner | Pizza | Pasta ", q));
  EXPECT_STREQ("Dinner", q.prompt()); EXPECT_EQ(2, q.count);
  EXPECT_STREQ("Pasta", q.answer(1));
  ASSERT_TRUE(parseQuestion("?Hungry? | 🍕 | | Gemüse", q));
  EXPECT_STREQ("🍕", q.answer(0)); EXPECT_STREQ("Gemüse", q.answer(1));
}
TEST(Questions, RejectsMalformedAndOversizedInput) {
  Question q;
  for (const char* s : {"", "hello", "?Dinner", "?Dinner | Pizza", "? | A | B",
                       "?Q|1|2|3|4|5|6|7", "Dad: ?Q|A|B"}) EXPECT_FALSE(parseQuestion(s, q)) << s;
  std::string s = "?" + std::string(157, 'x') + "|a|b";
  EXPECT_FALSE(parseQuestion(s.c_str(), q));
  EXPECT_FALSE(parseQuestion(nullptr, q));
}
TEST(Questions, SelectedAnswerTargetsSenderAndSurvivesReopening) {
  Messages svc; Storage storage; receive(svc, dad, "?Dinner | Pizza | Pasta");
  MessageView m; ASSERT_TRUE(svc.getMessage(dad, 0, m));
  QuestionApplet q; ASSERT_TRUE(q.setQuestion(dad, m, "Dad"));
  AppletContext ctx; ctx.messages = &svc; ctx.storage = &storage; q.onStart(ctx);
  q.onInput(InputEvent::NavDown); q.onInput(InputEvent::Select);
  EXPECT_EQ(1, svc.sends); EXPECT_STREQ("Pasta", svc.lastSent.c_str());
  EXPECT_TRUE(dad.equals(svc.lastSentKey));
  q.onInput(InputEvent::Select); EXPECT_EQ(1, svc.sends);
  ASSERT_TRUE(svc.getMessage(dad, 0, m));
  QuestionApplet reopened; ASSERT_TRUE(reopened.setQuestion(dad, m, "Dad"));
  reopened.onStart(ctx); reopened.onInput(InputEvent::Select);
  EXPECT_EQ(1, svc.sends);
}
TEST(Questions, FailedSendCanRetryAndRevokedQuestionCannotReply) {
  Messages svc; receive(svc, dad, "?Dinner|Pizza|Pasta");
  MessageView m; ASSERT_TRUE(svc.getMessage(dad, 0, m));
  QuestionApplet q; ASSERT_TRUE(q.setQuestion(dad, m, "Dad"));
  AppletContext ctx; ctx.messages = &svc; q.onStart(ctx);
  svc.fail = true; q.onInput(InputEvent::Select); EXPECT_EQ(0, svc.sends);
  svc.fail = false; q.onInput(InputEvent::Select); EXPECT_EQ(1, svc.sends);
  svc.store.setVisibilityFilter([](const ConvoKey&, void*) { return false; }, nullptr);
  q.onInput(InputEvent::Select); EXPECT_EQ(1, svc.sends);
}
TEST(Questions, DoesNotInterpretChannelOrOutboundMessages) {
  QuestionApplet q; MessageView m{}; m.kind = KIND_INBOUND; m.text = "?Q|A|B";
  EXPECT_FALSE(q.setQuestion(channelKey(0), m, "Dad"));
  m.outbound = true; EXPECT_FALSE(q.setQuestion(dad, m, "Dad"));
}
TEST(Questions, OpeningThreadShowsChoicesAndBackReturnsToConversation) {
  Messages svc; receive(svc, dad, "?Dinner|Pizza|Pasta");
  FakeDisplayDriver display; AppletContext ctx; ctx.messages = &svc;
  AppletHost host(&display, ctx); MessageThreadApplet thread;
  thread.setTarget(dad, "Dad"); host.setRoot(&thread);
  EXPECT_EQ(&questionApplet(), host.foreground());
  host.loop(1);
  host.dispatch(InputEvent::NavDown); host.dispatch(InputEvent::Select);
  EXPECT_STREQ("Pasta", svc.lastSent.c_str());
  host.dispatch(InputEvent::Back); EXPECT_EQ(&thread, host.foreground());
  host.loop(2);
}
TEST(CallBell, OnlySingleBellIsSpecial) {
  EXPECT_TRUE(isCallBell("🔔")); EXPECT_TRUE(isCallBell(" \t🔔️\n"));
  for (const char* s : {"", "🔔🔔", "Dad: 🔔", "Hello 🔔", "🔔 hello", "🔕"})
    EXPECT_FALSE(isCallBell(s)) << s;
  EXPECT_FALSE(isCallBell(nullptr));
}
TEST(CallBell, AnyKeyRepliesToFrozenCallerAndDuplicateDoesNotQueueTwice) {
  Messages svc; receive(svc, dad, "🔔"); receive(svc, mom, "🔔", 124);
  CallBellApplet bell;
  ASSERT_TRUE(bell.enqueue(dad, 123, "Dad")); ASSERT_TRUE(bell.enqueue(dad, 123, "Dad"));
  ASSERT_TRUE(bell.enqueue(mom, 124, "Mom"));
  AppletContext ctx; ctx.messages = &svc; bell.onStart(ctx);
  bell.onInput(InputEvent::None); EXPECT_EQ(0, svc.sends);
  bell.onInput(InputEvent::NavUp);
  EXPECT_EQ(1, svc.sends); EXPECT_TRUE(svc.lastSentKey.equals(dad));
  EXPECT_STREQ("👋 here", svc.lastSent.c_str()); EXPECT_TRUE(bell.pending());
  bell.onInput(InputEvent::Back);
  EXPECT_EQ(2, svc.sends); EXPECT_TRUE(svc.lastSentKey.equals(mom)); EXPECT_FALSE(bell.pending());
}
TEST(CallBell, SendFailureRetainsCallAndBlockedCallerGetsNoReply) {
  Messages svc; receive(svc, dad, "🔔"); CallBellApplet bell;
  bell.enqueue(dad, 123, "Dad"); AppletContext ctx; ctx.messages = &svc; bell.onStart(ctx);
  svc.fail = true; bell.onInput(InputEvent::Select); EXPECT_TRUE(bell.pending());
  svc.fail = false;
  svc.store.setVisibilityFilter([](const ConvoKey&, void*) { return false; }, nullptr);
  bell.onInput(InputEvent::Select); EXPECT_FALSE(bell.pending()); EXPECT_EQ(0, svc.sends);
}
TEST(CallBell, RingTimeoutDoesNotAcknowledge) {
  Messages svc; receive(svc, dad, "🔔"); CallBellApplet bell;
  bell.enqueue(dad, 123, "Dad"); AppletContext ctx; ctx.messages = &svc; bell.onStart(ctx);
  FakeDisplayDriver display; Canvas canvas(&display);
  canvas.setNow(1); bell.onRender(canvas); EXPECT_TRUE(bell.blocksSleep());
  canvas.setNow(45001); bell.onRender(canvas);
  EXPECT_FALSE(bell.blocksSleep()); EXPECT_TRUE(bell.pending()); EXPECT_EQ(0, svc.sends);
  bell.onInput(InputEvent::Select); EXPECT_EQ(1, svc.sends);
}
TEST(CallBell, QueueIsBoundedAndChannelsRejected) {
  CallBellApplet bell;
  EXPECT_FALSE(bell.enqueue(channelKey(0), 1, "Dad"));
  for (int i = 0; i < 4; ++i) EXPECT_TRUE(bell.enqueue(dad, i, "Dad"));
  EXPECT_FALSE(bell.enqueue(dad, 5, "Dad"));
}
TEST(NotificationPreview, FitsTwoLinesAndOpensWithRightOnThePager) {
  Messages svc;
  FakeDisplayDriver display;
  Canvas canvas(&display);
  AppletContext ctx; ctx.messages = &svc;
  TestRoot root;
  AppletHost host(&display, ctx);
  host.setRoot(&root);
  NotificationApplet notification;
  receive(svc, dad, "First line\nSecond line");
  notification.raise(&svc, dad);
  host.push(&notification);
  canvas.setNow(1);
  EXPECT_EQ(8000, notification.onRender(canvas));
  host.dispatch(InputEvent::NavRight);
  EXPECT_EQ(&messageThreadApplet(), host.foreground());
}

TEST(NotificationPreview, ScrollsNormalMessagesAndQuestionPromptsOnThePager) {
  Messages svc;
  FakeDisplayDriver display;
  Canvas canvas(&display);
  NotificationApplet notification;
  AppletContext ctx; ctx.messages = &svc;
  notification.onStart(ctx);
  const std::string message = "The first line and then the rest of the conversation. " + std::string(106, 'z');
  const std::string prompt = "Choose what you want to eat tonight from the two choices below.";
  auto render = [&]() {
    display.litPixels.clear();
    notification.onRender(canvas);
    return display.litPixels;
  };
  auto settle = [&]() {
    for (int i = 0; i < 8; ++i) render();
    return render();
  };
  canvas.setNow(1);
  receive(svc, dad, message.c_str());
  notification.raise(&svc, dad);
  EXPECT_EQ(message, notification.previewForTest());
  const auto firstPage = render();
  canvas.setNow(2000);
  EXPECT_EQ(firstPage, render());
  canvas.setNow(2001);
  const auto movingPage = render();
  EXPECT_NE(firstPage, movingPage);
  const auto nextPage = settle();
  EXPECT_NE(movingPage, nextPage);
  EXPECT_NE(firstPage, nextPage);
  notification.onInput(InputEvent::NavUp);
  EXPECT_EQ(firstPage, settle());
  canvas.setNow(4001);
  EXPECT_EQ(firstPage, render());  // auto-scroll pauses after manual input
  notification.onInput(InputEvent::NavDown);
  EXPECT_NE(firstPage, render());
  notification.onInput(InputEvent::NavUp);
  EXPECT_EQ(firstPage, settle());
  canvas.setNow(9000);
  EXPECT_GT(notification.onRender(canvas), 0);

  receive(svc, dad, ("?" + prompt + " | Pizza | Pasta").c_str(), 124);
  notification.raise(&svc, dad);
  EXPECT_EQ(prompt, notification.previewForTest());
  const auto questionPage = render();
  canvas.setNow(11000);
  EXPECT_NE(questionPage, render());
}

TEST(TimerRequests, ScrollsLongPromptWithoutAcceptingOrDismissing) {
  Messages svc;
  FakeDisplayDriver display;
  Canvas canvas(&display);
  AppletContext ctx; ctx.messages = &svc;
  TestRoot root;
  AppletHost host(&display, ctx);
  host.setRoot(&root);
  TimerRequestApplet timer;
  const char* request = "%Please bring your school bag and come inside after today's game | 5m";
  receive(svc, dad, request);
  ASSERT_TRUE(timer.enqueue(dad, 123, request));
  host.push(&timer);
  timer.onRender(canvas);
  const auto firstPage = display.litPixels;
  display.litPixels.clear();
  canvas.setNow(2000);
  EXPECT_EQ(ScrollableTextPreview::FRAME_MS, timer.onRender(canvas));
  EXPECT_NE(firstPage, display.litPixels);
  host.dispatch(InputEvent::NavUp);
  for (int i = 0; i < 8; ++i) {
    display.litPixels.clear();
    timer.onRender(canvas);
  }
  EXPECT_EQ(firstPage, display.litPixels);
  EXPECT_EQ(0, svc.sends);
  EXPECT_TRUE(timer.pending());
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, svc.sends);
  EXPECT_TRUE(timer.blocksSleep());
}

TEST(TimerRequests, RepliesToOriginalSenderOnStartAndFinishAfterHiding) {
  Messages svc;
  FakeDisplayDriver display;
  AppletContext ctx; ctx.messages = &svc;
  TestRoot root;
  AppletHost host(&display, ctx);
  host.setRoot(&root);
  TimerRequestApplet timer;
  const char* first = "%Please come in | 5s";
  const char* next = "%Dinner | 1m";
  receive(svc, dad, first);
  receive(svc, mom, next, 124);
  ASSERT_TRUE(timer.enqueue(dad, 123, first));
  ASSERT_TRUE(timer.enqueue(mom, 124, next));
  host.push(&timer);
  host.loop(1000);
  host.dispatch(InputEvent::Select);
  EXPECT_EQ(1, svc.sends);
  EXPECT_TRUE(svc.lastSentKey.equals(dad));
  EXPECT_EQ("Timer started: Please come in", svc.lastSent);

  host.dispatch(InputEvent::Back);
  EXPECT_FALSE(timer.isOpen());
  EXPECT_FALSE(timer.tick(5999));
  EXPECT_EQ(1, svc.sends);
  EXPECT_TRUE(timer.tick(6000));
  EXPECT_FALSE(timer.tick(6001));
  EXPECT_EQ(2, svc.sends);
  EXPECT_TRUE(svc.lastSentKey.equals(dad));
  EXPECT_EQ("Timer finished: Please come in", svc.lastSent);

  host.push(&timer);
  host.dispatch(InputEvent::Select); // close the finished alarm
  EXPECT_EQ(2, svc.sends);
  host.push(&timer);
  host.dispatch(InputEvent::Back); // dismiss the next request
  EXPECT_EQ(3, svc.sends);
  EXPECT_TRUE(svc.lastSentKey.equals(mom));
  EXPECT_EQ("Timer dismissed: Dinner", svc.lastSent);
  EXPECT_FALSE(timer.pending());
}

TEST(TimerRequests, RetriesTemporarySendFailureWithoutDelayingCountdown) {
  Messages svc;
  FakeDisplayDriver display;
  AppletContext ctx; ctx.messages = &svc;
  TestRoot root;
  AppletHost host(&display, ctx);
  host.setRoot(&root);
  TimerRequestApplet timer;
  const char* request = "%Snack | 1s";
  receive(svc, dad, request);
  ASSERT_TRUE(timer.enqueue(dad, 123, request));
  host.push(&timer);
  host.loop(1000);
  svc.fail = true;
  host.dispatch(InputEvent::Select);
  EXPECT_TRUE(timer.tick(2000));
  EXPECT_EQ(0, svc.sends);
  svc.fail = false;
  EXPECT_FALSE(timer.tick(3000));
  ASSERT_EQ(2, svc.sends);

  MessageView msg;
  ASSERT_TRUE(svc.getMessage(dad, 1, msg));
  EXPECT_STREQ("Timer started: Snack", msg.text);
  ASSERT_TRUE(svc.getMessage(dad, 2, msg));
  EXPECT_STREQ("Timer finished: Snack", msg.text);
}

TEST(TimerRequests, LongUtf8NameFitsTheReplyLimit) {
  Messages svc;
  FakeDisplayDriver display;
  AppletContext ctx; ctx.messages = &svc;
  TestRoot root;
  AppletHost host(&display, ctx);
  host.setRoot(&root);
  TimerRequestApplet timer;
  const std::string title = std::string(142, 'a') + "🍕 and more";
  const std::string request = "%" + title + "|1s";
  receive(svc, dad, request.c_str());
  ASSERT_TRUE(timer.enqueue(dad, 123, request.c_str()));
  host.push(&timer);
  host.dispatch(InputEvent::Back);
  EXPECT_EQ("Timer dismissed: " + std::string(142, 'a'), svc.lastSent);
  EXPECT_LE(svc.lastSent.size(), size_t(MAX_TEXT));
}

TEST(ChildPagerStore, RetriesMatchTimestampAndBodyWithoutHidingNewMessages) {
  Messages svc; receive(svc, dad, "🔔");
  EXPECT_TRUE(svc.store.hasInbound(dad, 123, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(mom, 123, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(dad, 124, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(dad, 123, "Hey", 3));
}
int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
