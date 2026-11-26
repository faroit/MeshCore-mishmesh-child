#include <gtest/gtest.h>
#include <mishmesh/core/Question.h>
#include <mishmesh/core/CallBell.h>
#include <mishmesh/applets/QuestionApplet.h>
#include <mishmesh/applets/CallBellApplet.h>
#include <mishmesh/applets/MessageThreadApplet.h>
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
TEST(ChildPagerStore, RetriesMatchTimestampAndBodyWithoutHidingNewMessages) {
  Messages svc; receive(svc, dad, "🔔");
  EXPECT_TRUE(svc.store.hasInbound(dad, 123, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(mom, 123, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(dad, 124, "🔔", strlen("🔔")));
  EXPECT_FALSE(svc.store.hasInbound(dad, 123, "Hey", 3));
}
int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
