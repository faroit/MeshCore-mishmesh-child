#ifdef MISHMESH_CHILD
#include <mishmesh/core/Applet.h>
#include <mishmesh/core/AppletRegistry.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/RemoteCommands.h>
#include <mishmesh/text/Fonts.h>
#include <mishmesh/widgets/ScrollText.h>
#include <string.h>

namespace mishmesh {
namespace {

class HelpApplet : public Applet {
public:
  HelpApplet() : Applet("Help") {}

  void onStart(AppletContext&) override {
    _text.clear();
    _text.setWrap(true);
    _text.addLine("Send these as direct messages from an Admin contact.");
    for (const auto& command : remote::commands) {
      const char* line = command.usage;
      while (const char* end = strchr(line, '\n')) {
        _text.addf("%.*s", int(end - line), line);
        line = end + 1;
      }
      _text.addLine(line);
    }
  }

  int onRender(Canvas& c) override {
    c.drawText(fontBody(), 3, 2, "Remote commands", DisplayDriver::LIGHT);
    const int top = c.lineHeight(fontBody()) + 4;
    _text.draw(c, 3, top, c.width() - 6, c.height() - top);
    return _text.needsAnimation() ? 33 : 1000;
  }

  bool onInput(InputEvent ev) override { return _text.onInput(ev); }

private:
  ScrollText _text;
};

HelpApplet helpApplet;
MISHMESH_REGISTER_APPLET_ICON(&helpApplet, Placement::AppMenu, "Help", 11, (uint16_t)Icon::Comment);

}  // namespace
}  // namespace mishmesh
#endif
