#include "ParentControlsApplet.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include <stdio.h>
#include <string.h>

namespace mishmesh {
uint32_t ParentControlsApplet::now() const { return _host ? _host->nowMs() : 0; }
void ParentControlsApplet::resetPin() { memcpy(_pin, "000000", 7); _digit = 0; }
void ParentControlsApplet::close() {
  if (_policy) _policy->lock();
  memset(_first, 0, sizeof(_first));
  resetPin();
  _stage = _policy && !_policy->configured() ? NewPin : EnterPin;
  _adding = false;
}
void ParentControlsApplet::onStart(AppletContext& ctx) {
  _host = ctx.host; _app = ctx.app; _contacts = ctx.contacts;
  _policy = _app ? _app->messageAccess() : nullptr;
  close();
  _bar.setTitle("Parent controls");
  _list.setRowHeight(14);
  _list.setModel(this);
  _list.resetSelection();
}
void ParentControlsApplet::showMenu() {
  memset(_first, 0, sizeof(_first));
  resetPin();
  _stage = Menu; _adding = false; _list.resetSelection();
}
int ParentControlsApplet::count() const {
  if (!_policy) return 0;
  return _adding ? (_contacts ? _contacts->countByKind(ContactKind::Chat) + _contacts->countDiscovered() : 0) : CONTACT_START + _policy->count();
}
bool ParentControlsApplet::isToggle(int i) const {
  return !_adding && (i == 0 || (i >= CONTACT_START && i < count()));
}
bool ParentControlsApplet::toggleState(int i) const {
  if (!_policy || !isToggle(i)) return false;
  if (i == 0) return _policy->restricted();
  return _policy->isRemoteParent(_policy->keyAt(i - CONTACT_START));
}
const char* ParentControlsApplet::label(int i) const {
  if (!_policy) return "";
  if (!_adding && i < CONTACT_START) {
    static const char* labels[] = {"Child mode", "Add allowed contact", "Change PIN", "Lock parent controls"};
    return i >= 0 ? labels[i] : "";
  }
  ContactView v;
  const uint8_t* key = nullptr;
  const char* name = "Unknown contact";
  if (_adding) {
    if (!_contacts) return "";
    int known = _contacts->countByKind(ContactKind::Chat);
    bool found = i < known ? _contacts->getByKind(ContactKind::Chat, i, v)
                          : _contacts->getDiscovered(i - known, v);
    if (!found) return "";
    key = v.pubKey; name = v.name;
  } else {
    key = _policy->keyAt(i - CONTACT_START);
    if (!key) return "";
    if (_contacts) for (int j = 0; j < _contacts->countByKind(ContactKind::Chat); ++j) {
      if (_contacts->getByKind(ContactKind::Chat, j, v) && memcmp(key, v.pubKey, 32) == 0) {
        name = v.name; break;
      }
    }
  }
  snprintf(_label, sizeof(_label), "%s%.32s [%02X%02X%02X%02X]",
           _adding ? (_policy->contains(key) ? "Allowed: " : "Add: ") : "",
           name, key[28], key[29], key[30], key[31]);
  return _label;
}
int ParentControlsApplet::onRender(Canvas& c) {
  const int barH = c.fontHeight(fontBody()) + 3;
  if (_stage == Menu && (!_policy || !_policy->unlocked(now()))) close();
  const bool contactSelected = _stage == Menu && !_adding && _list.selected() >= CONTACT_START;
  _bar.setTitle(contactSelected ? "Allowed contacts" : "Parent controls");
  _bar.setBattery(_app ? _app->batteryMillivolts() : 0);
  _bar.draw(c, 0, 0, c.width(), barH);
  if (!_policy || _policy->fault()) {
    c.drawText(fontBody(), 2, barH + 5, "Controls unavailable", DisplayDriver::LIGHT);
    return 1000;
  }
  if (_stage == Menu) {
    if (_adding && count() == 0)
      c.drawText(fontBody(), 2, barH + 5, "No chat contacts yet", DisplayDriver::LIGHT);
    else {
      const int footerH = contactSelected ? c.fontHeight(fontCaption()) + 2 : 0;
      _list.draw(c, 0, barH, c.width(), c.height() - barH - footerH);
      if (contactSelected) {
        const int footerY = c.height() - footerH + 1;
        c.drawText(fontCaption(), 2, footerY, "Right: Admin", DisplayDriver::LIGHT);
        c.drawText(fontCaption(), c.width() - 2, footerY, "OK: Remove", DisplayDriver::LIGHT, TextAlign::Right);
      }
    }
    return 100;
  }
  const char* title = _stage == NewPin ? "Set 6-digit parent PIN" :
                      _stage == ConfirmPin ? "Repeat new PIN" : "Enter parent PIN";
  char prompt[40];
  if (_stage == EnterPin && _policy->waitSeconds(now())) {
    snprintf(prompt, sizeof(prompt), "Try in %lus", (unsigned long)_policy->waitSeconds(now()));
    title = prompt;
  }
  c.drawText(fontBody(), 2, barH + 2, title, DisplayDriver::LIGHT);
  char masked[] = "* * * * * *";
  masked[2 * _digit] = _pin[_digit];
  int y = barH + c.lineHeight(fontBody()) + 3;
  c.drawText(fontBody(), 8, y, masked, DisplayDriver::LIGHT);
  c.drawText(fontCaption(), 2, y + c.lineHeight(fontBody()) + 4,
             "Up/down: digit  OK: next", DisplayDriver::LIGHT);
  return 250;
}
bool ParentControlsApplet::onInput(InputEvent ev) {
  if (!_policy || _policy->fault()) return false;
  if (_stage == Menu && !_policy->unlocked(now())) close();
  if (_stage != Menu) {
    if (ev == InputEvent::Back || ev == InputEvent::Cancel) { close(); return false; }
    if (ev == InputEvent::NavUp) _pin[_digit] = '0' + (_pin[_digit] - '0' + 1) % 10;
    else if (ev == InputEvent::NavDown) _pin[_digit] = '0' + (_pin[_digit] - '0' + 9) % 10;
    else if (ev == InputEvent::NavLeft) { if (_digit > 0) --_digit; }
    else if (ev == InputEvent::NavRight) { if (_digit < 5) ++_digit; }
    else if (ev == InputEvent::Select) {
      if (_digit < 5) { ++_digit; return true; }
      if (_stage == NewPin) {
        memcpy(_first, _pin, 7); resetPin(); _stage = ConfirmPin;
      } else if (_stage == ConfirmPin) {
        bool match = memcmp(_first, _pin, 6) == 0;
        if (match && _policy->setPin(_pin, now())) showMenu();
        else {
          if (_host) _host->postToast(match ? "PIN not saved" : "PINs did not match");
          memset(_first, 0, sizeof(_first)); resetPin(); _stage = NewPin;
        }
      } else if (_policy->unlock(_pin, now())) showMenu();
      else { resetPin(); if (_host) _host->postToast("Locked: wait and retry"); }
    }
    return true;
  }
  if (ev == InputEvent::Back && _adding) { showMenu(); return true; }
  if (!_adding && ev == InputEvent::NavRight && _list.selected() >= CONTACT_START) {
    const int index = _list.selected() - CONTACT_START;
    bool saved = _policy->toggleRemoteParent(index, now());
    if (_host) _host->postToast(!saved ? "Not saved" :
        toggleState(_list.selected()) ? "Admin access on" : "Admin access off");
    return true;
  }
  if (_list.onInput(ev)) return true;
  if (ev != InputEvent::Select) return false;
  int row = _list.selected();
  bool ok = false;
  if (_adding) {
    ContactView v;
    if (_contacts) {
      int known = _contacts->countByKind(ContactKind::Chat);
      bool found = row < known ? _contacts->getByKind(ContactKind::Chat, row, v)
                               : _contacts->getDiscovered(row - known, v);
      if (found && v.type == (uint8_t)ContactKind::Chat) {
        uint8_t key[32]; memcpy(key, v.pubKey, 32);
        // Discovery rows are transient. Copy the full key before promotion.
        if (row < known || _contacts->addDiscovered(key))
          ok = _policy->setAllowed(key, true, now());
      }
    }
  } else if (row == 0) ok = _policy->setRestricted(!_policy->restricted(), now());
  else if (row == 1) { _adding = true; _list.resetSelection(); return true; }
  else if (row == 2) { resetPin(); _stage = NewPin; return true; }
  else if (row == 3) { close(); return true; }
  else {
    const uint8_t* key = _policy->keyAt(row - CONTACT_START);
    if (key) ok = _policy->setAllowed(key, false, now());
    _list.resetSelection();
  }
  if (_host) _host->postToast(ok ? "Saved" : "Not saved / list full");
  return true;
}

static ParentControlsApplet s_parentControls;
MISHMESH_REGISTER_APPLET_ICON(&s_parentControls, Placement::AppMenu, "Parent controls", 10,
                              (uint16_t)Icon::Lock);
}
