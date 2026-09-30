#ifdef ST7789

#include "ST7789CanvasDisplay.h"

#ifndef ST7789_CANVAS_RGB
  #define ST7789_CANVAS_RGB  ST77XX_WHITE   // foreground colour on the TFT
#endif

#define PANEL_W    240
#define PANEL_H    135
#define SCALE_Y    2
#define OFFSET_Y   ((PANEL_H - 64 * SCALE_Y) / 2)

// panel x of logical x (floor, also for negative x from scrolling text)
static int mapX(int x) { return x >= 0 ? x * PANEL_W / 128 : -((-x * PANEL_W + 127) / 128); }
static int mapY(int y) { return OFFSET_Y + y * SCALE_Y; }

// Color scheme (1-bit canvas)
ColorVal UIColor::window_bkg = 0;
ColorVal UIColor::title_bkg = 0;
ColorVal UIColor::title_txt = 1;
ColorVal UIColor::primary_txt = 1;
ColorVal UIColor::secondary_txt = 1;
ColorVal UIColor::warning_txt = 1;
ColorVal UIColor::popup_bkg = 0;
ColorVal UIColor::popup_txt = 1;
ColorVal UIColor::corp_blue = 1;

static void backlight(bool on) {
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  digitalWrite(PIN_TFT_LEDA_CTL, on ? PIN_TFT_LEDA_CTL_ACTIVE : !PIN_TFT_LEDA_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_LEDA_CTL, on ? LOW : HIGH);
#endif
}

// CP437 codes the callers print (see ui-pager/PagerText.h) -> Unicode for the u8g2 fonts
static uint16_t cp437ToUnicode(unsigned char c) {
  if (c >= 0x20 && c < 0x7F) return c;
  switch (c) {
    case 0x84: return 0xE4;   // ä
    case 0x94: return 0xF6;   // ö
    case 0x81: return 0xFC;   // ü
    case 0x8E: return 0xC4;   // Ä
    case 0x99: return 0xD6;   // Ö
    case 0x9A: return 0xDC;   // Ü
    case 0xE1: return 0xDF;   // ß
    case 0x82: return 0xE9;   // é
    case 0xF8: return 0xB0;   // °
  }
  return 0;                   // arrows, block, ...: scaled bitmap font
}

void ST7789CanvasDisplay::powerOn() {
  digitalWrite(PIN_TFT_VDD_CTL, LOW);
  digitalWrite(PIN_TFT_RST, HIGH);
  display.init();
  display.landscapeScreen();
#ifdef DISPLAY_FLIP_VERTICALLY
  display.flipScreenVertically();
#endif
  display.setRGB(ST7789_CANVAS_RGB);
  display.displayOn();
}

bool ST7789CanvasDisplay::begin() {
  if (!_isOn) {
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    powerOn();
    backlight(true);
    _isOn = true;
  }
  glyph.cp437(true);   // same 256 char 'Code Page 437' font as SSD1306Display
  glyph.setTextWrap(false);
  big.begin(canvas);
  return true;
}

void ST7789CanvasDisplay::turnOn() {
  if (!_isOn) {
    powerOn();
    delay(20);
    backlight(true);   // after init, so the old frame isn't flashed
    _isOn = true;
  }
}

void ST7789CanvasDisplay::turnOff() {
  digitalWrite(PIN_TFT_VDD_CTL, HIGH);
  backlight(false);
  digitalWrite(PIN_TFT_RST, LOW);
  _isOn = false;
}

void ST7789CanvasDisplay::clear() {
  canvas.fillScreen(0);
  display.clear();
  display.display();
}

void ST7789CanvasDisplay::startFrame(ColorVal bkg) {
  canvas.fillScreen(0);
  _color = 1;
  _text_size = 1;
}

void ST7789CanvasDisplay::setTextSize(int sz) {
  _text_size = sz < 1 ? 1 : (sz > 3 ? 3 : sz);
}

void ST7789CanvasDisplay::setColor(ColorVal c) {
  _color = c ? 1 : 0;
}

void ST7789CanvasDisplay::setCursor(int x, int y) {
  _x = x;
  _y = y;
}

