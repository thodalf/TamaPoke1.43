#pragma once
// Custom RGB-LCD driver for the 2.8" round board (ST7701, RGB565 parallel),
// replacing Arduino_ESP32RGBPanel + Arduino_RGB_Display for this board only.
//
// WHY THIS EXISTS: Arduino_ESP32RGBPanel::getFrameBuffer() hardcodes its
// esp_lcd_rgb_panel_config_t.clk_src to LCD_CLK_SRC_DEFAULT (which resolves
// to PLL160M on this core), not exposed via its constructor. Waveshare's own
// official demo (Display_ST7701.cpp, confirmed by downloading the real
// Demo.zip and reading it -- see PORTAGE_28ROUND.md) uses LCD_CLK_SRC_PLL240M
// for this exact panel. Every pixel-clock value tried against the library's
// PLL160M path (30/16/12/10/8 MHz) plateaued with the SAME residual
// right/bottom-edge artifact, which is consistent with the clock SOURCE
// itself being wrong rather than the requested frequency. This class talks
// to the ESP-IDF esp_lcd_rgb_panel driver directly so PLL240M can actually
// be requested.
//
// WHY IT CAN BE THIS SMALL: this board's `gfx` is an Arduino_Canvas wrapping
// this class (see TamaPoke.ino), so the game never calls any drawing
// primitive on THIS object directly -- it draws into the Canvas's own
// off-screen buffer, and Canvas::flush() is the ONLY thing that ever reaches
// this class, via exactly one call: draw16bitRGBBitmap(0, 0, buf, W, H) for
// the whole frame at once. Arduino_G (not the much larger Arduino_GFX) is
// the right base class for that reason: it has no per-pixel drawing API.
#include <Arduino.h>
#include "Arduino_G.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

class TamaRgbPanel : public Arduino_G {
public:
  TamaRgbPanel(
      int16_t w, int16_t h,
      int8_t de, int8_t vsync, int8_t hsync, int8_t pclk,
      int8_t r0, int8_t r1, int8_t r2, int8_t r3, int8_t r4,
      int8_t g0, int8_t g1, int8_t g2, int8_t g3, int8_t g4, int8_t g5,
      int8_t b0, int8_t b1, int8_t b2, int8_t b3, int8_t b4,
      uint16_t hsync_polarity, uint16_t hsync_front_porch, uint16_t hsync_pulse_width, uint16_t hsync_back_porch,
      uint16_t vsync_polarity, uint16_t vsync_front_porch, uint16_t vsync_pulse_width, uint16_t vsync_back_porch,
      uint16_t pclk_active_neg, int32_t pclk_hz, size_t bounce_buffer_size_px);

  bool begin(int32_t speed = 0) override;

  // The only one Arduino_Canvas::flush() ever calls on this object.
  void draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) override;

  // Required by Arduino_G, unreachable in this board's draw path (the game
  // only ever draws into the Arduino_Canvas wrapping this object -- see
  // TamaPoke.ino). Implemented for correctness, not performance.
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) override;
  void drawIndexedBitmap(int16_t x, int16_t y, uint8_t *bitmap, uint16_t *color_index, int16_t w, int16_t h, int16_t x_skip = 0) override;
  void draw3bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) override;
  void draw24bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) override;

  uint16_t *getFramebuffer() { return _framebuffer; }

private:
  int8_t _de, _vsync, _hsync, _pclk;
  int8_t _r0, _r1, _r2, _r3, _r4;
  int8_t _g0, _g1, _g2, _g3, _g4, _g5;
  int8_t _b0, _b1, _b2, _b3, _b4;
  uint16_t _hsync_polarity, _hsync_front_porch, _hsync_pulse_width, _hsync_back_porch;
  uint16_t _vsync_polarity, _vsync_front_porch, _vsync_pulse_width, _vsync_back_porch;
  uint16_t _pclk_active_neg;
  int32_t _pclk_hz;
  size_t _bounce_buffer_size_px;

  esp_lcd_panel_handle_t _panel = nullptr;
  uint16_t *_framebuffer = nullptr;
};
