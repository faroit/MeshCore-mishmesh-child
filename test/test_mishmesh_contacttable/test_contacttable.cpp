// Covers the seam between mishmesh's contact list and BaseChatMesh's contact table.
// meshcore v1.17 reserved the first MAX_ANON_CONTACTS slots of contacts[] for anon
// requests and redefined getNumContacts() to exclude them, which silently shifted every
// mishmesh list that paired that count with a raw array index: the first rows rendered
// blank (empty name, type ADV_TYPE_NONE) and the tail fell off. Nothing in the type
// system moved, so only a test that walks a populated table catches it.
#include <gtest/gtest.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <string.h>

namespace {

class StubRTC : public mesh::RTCClock {
public:
  uint32_t getCurrentTime() override { return 1000; }
  void setCurrentTime(uint32_t time) override { }
};

class StubMillis : public mesh::MillisecondClock {
public:
  unsigned long getMillis() override { return 0; }
};

class StubRNG : public mesh::RNG {
public:
  void random(uint8_t* dest, size_t sz) override { memset(dest, 0, sz); }
};

class StubRadio : public mesh::Radio {
public:
  uint32_t getEstAirtimeFor(int len_bytes) override { return 0; }
  int recvRaw(uint8_t* bytes, int sz) override { return 0; }
  float packetScore(float snr, int packet_len) override { return 0; }
  bool startSendRaw(const uint8_t* bytes, int len) override { return true; }
  bool isSendComplete() override { return true; }
  void onSendFinished() override { }
  bool isInRecvMode() const override { return true; }
  bool isReceiving() override { return false; }
};

// The smallest concrete BaseChatMesh that will stand up: every hook is a no-op, since
// these tests only exercise the contact table.
class TestMesh : public BaseChatMesh {
public:
  bool restricted = false;
  int messages = 0, groups = 0, signedMessages = 0, requests = 0, responses = 0;
  bool isContactAllowed(const ContactInfo& c) const override { return !restricted || c.id.pub_key[31] == 1; }
  bool areChannelsAllowed() const override { return !restricted; }
  bool areRoomMessagesAllowed() const override { return !restricted; }
  using BaseChatMesh::searchPeersByHash;
  using BaseChatMesh::searchChannelsByHash;
  using BaseChatMesh::onPeerDataRecv;
  using BaseChatMesh::onPeerPathRecv;
  using BaseChatMesh::onGroupDataRecv;

  TestMesh(mesh::Radio& r, mesh::MillisecondClock& m, mesh::RNG& g, mesh::RTCClock& c,
           mesh::PacketManager& p, mesh::MeshTables& t) : BaseChatMesh(r, m, g, c, p, t) { }

  void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override { }
  ContactInfo* processAck(const uint8_t*) override { return nullptr; }
  void onContactPathUpdated(const ContactInfo&) override { }
  void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override { ++messages; }
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override { }
  void onCLICommandRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*, char*) override { }
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override { ++signedMessages; }
  uint32_t calcFloodTimeoutMillisFor(uint32_t) const override { return 0; }
  uint32_t calcDirectTimeoutMillisFor(uint32_t, uint8_t) const override { return 0; }
  void onSendTimeout() override { }
  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override { ++groups; }
  uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override { ++requests; return 0; }
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override { ++responses; }
};

struct Fixture {
  StubRadio radio;
  StubMillis millis;
  StubRNG rng;
  StubRTC rtc;
  StaticPoolPacketManager mgr{4};
  SimpleMeshTables tables;
  TestMesh mesh{radio, millis, rng, rtc, mgr, tables};

  void add(const char* name, uint8_t type, uint8_t seed) {
    ContactInfo c;
    memset(&c, 0, sizeof(c));
    memset(c.id.pub_key, seed, PUB_KEY_SIZE);   // distinct key per contact
    snprintf(c.name, sizeof(c.name), "%s", name);
    c.type = type;
    c.out_path_len = OUT_PATH_UNKNOWN;
    ASSERT_TRUE(mesh.addContact(c));
  }
};

}  // namespace

TEST(ContactTable, LogicalIndexZeroIsTheFirstRealContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);

  ASSERT_EQ(1, f.mesh.getNumContacts());
  const ContactInfo* c = f.mesh.getContactPtrByIdx(0);
  ASSERT_NE(nullptr, c);
  // Pre-fix this was contacts[0] - a zeroed anon slot, which is what put a blank
  // header on the contact detail screen.
  EXPECT_STREQ("Alice", c->name);
  EXPECT_EQ(ADV_TYPE_CHAT, c->type);
}

