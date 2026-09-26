#include <gtest/gtest.h>

#include <PagerText.h>
#include <string.h>
#include <vector>

// cells of a string: emoji glyph index, or -(CP437 char) for text, zero-width cells skipped
static std::vector<int> cells(const char* s) {
  std::vector<int> out;
  for (const char* p = s; *p; ) {
    unsigned char c;
    int g = pagerNextCell(p, c);
    if (g >= 0) out.push_back(g);
    else if (c) out.push_back(-(int)c);
  }
  return out;
}

TEST(PagerText, CodepointTableSortedAndValid) {
  const int n = sizeof(EMOJI_CODEPOINTS) / sizeof(EMOJI_CODEPOINTS[0]);
  for (int i = 0; i < n; i++) {
    EXPECT_LT(EMOJI_CODEPOINTS[i].glyph, EMOJI_GLYPH_COUNT);
    if (i > 0) EXPECT_LT(EMOJI_CODEPOINTS[i - 1].cp, EMOJI_CODEPOINTS[i].cp);
    EXPECT_EQ(EMOJI_CODEPOINTS[i].glyph, pagerEmojiGlyph(EMOJI_CODEPOINTS[i].cp));
  }
  EXPECT_EQ(-1, pagerEmojiGlyph('A'));
  EXPECT_EQ(-1, pagerEmojiGlyph(0x1F600 + 0x1000));
}

TEST(PagerText, PhoneKeepsItsGlyph) {
  // 📞 was the only glyph before FRD-022; the reply catalogue relies on it
  int g = pagerEmojiGlyph(0x1F4DE);
  ASSERT_GE(g, 0);
  const uint8_t phone[8] = { 0x7E, 0xC3, 0x00, 0x3C, 0x7E, 0x66, 0x7E, 0x00 };
  EXPECT_EQ(0, memcmp(phone, EMOJI_SMALL[g], 8));
}

TEST(PagerText, AsciiUmlautsAndUnknown) {
  EXPECT_EQ((std::vector<int>{ -'O', -'K' }), cells("OK"));
  EXPECT_EQ((std::vector<int>{ -'S', -'p', -0x84, -'t' }), cells("Sp\xC3\xA4t"));
  EXPECT_EQ((std::vector<int>{ -0xDB }), cells("\xE2\x82\xAC"));          // € -> block
  EXPECT_EQ((std::vector<int>{ -' ' }), cells("\t"));                     // control -> space
  EXPECT_EQ((std::vector<int>{ -0x18 }), cells("\x18"));                  // hint arrow kept
}

TEST(PagerText, EmojiWithSelectorsAndSkinTone) {
  int heart = pagerEmojiGlyph(0x2764), thumbs = pagerEmojiGlyph(0x1F44D);
  ASSERT_GE(heart, 0);
  ASSERT_GE(thumbs, 0);
  EXPECT_EQ((std::vector<int>{ heart }), cells("\xE2\x9D\xA4\xEF\xB8\x8F"));             // ❤️ (with FE0F)
  EXPECT_EQ((std::vector<int>{ thumbs }), cells("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"));    // 👍🏽
  EXPECT_EQ((std::vector<int>{ -'J', -'a', -' ', thumbs }), cells("Ja \xF0\x9F\x91\x8D"));
}

TEST(PagerText, AliasesShareAGlyph) {
  EXPECT_EQ(pagerEmojiGlyph(0x1F600), pagerEmojiGlyph(0x1F642));   // 😀 🙂
  EXPECT_EQ(pagerEmojiGlyph(0x2764), pagerEmojiGlyph(0x1F499));    // ❤ 💙
}

TEST(PagerText, BrokenUtf8NeverOverreads) {
  const char truncated[] = { 'A', (char)0xF0, (char)0x9F, 0 };
  EXPECT_EQ((std::vector<int>{ -'A', -0xDB }), cells(truncated));
  const char lone_cont[] = { (char)0x80, 'B', 0 };
  EXPECT_EQ((std::vector<int>{ -0xDB, -'B' }), cells(lone_cont));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
