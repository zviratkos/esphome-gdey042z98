# JS ESPHome custom components

### Components
- sencor_sacmt12 - Old air conditioner Sencor SAC MT12 IR remote control for climate entity in ESPHome
- touch_sensor - Capacitance sensor for measurement of water level using pair of simple copper tape
- gdey042z98 - Driver for LaskaKit GDEY042Z98 4.2 https://www.laskakit.cz/en/laskakit-espink-42-esp32-e-paper-pcb-antenna/

## gdey042z98

```
esp32:
  board: esp32-s3-devkitc-1          # modify based on your ESP board, for EspINK 4.2 this works
  framework:
    type: arduino

external_components:
  - source:
      type: git
      url: https://github.com/zviratkos/esphome-cc
      ref: main
    components: [gdey042z98]

# -----------------------------
# SPI bus - set pins based on ESPink available
#

spi:
  clk_pin: GPIO12
  miso_pin: GPIO21
  mosi_pin: GPIO11

# ---------------------------------------------------------------
# Colors for 3-color display
# ---------------------------------------------------------------
color:
  - id: color_black
    red: 0%
    green: 0%
    blue: 0%
  - id: color_red
    red: 100%
    green: 0%
    blue: 0%
  - id: color_white
    red: 100%
    green: 100%
    blue: 100%

# ---------------------------------------------------------------
# Font (download or use your own)
# ---------------------------------------------------------------
font:
  # Datum – střední, červeně
  - file: "gfonts://Roboto"
    id: font_date
    size: 22
  # Čas – velký, dominantní
  - file: "gfonts://Roboto@700"
    id: font_time
    size: 72
  # Hodnoty senzorů
  - file: "gfonts://Roboto@700"
    id: font_value
    size: 42
  # Popisky a jednotky
  - file: "gfonts://Roboto"
    id: font_label
    size: 20

# ---------------------------------------------------------------
# Display – modify CS, DC, RST, BUSY pins
# ---------------------------------------------------------------

display:
  - platform: gdey042z98
    id: my_display
    cs_pin: GPIO10         # Chip Select
    dc_pin: GPIO48         # Data/Command
    reset_pin: GPIO45      # Reset
    busy_pin: GPIO38       # Busy (HIGH = busy)
    power_pin: GPIO47      # power on/off PIN
    update_interval: 60s   # Update interval of diplay
    lambda: |-
      // White background
      it.fill(color_white);

      // ── Date ──────────────────────────────────────────────
      auto now = id(ha_time).now();
      if (now.is_valid()) {
        // Days
        const char* dny[] = {
          "Sunday", "Monday", "Tuesday", "Wednesday",
          "Thursday", "Friday", "Saturday"
        };
        // Months
        const char* mesice[] = {
          "", "January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"
        };

        it.printf(
          200, 10,
          id(font_date),
          color_red,
          TextAlign::TOP_CENTER,
          "%s %d. %s %d",
          dny[now.day_of_week],
          now.day_of_month,
          mesice[now.month],
          now.year
        );

        // ── Time ──────────────────────────────────────────────
        it.printf(
          200, 38,
          id(font_time),
          color_black,
          TextAlign::TOP_CENTER,
          "%02d:%02d",
          now.hour,
          now.minute
        );
      }

```

### License MIT
