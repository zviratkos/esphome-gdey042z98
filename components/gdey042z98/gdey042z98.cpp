// Display Library for SPI e-paper panels from Good Display GDEY042Z98
// modified for LaskaKit https://www.laskakit.cz/laskakit-espink-esp32-e-paper-pcb-antenna/
// with powering on / off display for power saving
//
// inspired by https://github.com/ZinggJM/GxEPD2
// supported by claude.ai

#include "gdey042z98.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace gdey042z98 {

static const char *const TAG = "gdey042z98";

// ---------------------------------------------------------------------------
// SPI support functions
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
// PowerOn / PowerOff for LaskaKit
// ---------------------------------------------------------------------------

void GDEY042Z98::power_on_() {
  if (this->power_pin_ == nullptr) return;
  this->power_pin_->digital_write(true);
  delay(100);
  ESP_LOGD(TAG, "PowerOn display: ON");
}

void GDEY042Z98::power_off_() {
  if (this->power_pin_ == nullptr) return;
  this->power_pin_->digital_write(false);
  ESP_LOGD(TAG, "PowerOff display: OFF");
}

// ---------------------------------------------------------------------------
// set_partial_ram_area_ – set data entry mode, RAM and pointer
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
// Initialization of display – based on GxEPD2 _InitDisplay for GDEY042Z98
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
  this->send_command_(0x18);  // internal temp sensor
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
  this->power_off_();  // after initialization poweroff immediately, poweron just before refresh

  ESP_LOGD(TAG, "GDEY042Z98 initialized");
}

void GDEY042Z98::dump_config() {
  LOG_DISPLAY("", "GDEY042Z98 (4.2\" B/W/R e-ink)", this);
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  LOG_PIN("  DC Pin: ",    this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  Busy Pin: ",  this->busy_pin_);
  LOG_PIN("  Power Pin: ", this->power_pin_);
}

// ---------------------------------------------------------------------------
// Drawing pixels to the framebuffer
// ---------------------------------------------------------------------------

void GDEY042Z98::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= GDEY042Z98_WIDTH || y < 0 || y >= GDEY042Z98_HEIGHT)
    return;

  size_t byte_idx = (y * (GDEY042Z98_WIDTH / 8)) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x % 8);

  bool is_red   = color.r > 127 && color.g < 127 && color.b < 127;
  bool is_black = color.r < 127 && color.g < 127 && color.b < 127;

  if (is_red) {
    this->bw_buffer_[byte_idx]  |= bit_mask;   // white v B/W
    this->red_buffer_[byte_idx] &= ~bit_mask;  // 0 = red (before inversion)
  } else if (is_black) {
    this->bw_buffer_[byte_idx]  &= ~bit_mask;  // black
    this->red_buffer_[byte_idx] |= bit_mask;   // 1 = not red
  } else {
    this->bw_buffer_[byte_idx]  |= bit_mask;   // white
    this->red_buffer_[byte_idx] |= bit_mask;   // 1 = not red
  }
}

// ---------------------------------------------------------------------------
// update() – Data transfer and refresh start, non-blocking
// ---------------------------------------------------------------------------

void GDEY042Z98::update() {
  if (this->refresh_state_ != RefreshState::IDLE) {
    ESP_LOGW(TAG, "Refresh still in progress, skipping update");
    return;
  }

  this->power_on_();
  this->initialize_display_();
  this->do_update_();  // call lambda writer

  // B/W buffer
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  this->send_command_(0x24);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (size_t i = 0; i < BUFFER_SIZE; i++)
    this->transfer_byte(this->bw_buffer_[i]);
  this->disable();

  // Red buffer (inverted0
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
  this->send_command_(0x26);
  this->dc_pin_->digital_write(true);
  this->enable();
  for (size_t i = 0; i < BUFFER_SIZE; i++)
    this->transfer_byte(~this->red_buffer_[i]);
  this->disable();

  // Start refresh – non-blocking, BUSY waiting moved to loop()
  this->send_command_(0x22);
  this->send_data_(0xF7);
  this->send_command_(0x20);

  this->refresh_state_ = RefreshState::WAITING;
  this->busy_start_ms_ = millis();
  ESP_LOGD(TAG, "Refresh started, waiting for BUSY...");
}

// ---------------------------------------------------------------------------
// loop() – Non-blocking wait for refresh completion
// ---------------------------------------------------------------------------

void GDEY042Z98::loop() {
  if (this->refresh_state_ != RefreshState::WAITING)
    return;

  // No BUSY pin: waiting defined 30s
  if (this->busy_pin_ == nullptr) {
    if (millis() - this->busy_start_ms_ > 30000) {
      this->refresh_state_ = RefreshState::IDLE;
      this->send_command_(0x10);  // deep sleep
      this->send_data_(0x11);
      this->power_off_();
      ESP_LOGD(TAG, "Display updated (no BUSY pin)");
    }
    return;
  }

  // With BUSY pin: wating to LOW
  if (this->busy_pin_->digital_read()) {
    // Still busy
    if (millis() - this->busy_start_ms_ > 30000) {
      ESP_LOGE(TAG, "Timeout waiting for BUSY");
      this->refresh_state_ = RefreshState::IDLE;
      this->power_off_();
    }
    return;
  }

  // BUSY is LOW – refresh finished
  uint32_t elapsed = millis() - this->busy_start_ms_;
  ESP_LOGD(TAG, "Display updated in %lu ms", elapsed);

  this->send_command_(0x10);  // deep sleep
  this->send_data_(0x11);
  this->power_off_();
  this->refresh_state_ = RefreshState::IDLE;
}

}  // namespace gdey042z98
}  // namespace esphome
