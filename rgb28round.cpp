#include "rgb28round.h"

TamaRgbPanel::TamaRgbPanel(
    int16_t w, int16_t h,
    int8_t de, int8_t vsync, int8_t hsync, int8_t pclk,
    int8_t r0, int8_t r1, int8_t r2, int8_t r3, int8_t r4,
    int8_t g0, int8_t g1, int8_t g2, int8_t g3, int8_t g4, int8_t g5,
    int8_t b0, int8_t b1, int8_t b2, int8_t b3, int8_t b4,
    uint16_t hsync_polarity, uint16_t hsync_front_porch, uint16_t hsync_pulse_width, uint16_t hsync_back_porch,
    uint16_t vsync_polarity, uint16_t vsync_front_porch, uint16_t vsync_pulse_width, uint16_t vsync_back_porch,
    uint16_t pclk_active_neg, int32_t pclk_hz, size_t bounce_buffer_size_px)
    : Arduino_G(w, h),
      _de(de), _vsync(vsync), _hsync(hsync), _pclk(pclk),
      _r0(r0), _r1(r1), _r2(r2), _r3(r3), _r4(r4),
      _g0(g0), _g1(g1), _g2(g2), _g3(g3), _g4(g4), _g5(g5),
      _b0(b0), _b1(b1), _b2(b2), _b3(b3), _b4(b4),
      _hsync_polarity(hsync_polarity), _hsync_front_porch(hsync_front_porch),
      _hsync_pulse_width(hsync_pulse_width), _hsync_back_porch(hsync_back_porch),
      _vsync_polarity(vsync_polarity), _vsync_front_porch(vsync_front_porch),
      _vsync_pulse_width(vsync_pulse_width), _vsync_back_porch(vsync_back_porch),
      _pclk_active_neg(pclk_active_neg), _pclk_hz(pclk_hz),
      _bounce_buffer_size_px(bounce_buffer_size_px) {
}

bool TamaRgbPanel::begin(int32_t speed) {
  esp_lcd_rgb_panel_config_t cfg = {};
  // The one field this whole class exists for: PLL240M, matching Waveshare's
  // own official demo for this panel (Arduino_ESP32RGBPanel hardcodes
  // LCD_CLK_SRC_DEFAULT/PLL160M instead, not overridable from outside it).
  cfg.clk_src = LCD_CLK_SRC_PLL240M;
  cfg.timings.pclk_hz = (uint32_t)_pclk_hz;
  cfg.timings.h_res = (uint32_t)WIDTH;
  cfg.timings.v_res = (uint32_t)HEIGHT;
  cfg.timings.hsync_pulse_width = _hsync_pulse_width;
  cfg.timings.hsync_back_porch = _hsync_back_porch;
  cfg.timings.hsync_front_porch = _hsync_front_porch;
  cfg.timings.vsync_pulse_width = _vsync_pulse_width;
  cfg.timings.vsync_back_porch = _vsync_back_porch;
  cfg.timings.vsync_front_porch = _vsync_front_porch;
  cfg.timings.flags.hsync_idle_low = (_hsync_polarity == 0) ? 1 : 0;
  cfg.timings.flags.vsync_idle_low = (_vsync_polarity == 0) ? 1 : 0;
  cfg.timings.flags.pclk_active_neg = _pclk_active_neg;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  cfg.bounce_buffer_size_px = _bounce_buffer_size_px;
  cfg.sram_trans_align = 8;
  cfg.psram_trans_align = 64;
  cfg.hsync_gpio_num = _hsync;
  cfg.vsync_gpio_num = _vsync;
  cfg.de_gpio_num = _de;
  cfg.pclk_gpio_num = _pclk;
  cfg.disp_gpio_num = GPIO_NUM_NC;
  // b0..b4, g0..g5, r0..r4 -- confirmed against Arduino_ESP32RGBPanel's own
  // useBigEndian=false ordering (read from its .cpp, not assumed) AND
  // against Waveshare's official Display_ST7701.h DATA0..DATA15 pin list,
  // which lines up with this exact order.
  cfg.data_gpio_nums[0] = _b0;
  cfg.data_gpio_nums[1] = _b1;
  cfg.data_gpio_nums[2] = _b2;
  cfg.data_gpio_nums[3] = _b3;
  cfg.data_gpio_nums[4] = _b4;
  cfg.data_gpio_nums[5] = _g0;
  cfg.data_gpio_nums[6] = _g1;
  cfg.data_gpio_nums[7] = _g2;
  cfg.data_gpio_nums[8] = _g3;
  cfg.data_gpio_nums[9] = _g4;
  cfg.data_gpio_nums[10] = _g5;
  cfg.data_gpio_nums[11] = _r0;
  cfg.data_gpio_nums[12] = _r1;
  cfg.data_gpio_nums[13] = _r2;
  cfg.data_gpio_nums[14] = _r3;
  cfg.data_gpio_nums[15] = _r4;
  cfg.flags.fb_in_psram = true;

  esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &_panel);
  if (err != ESP_OK) {
    Serial.printf("TamaRgbPanel: esp_lcd_new_rgb_panel fallo (%s)\n", esp_err_to_name(err));
    return false;
  }
  err = esp_lcd_panel_reset(_panel);
  if (err != ESP_OK) {
    Serial.printf("TamaRgbPanel: esp_lcd_panel_reset fallo (%s)\n", esp_err_to_name(err));
    return false;
  }
  err = esp_lcd_panel_init(_panel);
  if (err != ESP_OK) {
    Serial.printf("TamaRgbPanel: esp_lcd_panel_init fallo (%s)\n", esp_err_to_name(err));
    return false;
  }
  void *fb = nullptr;
  err = esp_lcd_rgb_panel_get_frame_buffer(_panel, 1, &fb);
  if (err != ESP_OK || !fb) {
    Serial.printf("TamaRgbPanel: get_frame_buffer fallo (%s)\n", esp_err_to_name(err));
    return false;
  }
  _framebuffer = (uint16_t *)fb;
  return true;
}

