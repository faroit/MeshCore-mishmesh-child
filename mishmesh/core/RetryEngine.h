// mishmesh/core/RetryEngine.h
#pragma once
#include <stdint.h>
#include <mishmesh/core/MessageStore.h>   // ConvoKey

namespace mishmesh {

// Actions the engine asks the host to perform. The host implements these with
// real I/O (re-transmit over the radio, flip a store record to failed); the
// engine itself stays pure so its decisions are host-testable.
struct RetryActions {
  virtual ~RetryActions() {}
  // Re-transmit a still-undelivered direct message. `attempt` is the retry
  // number (1..MAX_RETRIES). `resetPath` asks the host to clear the contact's
  // learned route first, so this attempt falls back to flood routing.
  virtual bool retransmit(const ConvoKey& key, uint32_t senderTime,
                          uint8_t attempt, bool resetPath) = 0;
  // Give up on a message after the last retry - mark it failed.
  virtual void markFailed(const ConvoKey& key, uint32_t senderTime) = 0;
};

// Tracks on-device-sent direct messages that are still awaiting delivery and
// drives automatic re-transmission.
//
// Entries are created only by track(), from the send path, so the tracked set is
// exactly the DMs this session sent and is still waiting on: a reboot starts
// empty and old store records are never resurrected. Each tick the host takes a
// snapshot(), asks the store which of those are still pending, reports them with
// see(), and endScan() re-transmits the due ones and drops the rest. see() never
// creates - a tracked message missing from a scan is delivered, failed or
// deleted, so an attempt count can never be reset behind an entry's back.
//
// No heap: a fixed table of in-flight messages, populated only in track().
class RetryEngine {
public:
  static const int     MAX_PENDING       = 8;     // messages tracked at once
  static const uint8_t MAX_RETRIES        = 5;     // re-transmissions after the first send
  static const uint8_t RESET_PATH_AFTER   = 2;     // reset the route on this retry (when enabled)
  static const uint32_t RETRY_INTERVAL_MS = 8000;  // wait between attempts

  RetryEngine() { reset(); }
  void reset();

  void configure(bool autoRetry, bool autoResetPath, bool persistent = false) {
    _enabled = autoRetry; _resetPath = autoResetPath; _persistent = persistent;
  }
  bool enabled() const { return _enabled; }

  // Start watching a DM the device just sent. Ignored when the table is full -
  // with MAX_PENDING re-transmissions already in flight, a further send simply
  // gets no auto-retry rather than displacing one that does.
  bool track(const ConvoKey& key, uint32_t senderTime, uint32_t now,
             uint32_t baseTimeout = RETRY_INTERVAL_MS);
  bool due(uint32_t now) const;
  void acknowledged(const ConvoKey& key, uint32_t time);
  bool hasRoom() const { return trackedCount() < MAX_PENDING; }
  void onRadioActivity(uint32_t now);
  void onRecipientHeard(const ConvoKey& key, uint32_t now);
  static uint32_t backoff(uint8_t attempt, uint32_t baseTimeout);
  static const uint32_t NUDGE_GAP_MS = 3000;
  static const uint32_t BACKOFF_CAP_MS = 180000;

  // --- scan protocol: call once per tick ---
  void beginScan();                                    // mark all entries unseen
  int  snapshot(ConvoKey* outKeys, uint32_t* outTimes) const;  // entries to ask the store about
  void see(const ConvoKey& key, uint32_t senderTime);  // still pending per the store
  void endScan(uint32_t now, RetryActions& actions);   // reap + fire due retries

  // --- introspection (tests) ---
  int  trackedCount() const;
  bool attemptsFor(const ConvoKey& key, uint32_t senderTime, uint8_t& out) const;

private:
  struct Entry {
    bool     used;
    bool     seen;
    ConvoKey key;
    uint32_t senderTime;
    uint8_t  attempts;
    uint32_t dueAt, lastSendAt, baseTimeout;
    bool forceFlood;
  };
  Entry _e[MAX_PENDING];
  bool  _enabled;
  bool  _resetPath;
  bool  _persistent;

  int find(const ConvoKey& key, uint32_t senderTime) const;
};

}  // namespace mishmesh
