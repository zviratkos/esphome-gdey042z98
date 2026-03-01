import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
CONF_POWER_PIN = "power_pin"
from esphome.const import (
    CONF_DC_PIN,
    CONF_RESET_PIN,
    CONF_BUSY_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_UPDATE_INTERVAL,
    CONF_PAGES,
)

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["display"]

gdey042z98_ns = cg.esphome_ns.namespace("gdey042z98")
GDEY042Z98 = gdey042z98_ns.class_(
    "GDEY042Z98",
    display.DisplayBuffer,
    spi.SPIDevice,
    cg.Component,
)

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GDEY042Z98),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_POWER_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))

    if CONF_BUSY_PIN in config:
        busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(busy))

    if CONF_POWER_PIN in config:
        power = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
        cg.add(var.set_power_pin(power))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.Display.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