TEST(ContactTable, EveryLogicalIndexResolvesToARealContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Bob", ADV_TYPE_CHAT, 3);

  int n = f.mesh.getNumContacts();
  ASSERT_EQ(3, n);
  for (int i = 0; i < n; i++) {
    const ContactInfo* c = f.mesh.getContactPtrByIdx(i);
    ASSERT_NE(nullptr, c) << "index " << i;
    EXPECT_NE('\0', c->name[0]) << "blank name at index " << i;
    EXPECT_NE(ADV_TYPE_NONE, c->type) << "anon slot leaked at index " << i;
  }
}

TEST(ContactTable, LogicalIndexIsBounded) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);

  EXPECT_EQ(nullptr, f.mesh.getContactPtrByIdx(-1));
  EXPECT_EQ(nullptr, f.mesh.getContactPtrByIdx(f.mesh.getNumContacts()));
}

// The Repeaters tab: scan [0, getNumContacts()) and keep the matching types. This is
// the exact loop shape UITask uses, and the one that reported an empty tab.
TEST(ContactTable, FilteringByTypeFindsRepeaters) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Bob", ADV_TYPE_CHAT, 3);

  int found = 0;
  const char* name = "";
  int n = f.mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    const ContactInfo* c = f.mesh.getContactPtrByIdx(i);
    if (c && c->type == ADV_TYPE_REPEATER) { found++; name = c->name; }
  }
  EXPECT_EQ(1, found);
  EXPECT_STREQ("Hilltop", name);
}

// getTotalContactSlots() counts the reserved slots, getNumContacts() does not. Pinning
// the difference documents why a raw index and a logical one are not interchangeable.
TEST(ContactTable, TotalSlotsIncludeTheReservedAnonSlots) {
  Fixture f;
  EXPECT_EQ(0, f.mesh.getNumContacts());
  EXPECT_EQ(MAX_ANON_CONTACTS, f.mesh.getTotalContactSlots());

  f.add("Alice", ADV_TYPE_CHAT, 1);
  EXPECT_EQ(1, f.mesh.getNumContacts());
  EXPECT_EQ(MAX_ANON_CONTACTS + 1, f.mesh.getTotalContactSlots());
}

// addContact() allocates a slot without checking for an existing pubkey, so callers
// must guard with lookupContactByPubKey() first. MyMesh::uiAddDiscovery skipped that
// and appended a second copy when adding a node that was already a contact.
TEST(ContactTable, AddContactDoesNotDedupeByPubKey) {
  Fixture f;
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);   // same pub_key
  EXPECT_EQ(2, f.mesh.getNumContacts()) << "addContact() is expected NOT to dedupe";
}

TEST(ContactTable, LookupByPubKeyFindsAnExistingContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);

  uint8_t key[PUB_KEY_SIZE];
  memset(key, 2, sizeof(key));
  const ContactInfo* found = f.mesh.lookupContactByPubKey(key, 6);
  ASSERT_NE(nullptr, found);
  EXPECT_STREQ("Hilltop", found->name);

  memset(key, 9, sizeof(key));   // never added
  EXPECT_EQ(nullptr, f.mesh.lookupContactByPubKey(key, 6));
}


