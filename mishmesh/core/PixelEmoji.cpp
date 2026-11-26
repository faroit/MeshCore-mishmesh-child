#include "PixelEmoji.h"
#include "Canvas.h"
#include "EmojiCatalog.h"
#include <mf_bwfont.h>

extern "C" { extern const struct mf_bwfont_s mf_bwfont_PixelEmoji; }

namespace mishmesh {
namespace {
#include <mishmesh/text/pixel-icons/Map.inc>
bool lookup(uint16_t key, uint16_t& glyph) {
  for (const auto& e : pixelEmojiMap) {
    if (e.key == key) { glyph = e.glyph; return true; }
  }
  return false;
}
bool zeroWidth(uint16_t key) {
  return key == 0xFE0E || key == 0xFE0F || key == 0x200D ||
         (key >= 0xF3FB && key <= 0xF3FF); // optional skin tones on hand emojis
}
}
void initPixelEmoji() {
  if (Canvas::hasEmojiRenderer()) return;
  Canvas::setEmojiRenderer(&mf_bwfont_PixelEmoji.font, lookup, zeroWidth);
  setEmojiCatalog(pixelEmojiCatalog, sizeof(pixelEmojiCatalog) / sizeof(pixelEmojiCatalog[0]));
}
}
