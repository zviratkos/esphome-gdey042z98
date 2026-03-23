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
// set_partial_ram_area_
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
// initialize_display_ – HW reset + SW reset + základní konfigurace
// Obsahuje delay(10) – volat pouze z POWERING_ON stavu v loop()
// kde už víme že napájení je stabilní
// ---------------------------------------------------------------------------

void GDEY042Z98::initialize_display_() {
  // Pouze HW reset + SW reset – zbytek konfigurace až po BUSY LOW v loop()
  this->hw_reset_();
  this->send_command_(0x12);  // SW reset – displej bude BUSY HIGH po dobu resetu
}

void GDEY042Z98::configure_display_() {
  // Konfigurace po SW resetu – volat až když BUSY LOW
  this->send_command_(0x01);
  this->send_data_((GDEY042Z98_HEIGHT - 1) % 256);
  this->send_data_((GDEY042Z98_HEIGHT - 1) / 256);
  this->send_data_(0x00);
  this->send_command_(0x3C);
  this->send_data_(0x05);
  this->send_command_(0x18);
  this->send_data_(0x80);
  this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
}

// ---------------------------------------------------------------------------
// do_send_() – přenos dat do RAM displeje a spuštění refreshe
// ---------------------------------------------------------------------------

void GDEY042Z98::do_send_() {
  if (this->refresh_type_ == RefreshType::FULL) {
    // B/W buffer – celá plocha
    this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
    this->send_command_(0x24);
    this->dc_pin_->digital_write(true);
    this->enable();
    for (size_t i = 0; i < BUFFER_SIZE; i++)
      this->transfer_byte(this->bw_buffer_[i]);
    this->disable();

    // Červený buffer (invertovaný)
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
    ESP_LOGD(TAG, "Full refresh spuštěn...");

  } else {
    // Partial – pouze výřez B/W bufferu
    uint16_t x = this->partial_x_;
    uint16_t y = this->partial_y_;
    uint16_t w = this->partial_w_;
    uint16_t h = this->partial_h_;

    // DIAGNOSTIKA: poslat celý B/W buffer (celá plocha) bez červené
    this->set_partial_ram_area_(0, 0, GDEY042Z98_WIDTH, GDEY042Z98_HEIGHT);
    this->send_command_(0x24);
    this->dc_pin_->digital_write(true);
    this->enable();
    for (size_t i = 0; i < BUFFER_SIZE; i++)
      this->transfer_byte(this->bw_buffer_[i]);
    this->disable();

    this->send_command_(0x22);
    this->send_data_(0xF7);
    this->send_command_(0x20);
    ESP_LOGD(TAG, "Partial refresh spuštěn (x=%d y=%d w=%d h=%d)...",
             x, y, w, h);
  }
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
  if (this->power_pin_ != nullptr) {
    this->power_pin_->setup();
    this->power_pin_->digital_write(false);
  }

  memset(this->bw_buffer_,  0xFF, BUFFER_SIZE);
  memset(this->red_buffer_, 0xFF, BUFFER_SIZE);

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
// update() – spustí full refresh, neblokuje
// ---------------------------------------------------------------------------

void GDEY042Z98::update() {
  if (this->refresh_state_ != RefreshState::IDLE) {
    ESP_LOGW(TAG, "Refresh stále probíhá, přeskakuji update");
    return;
  }
  this->do_update_();  // naplní framebuffer přes lambdu
  this->refresh_type_ = RefreshType::FULL;

  // Zapnout napájení – zbytek řeší loop()
  if (this->power_pin_ != nullptr)
    this->power_pin_->digital_write(true);

  this->refresh_state_ = RefreshState::POWERING_ON;
  this->state_start_ms_ = millis();
  ESP_LOGD(TAG, "Napájení ON, čekám na stabilizaci...");
}

// ---------------------------------------------------------------------------
// partial_update() – spustí partial refresh, neblokuje
// ---------------------------------------------------------------------------

void GDEY042Z98::partial_update(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (this->refresh_state_ != RefreshState::IDLE) {
    ESP_LOGW(TAG, "Refresh stále probíhá, přeskakuji partial update");
    return;
  }

  // Zarovnat na byte boundary
  x = x & 0xFFF8;
  w = (w + 7) & 0xFFF8;
  if (x + w > GDEY042Z98_WIDTH)  w = GDEY042Z98_WIDTH - x;
  if (y + h > GDEY042Z98_HEIGHT) h = GDEY042Z98_HEIGHT - y;

  this->do_update_();  // naplní framebuffer přes lambdu
  this->refresh_type_ = RefreshType::PARTIAL;
  this->partial_x_ = x;
  this->partial_y_ = y;
  this->partial_w_ = w;
  this->partial_h_ = h;

  // Zapnout napájení – zbytek řeší loop()
  if (this->power_pin_ != nullptr)
    this->power_pin_->digital_write(true);

  this->refresh_state_ = RefreshState::POWERING_ON;
  this->state_start_ms_ = millis();
}

// ---------------------------------------------------------------------------
// loop() – plně neblokující state machine
// ---------------------------------------------------------------------------

void GDEY042Z98::loop() {
  switch (this->refresh_state_) {

    case RefreshState::IDLE:
      return;

    case RefreshState::POWERING_ON:
      // Čekej 100ms na stabilizaci napájení – neblokuje!
      if (millis() - this->state_start_ms_ < 100)
        return;
      // Napájení stabilní – HW reset + SW reset, pak čekáme na BUSY LOW
      this->initialize_display_();
      this->refresh_state_ = RefreshState::INITIALIZING;
      this->state_start_ms_ = millis();
      return;

    case RefreshState::INITIALIZING:
      // Čekej až displej dokončí SW reset (BUSY LOW), max 1s
      if (this->busy_pin_ != nullptr && this->busy_pin_->digital_read()) {
        if (millis() - this->state_start_ms_ > 1000) {
          ESP_LOGW(TAG, "Timeout čekání na SW reset, pokračuji...");
        } else {
          return;  // stále zaneprázdněn po SW resetu
        }
      } else if (millis() - this->state_start_ms_ < 15) {
        return;  // bez BUSY pinu čekáme aspoň 15ms
      }
      // Displej připraven – dokonfiguruj a pošli data
      this->configure_display_();
      this->do_send_();
      this->refresh_state_ = RefreshState::WAITING;
      this->state_start_ms_ = millis();
      return;

    case RefreshState::WAITING: {
      uint32_t timeout = (this->refresh_type_ == RefreshType::PARTIAL) ? 15000 : 30000;

      if (this->busy_pin_ == nullptr) {
        if (millis() - this->state_start_ms_ > timeout) {
          this->send_command_(0x10);
          this->send_data_(0x11);
          if (this->power_pin_ != nullptr)
            this->power_pin_->digital_write(false);
          this->refresh_state_ = RefreshState::IDLE;
          ESP_LOGD(TAG, "Refresh dokončen (bez BUSY pinu)");
        }
        return;
      }

      if (this->busy_pin_->digital_read()) {
        // Stále zaneprázdněn
        if (millis() - this->state_start_ms_ > timeout) {
          ESP_LOGE(TAG, "Timeout čekání na BUSY");
          if (this->power_pin_ != nullptr)
            this->power_pin_->digital_write(false);
          this->refresh_state_ = RefreshState::IDLE;
        }
        return;
      }

      // BUSY LOW – hotovo
      uint32_t elapsed = millis() - this->state_start_ms_;
      ESP_LOGD(TAG, "%s refresh dokončen za %lu ms",
        this->refresh_type_ == RefreshType::PARTIAL ? "Partial" : "Full", elapsed);

      this->send_command_(0x10);  // deep sleep
      this->send_data_(0x11);
      if (this->power_pin_ != nullptr)
        this->power_pin_->digital_write(false);
      this->refresh_state_ = RefreshState::IDLE;
      return;
    }

    default:
      return;
  }
}

}  // namespace gdey042z98
}  // namespace esphome
