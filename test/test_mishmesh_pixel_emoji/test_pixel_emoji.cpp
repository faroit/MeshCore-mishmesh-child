#include <gtest/gtest.h>
#include <mishmesh/core/PixelEmoji.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/EmojiCatalog.h>
#include <mishmesh/text/Fonts.h>
#include <mf_bwfont.h>
#include "FakeDisplayDriver.h"
#include <set>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace mishmesh;
extern "C" { extern const struct mf_bwfont_s mf_bwfont_PixelEmoji; }

namespace {
struct PixelEmojiTest : testing::Test {
  void SetUp() override {
    Canvas::setEmojiRenderer(nullptr, nullptr, nullptr);
    setEmojiCatalog(nullptr, 0);
    initPixelEmoji();
  }
  void TearDown() override {
    Canvas::setEmojiRenderer(nullptr, nullptr, nullptr);
    setEmojiCatalog(nullptr, 0);
  }
};
struct Frame : FakeDisplayDriver {
  uint8_t pixels[64][128]{};
  void fillRect(int x, int y, int w, int h) override {
    FakeDisplayDriver::fillRect(x, y, w, h);
    for (int j = y; j < y + h; ++j) for (int i = x; i < x + w; ++i)
      if (i >= 0 && i < 128 && j >= 0 && j < 64)
        pixels[j][i] = lastColor == UIColor::window_bkg ? 0 : 255;
  }
};
}

TEST_F(PixelEmojiTest, InstallsCatalogAndEveryGlyphHasNinePixelAdvanceAndInk) {
  ASSERT_TRUE(Canvas::hasEmojiRenderer());
  ASSERT_EQ(45, emojiCatalogCount());
  std::set<uint32_t> unique;
  for (int i = 0; i < emojiCatalogCount(); ++i) {
    EXPECT_TRUE(unique.insert(emojiCatalogAt(i)).second);
    unsigned pixels = 0;
    EXPECT_EQ(9, mf_render_character(&mf_bwfont_PixelEmoji.font, 0, 0, 0xE100 + i,
      [](int16_t x, int16_t y, uint8_t count, uint8_t, void* state) {
        EXPECT_GE(x, 0); EXPECT_GE(y, 0); EXPECT_LE(x + count, 9); EXPECT_LT(y, 9);
        *static_cast<unsigned*>(state) += count;
      }, &pixels));
    EXPECT_GT(pixels, 0u);
    EXPECT_LT(pixels, 81u);
  }
  EXPECT_TRUE(unique.count(0x1F514)); // bell
  EXPECT_TRUE(unique.count(0x1F44B)); // wave
  EXPECT_TRUE(unique.count(0x1F355)); // pizza
}
TEST_F(PixelEmojiTest, BellPixelsMatchTheVendoredIcon) {
  bool pixels[9][9]{};
  mf_render_character(&mf_bwfont_PixelEmoji.font, 0, 0, 0xE100,
    [](int16_t x, int16_t y, uint8_t n, uint8_t, void* state) {
      auto p = static_cast<bool (*)[9]>(state);
      for (int i = 0; i < n; ++i) p[y][x + i] = true;
    }, pixels);
  const char* expected[] = {".........", "...###...", "..#####..", "..#####..",
    "..#####..", ".#######.", ".........", "....#....", "........."};
  for (int y = 0; y < 9; ++y) for (int x = 0; x < 9; ++x)
    EXPECT_EQ(expected[y][x] == '#', pixels[y][x]) << x << ',' << y;
}
TEST_F(PixelEmojiTest, UnicodeEmojiAndModifiersMeasureAsOneGlyph) {
  FakeDisplayDriver d; Canvas c(&d);
  for (const char* text : {"🔔", "🔔️", "👋", "👋🏽", "🍕", "🍝", "❤️"}) {
    EXPECT_EQ(11, c.textWidth(fontBody(), text)) << text;
    c.drawText(fontBody(), 0, 0, text, DisplayDriver::LIGHT);
  }
  EXPECT_GT(d.fills.size(), 0u);
  EXPECT_EQ(c.textWidth(fontBody(), " here") + 11, c.textWidth(fontBody(), "👋 here"));
}
TEST_F(PixelEmojiTest, DoesNotReplaceAnOptionalLocalOverlay) {
  uint32_t custom[] = {0x1F600};
  Canvas::setEmojiRenderer(fontBody(), [](uint16_t, uint16_t& out) { out = 'A'; return true; }, nullptr);
  setEmojiCatalog(custom, 1);
  initPixelEmoji();
  EXPECT_EQ(1, emojiCatalogCount()); EXPECT_EQ(0x1F600u, emojiCatalogAt(0));
}
TEST_F(PixelEmojiTest, RendersPagerTextInTheActualSmallScreenLayout) {
  Frame display; Canvas c(&display);
  c.drawText(fontBody(), 2, 1, "Dad: 🔔", DisplayDriver::LIGHT);
  c.drawText(fontBody(), 2, 14, "👋 here", DisplayDriver::LIGHT);
  c.drawText(fontBody(), 2, 27, "Dinner? 🍕 🍝", DisplayDriver::LIGHT);
  c.drawText(fontBody(), 2, 40, "👍 👎 ❤️ ⭐ 🏠", DisplayDriver::LIGHT);
  c.drawText(fontBody(), 2, 53, "🐈 🐕 🚗 🚌 🎁", DisplayDriver::LIGHT);
  ASSERT_GT(display.fills.size(), 50u);
  for (const auto& rect : display.fills) {
    EXPECT_GE(rect.x, 0); EXPECT_GE(rect.y, 0);
    EXPECT_LE(rect.x + rect.w, 128); EXPECT_LE(rect.y + rect.h, 64);
  }
  if (const char* path = std::getenv("MISHMESH_EMOJI_PREVIEW")) {
    FILE* file = std::fopen(path, "wb"); ASSERT_NE(nullptr, file);
    std::fputs("P5\n128 64\n255\n", file);
    EXPECT_EQ(sizeof(display.pixels), std::fwrite(display.pixels, 1, sizeof(display.pixels), file));
    std::fclose(file);
  }
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