TEST(MessageAccess, RejectsOutboundMessagesCommandsAndRequestsBeforeQueueing) {
  Fixture f; f.add("Blocked", ADV_TYPE_CHAT, 2); f.mesh.restricted = true;
  const ContactInfo& c = *f.mesh.getContactPtrByIdx(0);
  uint32_t ack = 0, timeout = 0, tag = 0; uint8_t data[1] = {1};
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendMessage(c, 1, 0, "hello", ack, timeout));
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendCommandData(c, 1, 0, "hello", timeout));
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendLogin(c, "pw", timeout));
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendAnonReq(c, data, 1, tag, timeout));
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendRequest(c, data, 1, tag, timeout));
  EXPECT_EQ(MSG_SEND_FAILED, f.mesh.sendRequest(c, 1, tag, timeout));
  EXPECT_EQ(0, f.mgr.getOutboundTotal());
}
TEST(MessageAccess, AllowedContactStillSends) {
  Fixture f; f.add("Allowed", ADV_TYPE_CHAT, 1); f.mesh.restricted = true;
  uint32_t ack = 0, timeout = 0;
  EXPECT_EQ(MSG_SEND_SENT_FLOOD, f.mesh.sendMessage(*f.mesh.getContactPtrByIdx(0), 1, 0, "hello", ack, timeout));
  EXPECT_EQ(1, f.mgr.getOutboundTotal());
}
TEST(MessageAccess, DeniedPeerIsNotADecryptionCandidateAndRecheckedBeforeDelivery) {
  Fixture f; f.add("Blocked", ADV_TYPE_CHAT, 2);
  uint8_t hash[1] = {2}, secret[32] = {}, data[32] = {1,0,0,0,0,'h','i',0};
  ASSERT_EQ(1, f.mesh.searchPeersByHash(hash));
  f.mesh.restricted = true;
  mesh::Packet p{};
  f.mesh.onPeerDataRecv(&p, PAYLOAD_TYPE_TXT_MSG, 0, secret, data, 8);
  f.mesh.onPeerDataRecv(&p, PAYLOAD_TYPE_REQ, 0, secret, data, 8);
  f.mesh.onPeerDataRecv(&p, PAYLOAD_TYPE_RESPONSE, 0, secret, data, 8);
  EXPECT_FALSE(f.mesh.onPeerPathRecv(&p, 0, secret, data, 0, PAYLOAD_TYPE_RESPONSE, data, 8));
  EXPECT_EQ(0, f.mesh.messages); EXPECT_EQ(0, f.mesh.requests); EXPECT_EQ(0, f.mesh.responses);
  EXPECT_EQ(0, f.mgr.getOutboundTotal());
  EXPECT_EQ(0, f.mesh.searchPeersByHash(hash));
}
TEST(MessageAccess, AllowedPeerReceivesButRoomForwardedMessagesAreDropped) {
  Fixture f; f.add("Allowed", ADV_TYPE_CHAT, 1); f.mesh.restricted = true;
  uint8_t hash[1] = {1}, secret[32] = {}, data[32] = {1,0,0,0,0,'h','i',0};
  ASSERT_EQ(1, f.mesh.searchPeersByHash(hash));
  mesh::Packet p{};
  f.mesh.onPeerDataRecv(&p, PAYLOAD_TYPE_TXT_MSG, 0, secret, data, 8);
  EXPECT_EQ(1, f.mesh.messages);
  data[4] = TXT_TYPE_SIGNED_PLAIN << 2;
  f.mesh.onPeerDataRecv(&p, PAYLOAD_TYPE_TXT_MSG, 0, secret, data, 16);
  EXPECT_EQ(0, f.mesh.signedMessages);
}
TEST(MessageAccess, ExistingAndNewChannelsCannotSendReceiveOrExposeKeys) {
  Fixture f;
  ChannelDetails cd{}; memset(cd.channel.secret, 1, 16);
  ASSERT_TRUE(f.mesh.setChannel(0, cd));
  ASSERT_TRUE(f.mesh.getChannel(0, cd));
  mesh::GroupChannel matches[4];
  ASSERT_GT(f.mesh.searchChannelsByHash(cd.channel.hash, matches, 4), 0);
  f.mesh.restricted = true;
  EXPECT_EQ(0, f.mesh.searchChannelsByHash(cd.channel.hash, matches, 4));
  EXPECT_FALSE(f.mesh.getChannel(0, cd));
  EXPECT_FALSE(f.mesh.setChannel(1, cd));
  EXPECT_EQ(nullptr, f.mesh.addChannel("Public", "izOH6cXN6mrJ5e26oRXNcg=="));
  EXPECT_FALSE(f.mesh.sendGroupMessage(1, cd.channel, "Me", "hello", 5));
  uint8_t data[32] = {1,0,0,0,0,'h','i',0};
  EXPECT_FALSE(f.mesh.sendGroupData(cd.channel, nullptr, OUT_PATH_UNKNOWN, 1, data, 1));
  mesh::Packet p{};
  f.mesh.onGroupDataRecv(&p, PAYLOAD_TYPE_GRP_TXT, cd.channel, data, 8);
  EXPECT_EQ(0, f.mesh.groups); EXPECT_EQ(0, f.mgr.getOutboundTotal());
  // Disabling parent controls restores the retained channel, without rejoining.
  f.mesh.restricted = false;
  EXPECT_TRUE(f.mesh.getChannel(0, cd));
}

TEST(MessageAccess, FullLengthMessageSupportsExtendedRetries) {
  Fixture f; f.add("Allowed", ADV_TYPE_CHAT, 1); f.mesh.restricted = true;
  auto* contact = f.mesh.getContactPtrByIdx(0);
  char text[161]; memset(text, 'x', 160); text[160] = 0;
  uint32_t ack = 0, timeout = 0;
  EXPECT_NE(MSG_SEND_FAILED, f.mesh.sendMessage(*contact, 123, 255, text, ack, timeout));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
