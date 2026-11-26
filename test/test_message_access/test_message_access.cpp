#include <gtest/gtest.h>
#include <helpers/MessageAccessPolicy.h>
#include <Utils.h>
#include <string.h>
#include <mishmesh/applets/ParentControlsApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"

namespace {
struct MemoryPolicy : MessageAccessPolicy::Storage {
  MessageAccessPolicy::Record saved{};
  int state = 0, changes = 0;
  bool fail = false;
  int loadAccessPolicy(MessageAccessPolicy::Record& out) override { out = saved; return state; }
  bool saveAccessPolicy(const MessageAccessPolicy::Record& r) override {
    if (fail) return false;
    saved = r; state = 1; return true;
  }
  void accessPolicyChanged() override { ++changes; }
};
struct Random : mesh::RNG {
  void random(uint8_t* dest, size_t n) override { memset(dest, 0xA5, n); }
};
struct PolicyTest : testing::Test {
  MemoryPolicy storage;
  Random rng;
  MessageAccessPolicy policy;
  uint8_t alice[32] = {1};
  void SetUp() override { policy.begin(&storage, &rng, 0); }
  void setup() { ASSERT_TRUE(policy.setPin("123456", 0)); }
};
}

TEST_F(PolicyTest, NewDevicesDenyMessagesBeforeParentSetup) {
  EXPECT_FALSE(policy.configured());
  EXPECT_FALSE(policy.allows(alice));
  EXPECT_TRUE(policy.restricted());
  EXPECT_FALSE(policy.setPin("123", 0));
  EXPECT_FALSE(policy.setPin("12345x", 0));
  setup();
  EXPECT_TRUE(policy.restricted());
  EXPECT_FALSE(policy.allows(alice));
}
TEST_F(PolicyTest, FullKeyMatchingDoesNotTrustNamesOrShortPrefixes) {
  setup();
  ASSERT_TRUE(policy.setAllowed(alice, true, 1));
  uint8_t impostor[32]; memcpy(impostor, alice, 32); impostor[31] = 1;
  EXPECT_TRUE(policy.allows(alice));
  EXPECT_FALSE(policy.allows(impostor));
  EXPECT_TRUE(policy.setAllowed(alice, true, 2));
  EXPECT_EQ(1, policy.count());
}
TEST_F(PolicyTest, LockedAndExpiredSessionsCannotChangePolicy) {
  setup(); policy.lock();
  EXPECT_FALSE(policy.setAllowed(alice, true, 1));
  EXPECT_FALSE(policy.setRestricted(false, 1));
  EXPECT_FALSE(policy.setPin("654321", 1));
  ASSERT_TRUE(policy.unlock("123456", 1));
  EXPECT_TRUE(policy.restricted());  // admin unlock never opens messaging
  EXPECT_FALSE(policy.setRestricted(false, 120001));
  EXPECT_FALSE(policy.setAllowed(alice, true, 120001));
}
TEST_F(PolicyTest, WrongPinAndRebootBothEnforceRetryDelay) {
  setup(); policy.lock();
  EXPECT_FALSE(policy.unlock("654321", 0));
  EXPECT_FALSE(policy.unlock("123456", 29999));
  EXPECT_TRUE(policy.unlock("123456", 30000));
  policy.begin(&storage, &rng, 0);
  EXPECT_FALSE(policy.unlock("123456", 0));
  EXPECT_TRUE(policy.unlock("123456", 30000));
}
TEST_F(PolicyTest, RevocationAndPinChangeSurviveReboot) {
  setup();
  ASSERT_TRUE(policy.setAllowed(alice, true, 1));
  policy.begin(&storage, &rng, 0);
  EXPECT_TRUE(policy.allows(alice));
  ASSERT_TRUE(policy.unlock("123456", 30000));
  ASSERT_TRUE(policy.setAllowed(alice, false, 30001));
  ASSERT_TRUE(policy.setPin("987654", 30002));
  policy.begin(&storage, &rng, 0);
  EXPECT_FALSE(policy.allows(alice));
  EXPECT_FALSE(policy.unlock("123456", 30000));
  EXPECT_TRUE(policy.unlock("987654", 60000));
}
TEST_F(PolicyTest, DamagedOrIncompleteStorageFailsClosed) {
  setup(); storage.saved.keys[3][20] ^= 1;
  policy.begin(&storage, &rng, 0);
  EXPECT_TRUE(policy.fault());
  EXPECT_TRUE(policy.restricted());
  EXPECT_FALSE(policy.allows(alice));
  EXPECT_FALSE(policy.setPin("123456", 50000));
  storage.state = -1;
  policy.begin(&storage, &rng, 0);
  EXPECT_TRUE(policy.restricted());
}
TEST_F(PolicyTest, FailedWriteNeverGrantsPermissionOrReportsSuccess) {
  setup(); storage.fail = true;
  EXPECT_FALSE(policy.setAllowed(alice, true, 1));
  EXPECT_FALSE(policy.allows(alice));
  EXPECT_FALSE(policy.setRestricted(false, 1));
  EXPECT_TRUE(policy.restricted());
  EXPECT_EQ(1, storage.changes);
}
TEST_F(PolicyTest, ListLimitAndRemovalPreserveOtherKeys) {
  setup(); uint8_t key[32] = {};
  for (int i = 0; i < MessageAccessPolicy::MAX_ALLOWED; ++i) {
    key[0] = i;
    ASSERT_TRUE(policy.setAllowed(key, true, 1));
  }
  key[0] = 99; EXPECT_FALSE(policy.setAllowed(key, true, 1));
  key[0] = 0; ASSERT_TRUE(policy.setAllowed(key, false, 1));
  EXPECT_FALSE(policy.contains(key));
  for (int i = 1; i < MessageAccessPolicy::MAX_ALLOWED; ++i) {
    key[0] = i; EXPECT_TRUE(policy.contains(key));
  }
}
TEST_F(PolicyTest, DelayAndSessionHandleMillisWraparound) {
  setup(); policy.lock();
  uint32_t now = 0xFFFFFF00u;
  policy.begin(&storage, &rng, now);
  EXPECT_FALSE(policy.unlock("123456", now + 29999));
  EXPECT_TRUE(policy.unlock("123456", now + 30000));
  EXPECT_TRUE(policy.unlocked(now + 30001));
  EXPECT_FALSE(policy.unlocked(now + 150000));
}


