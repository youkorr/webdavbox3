import esphome.config_validation as cv
from esphome import pins
from esphome.components import sdmmc
from esphome.const import (
    CONF_ID,
    CONF_CLK_PIN,
    CONF_CMD_PIN,
    CONF_DATA0_PIN,
    CONF_DATA1_PIN,
    CONF_DATA2_PIN,
    CONF_DATA3_PIN,
    CONF_MODE,
)

CONF_MODE_1BIT = "mode_1bit"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(sdmmc.SDMMCComponent),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_number,
        cv.Required(CONF_CMD_PIN): pins.gpio_output_pin_number,
        cv.Required(CONF_DATA0_PIN): pins.gpio_output_pin_number,
        cv.Optional(CONF_DATA1_PIN): pins.gpio_output_pin_number,
        cv.Optional(CONF_DATA2_PIN): pins.gpio_output_pin_number,
        cv.Optional(CONF_DATA3_PIN): pins.gpio_output_pin_number,
        cv.Optional(CONF_MODE_1BIT, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)




