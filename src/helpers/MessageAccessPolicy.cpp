#include "MessageAccessPolicy.h"
#include <Utils.h>
#include <string.h>
#include <stddef.h>

namespace {
const uint8_t MAGIC[4] = {'M', 'A', 'P', 1};
bool validPin(const char* pin) {
  if (!pin || strlen(pin) != 6) return false;
  for (int i = 0; i < 6; ++i) if (pin[i] < '0' || pin[i] > '9') return false;
  return true;
}
void checksum(const MessageAccessPolicy::Record& r, uint8_t* out) {
  mesh::Utils::sha256(out, 32, reinterpret_cast<const uint8_t*>(&r),
                      offsetof(MessageAccessPolicy::Record, checksum));
}
}

void MessageAccessPolicy::begin(Storage* storage, mesh::RNG* rng, uint32_t now) {
  _storage = storage; _rng = rng;
  _configured = _fault = _unlocked = false;
  memset(&_record, 0, sizeof(_record));
  int result = storage ? storage->loadAccessPolicy(_record) : -1;
  if (result != 0) {
    uint8_t hash[32]; checksum(_record, hash);
    _fault = result != 1 || memcmp(_record.magic, MAGIC, 4) != 0 ||
             _record.enabled > 1 || _record.count > MAX_ALLOWED ||
             memcmp(hash, _record.checksum, 32) != 0;
    _configured = true;
  }
  // A reboot cannot bypass the retry delay.
  _attemptAt = now + (_configured ? 30000 : 0);
  ++_revision;
}

bool MessageAccessPolicy::contains(const uint8_t* key) const {
  if (_fault || !key) return false;
  for (int i = 0; i < _record.count; ++i)
    if (memcmp(key, _record.keys[i], 32) == 0) return true;
  return false;
}

bool MessageAccessPolicy::unlocked(uint32_t now) const {
  return !_fault && _unlocked && int32_t(_unlockUntil - now) > 0;
}
uint32_t MessageAccessPolicy::waitSeconds(uint32_t now) const {
  int32_t remaining = int32_t(_attemptAt - now);
  return remaining > 0 ? (uint32_t(remaining) + 999) / 1000 : 0;
}
bool MessageAccessPolicy::unlock(const char* pin, uint32_t now) {
  if (!_configured || _fault || waitSeconds(now) || !validPin(pin)) return false;
  _unlocked = false;
  _attemptAt = now + 30000;
  uint8_t hash[32];
  mesh::Utils::sha256(hash, 32, _record.salt, 16, reinterpret_cast<const uint8_t*>(pin), 6);
  uint8_t diff = 0;
  for (int i = 0; i < 32; ++i) diff |= hash[i] ^ _record.pinHash[i];
  memset(hash, 0, sizeof(hash));
  if (diff) return false;
  _unlocked = true;
  _unlockUntil = now + 120000;
  return true;
}
bool MessageAccessPolicy::commit(Record& next) {
  checksum(next, next.checksum);
  if (!_storage || !_storage->saveAccessPolicy(next)) return false;
  _record = next;
  _configured = true;
  ++_revision;
  _storage->accessPolicyChanged();
  return true;
}
bool MessageAccessPolicy::setPin(const char* pin, uint32_t now) {
  if (_fault || !validPin(pin) || !_rng || (_configured && !unlocked(now))) return false;
  Record next = _record;
  memcpy(next.magic, MAGIC, 4);
  if (!_configured) next.enabled = 1;
  _rng->random(next.salt, sizeof(next.salt));
  mesh::Utils::sha256(next.pinHash, 32, next.salt, 16,
                      reinterpret_cast<const uint8_t*>(pin), 6);
  if (!commit(next)) return false;
  _unlocked = true;
  _unlockUntil = now + 120000;
  return true;
}
bool MessageAccessPolicy::setRestricted(bool enabled, uint32_t now) {
  if (!unlocked(now)) return false;
  Record next = _record;
  next.enabled = enabled;
  return commit(next);
}
bool MessageAccessPolicy::setAllowed(const uint8_t* key, bool allowed, uint32_t now) {
  if (!unlocked(now) || !key) return false;
  Record next = _record;
  int i = 0;
  while (i < next.count && memcmp(next.keys[i], key, 32) != 0) ++i;
  if (allowed) {
    if (i < next.count) return true;
    if (next.count == MAX_ALLOWED) return false;
    memcpy(next.keys[next.count++], key, 32);
  } else {
    if (i == next.count) return true;
    --next.count;
    memmove(next.keys[i], next.keys[i + 1], (next.count - i) * 32);
    memset(next.keys[next.count], 0, 32);
  }
  return commit(next);
}
