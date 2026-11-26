#pragma once

#include <stdint.h>
#include <stddef.h>

namespace mesh { class RNG; }

// Independent of the contact table: importing, renaming or favouriting a contact
// must never grant permission. Only the local PIN-authenticated UI edits this.
class MessageAccessPolicy {
public:
  static constexpr int MAX_ALLOWED = 32;
  struct Record {
    uint8_t magic[4];
    uint8_t enabled, count, reserved[2];
    uint8_t salt[16], pinHash[32];
    uint8_t keys[MAX_ALLOWED][32];
    uint32_t remoteTimestamp;
    uint8_t checksum[32];
  };
  class Storage {
  public:
    virtual ~Storage() {}
    // 0 = absent, 1 = loaded, -1 = unreadable/incomplete.
    virtual int loadAccessPolicy(Record& out) = 0;
    virtual bool saveAccessPolicy(const Record& record) = 0;
    virtual void accessPolicyChanged() {}
  };

  void begin(Storage* storage, mesh::RNG* rng, uint32_t now);
  bool configured() const { return _configured; }
  bool restricted() const { return _fault || !_configured || _record.enabled; }
  bool fault() const { return _fault; }
  bool contains(const uint8_t* key) const;
  bool allows(const uint8_t* key) const { return !restricted() || (!_fault && contains(key)); }
  int count() const { return _fault ? 0 : _record.count; }
  const uint8_t* keyAt(int i) const { return i >= 0 && i < count() ? _record.keys[i] : nullptr; }
  bool unlocked(uint32_t now) const;
  uint32_t waitSeconds(uint32_t now) const;
  bool unlock(const char* pin, uint32_t now);
  void lock() { _unlocked = false; }
  bool setPin(const char* pin, uint32_t now);
  bool setRestricted(bool enabled, uint32_t now);
  bool setAllowed(const uint8_t* key, bool allowed, uint32_t now);
  bool isRemoteParent(const uint8_t* key) const;
  bool toggleRemoteParent(int index, uint32_t now);
  bool applyRemote(const uint8_t* key, bool restricted, uint32_t timestamp);
  bool claimRemoteCommand(const uint8_t* key, uint32_t timestamp);
  uint32_t revision() const { return _revision; }

private:
  Record _record{};
  Storage* _storage = nullptr;
  mesh::RNG* _rng = nullptr;
  bool _configured = false, _fault = false, _unlocked = false;
  uint32_t _unlockUntil = 0, _attemptAt = 0, _revision = 0;
  bool commit(Record& next, bool notify = true);
};
