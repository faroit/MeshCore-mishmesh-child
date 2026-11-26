#pragma once
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/StatusBar.h>
#include <helpers/MessageAccessPolicy.h>

namespace mishmesh {
class ParentControlsApplet : public Applet, public ListModel {
public:
  ParentControlsApplet() : Applet("Parent controls") {}
  void onStart(AppletContext& ctx) override;
  void onStop() override { close(); }
  void onSleep() override { close(); }
  void onBackground() override { close(); }
  int onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int count() const override;
  const char* label(int i) const override;
  bool isToggle(int i) const override { return !_adding && i == 0; }
  bool toggleState(int) const override { return _policy && _policy->restricted(); }
private:
  enum Stage { EnterPin, NewPin, ConfirmPin, Menu } _stage = EnterPin;
  AppletHost* _host = nullptr;
  AppServices* _app = nullptr;
  ContactsService* _contacts = nullptr;
  MessageAccessPolicy* _policy = nullptr;
  ListMenu _list;
  StatusBar _bar;
  bool _adding = false;
  int _digit = 0;
  char _pin[7] = "000000", _first[7] = {};
  mutable char _label[80] = {};
  uint32_t now() const;
  void close();
  void resetPin();
  void showMenu();
};
}
