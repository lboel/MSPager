#pragma once

// MSPager text cells for the OLED (FRD-013, FRD-022): UTF-8 -> CP437 characters and emoji glyphs.
// Plain C++ (no Arduino), so it runs in the native tests.

#include <stdint.h>
#include "EmojiGlyphs.h"

#define PAGER_IS_ARROW(c)  ((c) >= 0x18 && (c) <= 0x1B)   // CP437 arrows used in button hints

// Decodes one UTF-8 codepoint at s, returns its byte length. Invalid bytes give U+FFFD
// (never reads past a NUL).
static inline int pagerUtf8Decode(const char* s, uint32_t& cp) {
  const unsigned char* p = (const unsigned char*)s;
  int n = p[0] >= 0xF0 ? 4 : p[0] >= 0xE0 ? 3 : p[0] >= 0xC0 ? 2 : 1;
  if (n == 1) {
    cp = p[0] < 0x80 ? p[0] : 0xFFFD;
    return 1;
  }
  cp = p[0] & (0x3F >> (n - 1));
  for (int i = 1; i < n; i++) {
    if ((p[i] & 0xC0) != 0x80) { cp = 0xFFFD; return i; }
    cp = (cp << 6) | (p[i] & 0x3F);
  }
  return n;
}

// emoji glyph index for a codepoint, or -1
static inline int pagerEmojiGlyph(uint32_t cp) {
  int lo = 0, hi = (int)(sizeof(EMOJI_CODEPOINTS) / sizeof(EMOJI_CODEPOINTS[0])) - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (EMOJI_CODEPOINTS[mid].cp == cp) return EMOJI_CODEPOINTS[mid].glyph;
    if (EMOJI_CODEPOINTS[mid].cp < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

// German letters and a few symbols from the CP437 font; anything else becomes a full block
static inline unsigned char pagerCp437(uint32_t cp) {
  switch (cp) {
    case 0xE4: return 0x84;   // ä
    case 0xF6: return 0x94;   // ö
    case 0xFC: return 0x81;   // ü
    case 0xC4: return 0x8E;   // Ä
    case 0xD6: return 0x99;   // Ö
    case 0xDC: return 0x9A;   // Ü
    case 0xDF: return 0xE1;   // ß
    case 0xE9: return 0x82;   // é
    case 0xB0: return 0xF8;   // °
  }
  return 0xDB;                // full block
}

// Invisible parts of emoji sequences: variation selectors, zero-width joiner, skin tones
static inline bool pagerZeroWidth(uint32_t cp) {
  return cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || (cp >= 0x1F3FB && cp <= 0x1F3FF);
}

// One display cell at p, advancing p. Returns an emoji glyph index (>= 0), or -1 with
// c = the CP437 character to print (0 = zero-width, draw nothing).
static inline int pagerNextCell(const char*& p, unsigned char& c) {
  unsigned char b = (unsigned char)*p;
  if (b < 0x80) {
    p++;
    c = (b < 32 && !PAGER_IS_ARROW(b)) ? ' ' : b;
    return -1;
  }
  uint32_t cp;
  p += pagerUtf8Decode(p, cp);
  c = 0;
  if (pagerZeroWidth(cp)) return -1;
  int g = pagerEmojiGlyph(cp);
  if (g < 0) c = pagerCp437(cp);
  return g;
}
