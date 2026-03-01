#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"

namespace esphome {
namespace gdey042z98 {

// GDEY042Z98 – 4.2" 400×300 tříbarevný e-ink displej (B/W/R)
// Řadič: SSD1683, SPI 4-vodičový

static const uint16_t GDEY042Z98_WIDTH  = 400;
static const uint16_t GDEY042Z98_HEIGHT = 300;

enum class RefreshState {
  IDLE,         // klidový stav
  WAITING,      // čekáme až BUSY půjde LOW po refreshi
};

class GDEY042Z98 : public display::DisplayBuffer,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,
                                         spi::CLOCK_POLARITY_LOW,
                                         spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_dc_pin(GPIOPin *pin)    { dc_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { reset_pin_ = pin; }
  void set_busy_pin(GPIOPin *pin)  { busy_pin_ = pin; }
  void set_power_pin(GPIOPin *pin) { power_pin_ = pin; }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

  int get_width_internal()  override { return GDEY042Z98_WIDTH; }
  int get_height_internal() override { return GDEY042Z98_HEIGHT; }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;

  void initialize_display_();
  void send_command_(uint8_t cmd);
  void send_data_(uint8_t data);
  void hw_reset_();
  void power_on_();
  void power_off_();
  void set_partial_ram_area_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *power_pin_{nullptr};

  RefreshState refresh_state_{RefreshState::IDLE};
  uint32_t busy_start_ms_{0};

  static const size_t BUFFER_SIZE = (GDEY042Z98_WIDTH * GDEY042Z98_HEIGHT) / 8;
  uint8_t bw_buffer_[BUFFER_SIZE];
  uint8_t red_buffer_[BUFFER_SIZE];
};

}  // namespace gdey042z98
}  // namespace esphome
