// mishmesh/core/RetryEngine.cpp
#include <mishmesh/core/RetryEngine.h>

namespace mishmesh {

void RetryEngine::reset() {
  for (int i = 0; i < MAX_PENDING; i++) _e[i].used = false;
  _enabled = false;
  _resetPath = false;
  _persistent = false;
}

int RetryEngine::find(const ConvoKey& key, uint32_t senderTime) const {
  for (int i = 0; i < MAX_PENDING; i++)
    if (_e[i].used && _e[i].senderTime == senderTime && _e[i].key.equals(key)) return i;
  return -1;
}

bool RetryEngine::track(const ConvoKey& key, uint32_t senderTime, uint32_t now, uint32_t baseTimeout) {
  if (key.type != 0) return false;                  // direct messages only
  if (find(key, senderTime) >= 0) return true;
  for (int j = 0; j < MAX_PENDING; j++) {
    if (!_e[j].used) {
      _e[j].used = true; _e[j].seen = true;
      _e[j].key = key; _e[j].senderTime = senderTime;
      _e[j].baseTimeout = baseTimeout < NUDGE_GAP_MS ? NUDGE_GAP_MS : baseTimeout;
      _e[j].attempts = 0;
      _e[j].dueAt = now + (_persistent ? backoff(0, _e[j].baseTimeout) : RETRY_INTERVAL_MS);
      _e[j].lastSendAt = now; _e[j].forceFlood = false;
      return true;
    }
  }
  return false;
}

void RetryEngine::beginScan() {
  for (int i = 0; i < MAX_PENDING; i++) _e[i].seen = false;
}

int RetryEngine::snapshot(ConvoKey* outKeys, uint32_t* outTimes) const {
  int n = 0;
  for (int i = 0; i < MAX_PENDING; i++) {
    if (!_e[i].used) continue;
    outKeys[n] = _e[i].key; outTimes[n] = _e[i].senderTime; n++;
  }
  return n;
}

void RetryEngine::see(const ConvoKey& key, uint32_t senderTime) {
  int i = find(key, senderTime);
  if (i >= 0) _e[i].seen = true;
}

void RetryEngine::endScan(uint32_t now, RetryActions& actions) {
  for (int i = 0; i < MAX_PENDING; i++) {
    Entry& e = _e[i];
    if (!e.used) continue;
    if (!e.seen) { e.used = false; continue; }   // delivered/failed/deleted since last scan
    if (!_enabled) continue;                       // tracking only; no retries when off
    if ((int32_t)(now - e.dueAt) < 0) continue;    // not due yet (wrap-safe compare)

    if (!_persistent && e.attempts >= MAX_RETRIES) {               // out of retries -> fail
      actions.markFailed(e.key, e.senderTime);
      e.used = false;
      continue;
    }
    // Keep the original timestamp and vary the attempt byte. Cycle extended
    // attempts rather than saturating at 255 and emitting identical packets.
    uint8_t attempt = e.attempts == 255 ? 4 : e.attempts + 1;
    bool reset = e.forceFlood || (_resetPath &&
                 (_persistent ? attempt % RESET_PATH_AFTER == 0 : attempt == RESET_PATH_AFTER));
    bool sent = actions.retransmit(e.key, e.senderTime, attempt, reset);
    if (sent) { e.attempts = attempt; e.forceFlood = false; }
    // A full packet pool does not consume an attempt. Also rate-limit failed
    // enqueue attempts so continual RX cannot create a busy retry loop.
    e.lastSendAt = now;
    e.dueAt = now + (_persistent ? backoff(e.attempts, e.baseTimeout) : RETRY_INTERVAL_MS);
  }
}

bool RetryEngine::due(uint32_t now) const {
  if (!_enabled) return false;
  for (const auto& e : _e) if (e.used && int32_t(now - e.dueAt) >= 0) return true;
  return false;
}
void RetryEngine::acknowledged(const ConvoKey& key, uint32_t time) {
  int i = find(key, time);
  if (i >= 0) _e[i].used = false;
}

uint32_t RetryEngine::backoff(uint8_t attempt, uint32_t baseTimeout) {
  uint32_t wait = baseTimeout < NUDGE_GAP_MS ? NUDGE_GAP_MS : baseTimeout;
  if (wait >= BACKOFF_CAP_MS) return BACKOFF_CAP_MS;
  while (attempt-- && wait < BACKOFF_CAP_MS) {
    if (wait > BACKOFF_CAP_MS / 2) return BACKOFF_CAP_MS;
    wait *= 2;
  }
  return wait > BACKOFF_CAP_MS ? BACKOFF_CAP_MS : wait;
}

void RetryEngine::onRadioActivity(uint32_t now) {
  if (!_enabled || !_persistent) return;
  for (auto& e : _e) if (e.used && uint32_t(now - e.lastSendAt) >= NUDGE_GAP_MS)
    e.dueAt = now;
}
void RetryEngine::onRecipientHeard(const ConvoKey& key, uint32_t now) {
  if (!_enabled || !_persistent) return;
  for (auto& e : _e) if (e.used && e.key.equals(key)) {
    e.forceFlood = true;
    uint32_t earliest = e.lastSendAt + NUDGE_GAP_MS;
    e.dueAt = int32_t(now - earliest) >= 0 ? now : earliest;
  }
}

int RetryEngine::trackedCount() const {
  int n = 0;
  for (int i = 0; i < MAX_PENDING; i++) if (_e[i].used) n++;
  return n;
}

bool RetryEngine::attemptsFor(const ConvoKey& key, uint32_t senderTime, uint8_t& out) const {
  int i = find(key, senderTime);
  if (i < 0) return false;
  out = _e[i].attempts;
  return true;
}

}  // namespace mishmesh
