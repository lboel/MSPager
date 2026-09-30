#pragma once

#include "DisplayDriver.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "ST7789Spi.h"

// ST7789 TFT that behaves like SSD1306Display: callers draw in 128x64 coordinates with the
// Adafruit GFX 6x8 font (CP437) metrics, so layouts made for the 128x64 OLED fit unchanged.
// Drawing happens at panel resolution (x: 240/128 nearest neighbour, y: 2x, centred):
// shapes and bitmaps are the scaled OLED pixels, text uses real fonts instead of blown-up
// 5x7 pixels (size 1: t0_17b, size 2/3: Inconsolata Bold). Characters missing from a font
// (CP437 arrows, block) fall back to the scaled bitmap font.
class ST7789CanvasDisplay : public DisplayDriver {
  ST7789Spi display;
  GFXcanvas1 canvas;    // panel resolution
  GFXcanvas1 glyph;     // scratch for one scaled-bitmap character
  U8G2_FOR_ADAFRUIT_GFX big;
  bool _isOn;
  uint16_t _color;
  int _x, _y, _text_size;

  void powerOn();
  void fillLogical(int x, int y, int w, int h);
  void drawBitmapChar(unsigned char c);
  bool drawFontChar(unsigned char c);
public:
  ST7789CanvasDisplay() : DisplayDriver(128, 64),
      display(&SPI1, PIN_TFT_RST, PIN_TFT_DC, PIN_TFT_CS, GEOMETRY_RAWMODE, 240, 135),
      canvas(240, 135), glyph(18, 24) {
    _isOn = false; _color = 1; _x = _y = 0; _text_size = 1;
  }
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