namespace {
struct ParentApp : mishmesh::AppServices {
  MessageAccessPolicy* policy;
  explicit ParentApp(MessageAccessPolicy* p) : policy(p) {}
  const char* nodeName() const override { return "Pager"; }
  uint16_t batteryMillivolts() const override { return 4000; }
  uint32_t epochSeconds() const override { return 1; }
  MessageAccessPolicy* messageAccess() override { return policy; }
};
void enterPin(mishmesh::ParentControlsApplet& ui, const char* pin) {
  for (int i = 0; i < 6; ++i) {
    for (int d = 0; d < pin[i] - '0'; ++d) ui.onInput(mishmesh::InputEvent::NavUp);
    ui.onInput(mishmesh::InputEvent::Select);
  }
}
}
TEST_F(PolicyTest, ParentScreenRequiresConfirmationAndLocksOnSleep) {
  ParentApp app(&policy); FakeContactsService contacts; FakeDisplayDriver display;
  FakeContactsService::Row row; row.name = "Dad"; memcpy(row.pubkey, alice, 32);
  contacts.chats.push_back(row);
  mishmesh::AppletContext ctx; ctx.app = &app; ctx.contacts = &contacts;
  mishmesh::AppletHost host(&display, ctx); ctx.host = &host;
  mishmesh::ParentControlsApplet ui; ui.onStart(ctx);
  enterPin(ui, "123456");
  EXPECT_FALSE(policy.configured());
  enterPin(ui, "123456");
  ASSERT_TRUE(policy.configured()); ASSERT_TRUE(policy.restricted());
  EXPECT_TRUE(policy.unlocked(0));
  ui.onInput(mishmesh::InputEvent::NavDown); // Add allowed contact
  ui.onInput(mishmesh::InputEvent::Select);
  ui.onInput(mishmesh::InputEvent::Select); // Dad
  EXPECT_TRUE(policy.allows(alice));
  mishmesh::Canvas canvas(&display);
  ui.onRender(canvas); // exercise actual small-screen rendering
  ui.onSleep();
  EXPECT_FALSE(policy.unlocked(0));
  EXPECT_FALSE(policy.setRestricted(false, 0));
}
TEST_F(PolicyTest, ParentScreenWrongConfirmationDoesNotConfigurePin) {
  ParentApp app(&policy); mishmesh::AppletContext ctx; ctx.app = &app;
  mishmesh::ParentControlsApplet ui; ui.onStart(ctx);
  enterPin(ui, "123456"); enterPin(ui, "123457");
  EXPECT_FALSE(policy.configured());
  enterPin(ui, "123456"); enterPin(ui, "123456");
  ASSERT_TRUE(policy.configured());
  ui.onBackground(); EXPECT_FALSE(policy.unlocked(0));
  enterPin(ui, "000000");
  EXPECT_FALSE(policy.unlocked(0));
  EXPECT_TRUE(policy.restricted());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