void TamaRgbPanel::draw16bitRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
  if (!_framebuffer) return;
  // Arduino_Canvas::flush() always calls this with (0, 0, WIDTH, HEIGHT) --
  // the whole composed frame at once, since this board's gfx is a Canvas
  // wrapping this object and nothing else ever writes through it. That
  // common case is one contiguous copy; anything else (never exercised on
  // this board, kept for correctness) falls back to a row loop.
  if (x == 0 && y == 0 && w == WIDTH && h == HEIGHT) {
    memcpy(_framebuffer, bitmap, (size_t)w * h * 2);
    return;
  }
  if (x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT) return;
  for (int16_t row = 0; row < h; row++) {
    memcpy(_framebuffer + (size_t)(y + row) * WIDTH + x,
           bitmap + (size_t)row * w, (size_t)w * 2);
  }
}

// Everything below is required by Arduino_G but unreachable on this board:
// the game draws only into the Arduino_Canvas wrapping this object (see
// TamaPoke.ino), so Canvas::flush()'s draw16bitRGBBitmap() above is the only
// entry point this class ever actually sees. Implemented plainly rather than
// left as stubs, so a future direct use is merely slow, not silently wrong.

void TamaRgbPanel::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  if (!_framebuffer) return;
  int16_t byteWidth = (w + 7) / 8;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      int16_t px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= WIDTH || py >= HEIGHT) continue;
      bool set = bitmap[j * byteWidth + i / 8] & (0x80 >> (i & 7));
      _framebuffer[(size_t)py * WIDTH + px] = set ? color : bg;
    }
  }
}

void TamaRgbPanel::drawIndexedBitmap(int16_t x, int16_t y, uint8_t *bitmap, uint16_t *color_index, int16_t w, int16_t h, int16_t x_skip) {
  if (!_framebuffer) return;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      int16_t px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= WIDTH || py >= HEIGHT) continue;
      _framebuffer[(size_t)py * WIDTH + px] = color_index[bitmap[j * (w + x_skip) + i]];
    }
  }
}

void TamaRgbPanel::draw3bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) {
  if (!_framebuffer) return;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      int16_t px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= WIDTH || py >= HEIGHT) continue;
      uint8_t v = bitmap[j * w + i];
      uint8_t r = (v >> 5) & 0x07, g = (v >> 2) & 0x07, b = v & 0x03;
      uint16_t c = ((r * 31 / 7) << 11) | ((g * 63 / 7) << 5) | (b * 31 / 3);
      _framebuffer[(size_t)py * WIDTH + px] = c;
    }
  }
}

void TamaRgbPanel::draw24bitRGBBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) {
  if (!_framebuffer) return;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      int16_t px = x + i, py = y + j;
      if (px < 0 || py < 0 || px >= WIDTH || py >= HEIGHT) continue;
      const uint8_t *p = bitmap + ((size_t)j * w + i) * 3;
      uint16_t c = ((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3);
      _framebuffer[(size_t)py * WIDTH + px] = c;
    }
  }
}