// logical rect -> panel rect
void ST7789CanvasDisplay::fillLogical(int x, int y, int w, int h) {
  int x0 = mapX(x);
  canvas.fillRect(x0, mapY(y), mapX(x + w) - x0, h * SCALE_Y, _color);
}

// one character of the GFX 6x8 font at the cursor, pixels scaled like everything else
void ST7789CanvasDisplay::drawBitmapChar(unsigned char c) {
  int sz = _text_size;
  glyph.fillScreen(0);
  glyph.drawChar(0, 0, c, 1, 0, sz);
  for (int gy = 0; gy < 8 * sz; gy++) {
    for (int gx = 0; gx < 6 * sz; gx++) {
      if (glyph.getPixel(gx, gy)) fillLogical(_x + gx, _y + gy, 1, 1);
    }
  }
}

// large font glyph, centred on the 5*sz wide GFX glyph box, top of the capitals where the
// GFX capitals start. false: not in the font
bool ST7789CanvasDisplay::drawBigChar(unsigned char c) {
  uint16_t u = cp437ToUnicode(c);
  if (u == 0) return false;
  big.setFont(_text_size == 2 ? u8g2_font_inb24_mf : u8g2_font_inb38_mn);
  big.setFontMode(1);   // transparent - after setFont(), which resets it to opaque (black bg box)
  if (!u8g2_IsGlyph(&big.u8g2, u)) return false;
  if (u == ' ') return true;
  int adv = u8g2_GetGlyphWidth(&big.u8g2, u);
  int box = mapX(_x + 5 * _text_size) - mapX(_x);
  int baseline = mapY(_y) + 7 * _text_size * SCALE_Y - 2;
  big.setForegroundColor(_color);
  big.drawGlyph(mapX(_x) + (box - adv) / 2, baseline, u);
  return true;
}

void ST7789CanvasDisplay::print(const char* str) {
  for (const unsigned char* p = (const unsigned char*)str; *p; p++) {
    if (*p == '\n') { _x = 0; _y += 8 * _text_size; continue; }
    if (*p == '\r') continue;
    if (_text_size == 1 || !drawBigChar(*p)) drawBitmapChar(*p);
    _x += 6 * _text_size;   // GFX advance, so getTextWidth() and layouts stay as on the OLED
  }
}

void ST7789CanvasDisplay::fillRect(int x, int y, int w, int h) {
  fillLogical(x, y, w, h);
}

void ST7789CanvasDisplay::drawRect(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  fillLogical(x, y, w, 1);
  fillLogical(x, y + h - 1, w, 1);
  fillLogical(x, y, 1, h);
  fillLogical(x + w - 1, y, 1, h);
}

void ST7789CanvasDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  int bytes_per_row = (w + 7) / 8;
  for (int by = 0; by < h; by++) {
    for (int bx = 0; bx < w; bx++) {
      if (pgm_read_byte(bits + by * bytes_per_row + bx / 8) & (0x80 >> (bx & 7))) {
        fillLogical(x + bx, y + by, 1, 1);
      }
    }
  }
}

uint16_t ST7789CanvasDisplay::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  glyph.setTextSize(_text_size);
  glyph.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// copy the canvas into the panel buffer (runs of set pixels),
// ST7789Spi::display() then only sends the changed areas
void ST7789CanvasDisplay::endFrame() {
  const int bytes_per_row = (PANEL_W + 7) / 8;
  const uint8_t* buf = canvas.getBuffer();   // row-major, MSB = leftmost pixel
  display.clear();
  display.setColor(WHITE);
  for (int y = 0; y < PANEL_H; y++) {
    const uint8_t* row = buf + y * bytes_per_row;
    int start = -1;
    for (int x = 0; x <= PANEL_W; x++) {
      if (x < PANEL_W && (x & 7) == 0 && start < 0 && row[x >> 3] == 0) { x += 7; continue; }
      bool on = x < PANEL_W && (row[x >> 3] & (0x80 >> (x & 7)));
      if (on && start < 0) start = x;
      if (!on && start >= 0) { display.fillRect(start, y, x - start, 1); start = -1; }
    }
  }
  display.display();
}

#endif
