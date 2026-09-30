#ifdef ST7789

#include "ST7789CanvasDisplay.h"

#ifndef ST7789_CANVAS_RGB
  #define ST7789_CANVAS_RGB  ST77XX_WHITE   // foreground colour on the TFT
#endif

#define PANEL_W    240
#define PANEL_H    135
#define SCALE_Y    2
#define OFFSET_Y   ((PANEL_H - 64 * SCALE_Y) / 2)

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
  canvas.setTextColor(_color);
  canvas.setTextSize(1);
  canvas.cp437(true);   // same 256 char 'Code Page 437' font as SSD1306Display
}

void ST7789CanvasDisplay::setTextSize(int sz) {
  canvas.setTextSize(sz);
}

void ST7789CanvasDisplay::setColor(ColorVal c) {
  _color = c ? 1 : 0;
  canvas.setTextColor(_color);
}

void ST7789CanvasDisplay::setCursor(int x, int y) {
  canvas.setCursor(x, y);
}

void ST7789CanvasDisplay::print(const char* str) {
  canvas.print(str);
}

void ST7789CanvasDisplay::fillRect(int x, int y, int w, int h) {
  canvas.fillRect(x, y, w, h, _color);
}

void ST7789CanvasDisplay::drawRect(int x, int y, int w, int h) {
  canvas.drawRect(x, y, w, h, _color);
}

void ST7789CanvasDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  canvas.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t ST7789CanvasDisplay::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// Scale the canvas into the panel buffer (runs of set pixels become one rect),
// ST7789Spi::display() then only sends the changed areas.
void ST7789CanvasDisplay::endFrame() {
  display.clear();
  display.setColor(WHITE);
  for (int y = 0; y < 64; y++) {
    int x = 0;
    while (x < 128) {
      if (!canvas.getPixel(x, y)) { x++; continue; }
      int start = x;
      while (x < 128 && canvas.getPixel(x, y)) x++;
      int x0 = start * PANEL_W / 128;
      int x1 = x * PANEL_W / 128;
      display.fillRect(x0, OFFSET_Y + y * SCALE_Y, x1 - x0, SCALE_Y);
    }
  }
  display.display();
}

#endif
