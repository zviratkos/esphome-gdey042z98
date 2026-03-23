#include "gdey042z98.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace gdey042z98 {

static const char *const TAG = "gdey042z98";

// ---------------------------------------------------------------------------
// SPI pomocné funkce
// ---------------------------------------------------------------------------

void GDEY042Z98::send_command_(uint8_t cmd) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->transfer_byte(cmd);
  this->disable();
}

void GDEY042Z98::send_data_(uint8_t data) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->transfer_byte(data);
  this->disable();
}

void GDEY042Z98::hw_reset_() {
  if (this->reset_pin_ == nullptr) return;
  this->reset_pin_->digital_write(true);
  delay(10);
  this->reset_pin_->digital_write(false);
  delay(10);
  this->reset_pin_->digital_write(true);
  delay(10);
}

// ---------------------------------------------------------------------------
// Napájení displeje
// ---------------------------------------------------------------------------

void GDEY042Z98::power_on_() {
  if (this->power_pin_ == nullptr) return;
  this->power_pin_->digital_write(true);
  delay(100);
  ESP_LOGD(TAG, "Napájení displeje: ON");
}

void GDEY042Z98::power_off_() {
  if (this->power_pin_ == nullptr) return;
  this->power_pin_->digital_write(false);
  ESP_LOGD(TAG, "Napájení displeje: OFF");
}

// ---------------------------------------------------------------------------
// set_partial_ram_area_ – nastavuje data entry mode, oblast RAM i ukazatel
// ---------------------------------------------------------------------------

void GDEY042Z98::set_partial_ram_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  this->send_command_(0x11);
  this->send_data_(0x03);
  this->send_command_(0x44);
  this->send_data_(x / 8);
  this->send_data_((x + w - 1) / 8);
  this->send_command_(0x45);
  this->send_data_(y % 256);
  this->send_data_(y / 256);
  this->send_data_((y + h - 1) % 256);
  this->send_data_((y + h - 1) / 256);
  this->send_command_(0x4E);
  this->send_data_(x / 8);
  this->send_command_(0x4F);
  this->send_data_(y % 256);
  this->send_data_(y / 256);
}

// ---------------------------------------------------------------------------
// Inicializace displeje – dle GxEPD2 _InitDisplay pro GDEY042Z98
// ---------------------------------------------------------------------------

void GDEY042Z98::initialize_display_() {
  this->hw_reset_();
  this->send_command_(0x12);  // SW reset
  delay(10);
  this->send_command_(0x01);  // driver output control
  this->send_data_((GDEY042Z98_HEIGHT - 1) % 256);
  this->send_data_((GDEY042Z98_HEIGHT - 1) / 256);
  this->send_data_(0x00);
  this->send_command_(0x3C);  // border waveform
  this->send_data_(0x05);
  this->send_command_(0x18);  // interní teplotní senzor
  this->send_data_(0x80);
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------

void GDEY042Z98::setup() {
  this->spi_setup();
  this->dc_pin_->setup();
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr)
    this->busy_pin_->setup();
  if (this->power_pin_ != nullptr)
    this->power_pin_->setup();

  memset(this->bw_buffer_,  0xFF, BUFFER_SIZE);
  memset(this->red_buffer_, 0xFF, BUFFER_SIZE);

  this->power_on_();
  this->initialize_display_();
  this->power_off_();

  ESP_LOGD(TAG, "GDEY042Z98 inicializován");
}

void GDEY042Z98::dump_config() {
  LOG_DISPLAY("", "GDEY042Z98 (4.2\" B/W/R e-ink)", this);
  ESP_LOGCONFIG(TAG, "  Rozlišení: %dx%d", GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  LOG_PIN("  DC Pin: ",    this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ",  this->busy_pin_);
  LOG_PIN("  Power Pin: ", this->power_pin_);
}

// ---------------------------------------------------------------------------
// Kreslení pixelů do framebufferu
// ---------------------------------------------------------------------------

void GDEY042Z98::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= GDEY042Z98_WIDTH || y < 0 || y >= GDEY042Z98_HEIGHT)
    return;

  size_t byte_idx = (y * (GDEY042Z98_WIDTH / 8)) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x % 8);

  bool is_red   = color.r > 127 && color.g < 127 && color.b < 127;
  bool is_black = color.r < 127 && color.g < 127 && color.b < 127;

  if (is_red) {
    this->bw_buffer_[byte_idx]  |= bit_mask;
    this->red_buffer_[byte_idx] &= ~bit_mask;
  } else if (is_black) {
    this->bw_buffer_[byte_idx]  &= ~bit_mask;
    this->red_buffer_[byte_idx] |= bit_mask;
  } else {
    this->bw_buffer_[byte_idx]  |= bit_mask;
    this->red_buffer_[byte_idx] |= bit_mask;
  }
}

