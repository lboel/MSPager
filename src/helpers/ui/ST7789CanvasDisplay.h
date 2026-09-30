#pragma once

#include "DisplayDriver.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "ST7789Spi.h"

// ST7789 TFT that draws exactly like SSD1306Display: a 128x64 1-bit canvas with the Adafruit GFX
// 6x8 font (CP437), scaled to the panel on endFrame(). Unlike ST7789Display (Arial fonts, scaled
// per primitive), layouts made for the 128x64 OLED look the same, pixel for pixel.
// Horizontal scale: nearest neighbour (240/128 = 1.875), vertical: 2x, centred.
class ST7789CanvasDisplay : public DisplayDriver {
  ST7789Spi display;
  GFXcanvas1 canvas;
  bool _isOn;
  uint16_t _color;

  void powerOn();
public:
  ST7789CanvasDisplay() : DisplayDriver(128, 64),
      display(&SPI1, PIN_TFT_RST, PIN_TFT_DC, PIN_TFT_CS, GEOMETRY_RAWMODE, 240, 135),
      canvas(128, 64) { _isOn = false; _color = 1; }
  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setTextWrap(bool wrap) override { canvas.setTextWrap(wrap); }
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