// ---------------------------------------------------------------------------
// update() – full refresh (B/W + červená), neblokuje
// ---------------------------------------------------------------------------

void GDEY042Z98::update() {
  if (this->refresh_state_ != RefreshState::IDLE) {
    ESP_LOGW(TAG, "Refresh stále probíhá, přeskakuji update");
    return;
  }

  this->power_on_();
  this->initialize_display_();
  this->do_update_();  // zavolá lambda writer

  // B/W buffer – celá plocha
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  this->send_command_(0x24);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (size_t i = 0; i < BUFFER_SIZE; i++)
    this->transfer_byte(this->bw_buffer_[i]);
  this->disable();

  // Červený buffer (invertovaný) – celá plocha
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  this->send_command_(0x26);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (size_t i = 0; i < BUFFER_SIZE; i++)
    this->transfer_byte(~this->red_buffer_[i]);
  this->disable();

  // Full refresh
  this->send_command_(0x22);
  this->send_data_(0xF7);
  this->send_command_(0x20);

  this->current_refresh_is_partial_ = false;
  this->refresh_state_ = RefreshState::WAITING;
  this->busy_start_ms_ = millis();
  ESP_LOGD(TAG, "Full refresh spuštěn...");
}

// ---------------------------------------------------------------------------
// partial_update() – překreslí jen zadanou oblast, pouze B/W (~1s)
// ---------------------------------------------------------------------------

void GDEY042Z98::partial_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (this->refresh_state_ != RefreshState::IDLE) {
    ESP_LOGW(TAG, "Refresh stále probíhá, přeskakuji partial update");
    return;
  }

  // Zarovnat x a w na byte boundary (násobek 8)
  x = x & 0xFFF8;
  w = (w + 7) & 0xFFF8;

  // Oříznout na rozměry displeje
  if (x + w > GDEY042Z98_WIDTH)  w = GDEY042Z98_WIDTH - x;
  if (y + h > GDEY042Z98_HEIGHT) h = GDEY042Z98_HEIGHT - y;

  ESP_LOGD(TAG, "Partial update oblast: x=%d y=%d w=%d h=%d", x, y, w, h);

  this->power_on_();
  this->initialize_display_();
  this->do_update_();  // překresli celý framebuffer (lambda), pak pošleme jen výřez

  // Poslat pouze výřez B/W bufferu
  this->set_partial_ram_area_(x, y, w, h);
  this->send_command_(0x24);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (uint16_t row = y; row < y + h; row++) {
    for (uint16_t col = x; col < x + w; col += 8) {
      size_t byte_idx = (row * (GDEY042Z98_WIDTH / 8)) + (col / 8);
      this->transfer_byte(this->bw_buffer_[byte_idx]);
    }
  }
  this->disable();

  // Partial refresh – rychlý, jen B/W
  this->send_command_(0x22);
  this->send_data_(0xC7);  // 0xC7 = partial/fast refresh (bez červené LUT)
  this->send_command_(0x20);

  this->current_refresh_is_partial_ = true;
  this->refresh_state_ = RefreshState::WAITING;
  this->busy_start_ms_ = millis();
  ESP_LOGD(TAG, "Partial refresh spuštěn...");
}

// ---------------------------------------------------------------------------
// loop() – neblokující čekání na dokončení refreshe
// ---------------------------------------------------------------------------

void GDEY042Z98::loop() {
  if (this->refresh_state_ != RefreshState::WAITING)
    return;

  // Timeout – partial max 5s, full max 30s
  uint32_t timeout = this->current_refresh_is_partial_ ? 5000 : 30000;

  if (this->busy_pin_ == nullptr) {
    if (millis() - this->busy_start_ms_ > timeout) {
      this->send_command_(0x10);
      this->send_data_(0x11);
      this->power_off_();
      this->refresh_state_ = RefreshState::IDLE;
      ESP_LOGD(TAG, "Refresh dokončen (bez BUSY pinu)");
    }
    return;
  }

  if (this->busy_pin_->digital_read()) {
    if (millis() - this->busy_start_ms_ > timeout) {
      ESP_LOGE(TAG, "Timeout čekání na BUSY");
      this->refresh_state_ = RefreshState::IDLE;
      this->power_off_();
    }
    return;
  }

  // BUSY LOW – hotovo
  uint32_t elapsed = millis() - this->busy_start_ms_;
  ESP_LOGD(TAG, "%s refresh dokončen za %lu ms",
    this->current_refresh_is_partial_ ? "Partial" : "Full", elapsed);

  this->send_command_(0x10);  // deep sleep
  this->send_data_(0x11);
  this->power_off_();
  this->refresh_state_ = RefreshState::IDLE;
}

}  // namespace gdey042z98
}  // namespace esphome
