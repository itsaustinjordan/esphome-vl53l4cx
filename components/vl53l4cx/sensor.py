"""ESPHome sensor platform for the ST VL53L4CX Time-of-Flight sensor.

Wraps the official stm32duino "STM32duino VL53L4CX" Arduino library, pulled in
automatically at build time. Requires the Arduino framework on ESP32 (ESPHome
2025+ hybrid Arduino-on-IDF builds are supported).

This component owns its own I2C bus through the Arduino Wire library (modern
ESPHome's `i2c:` component is ESP-IDF-native and would conflict on the same
pins -- do NOT add an `i2c:` block for these sensors).

Multiple sensors on one board are supported: give every sensor its own
xshut_pin and a unique address (e.g. 0x29, 0x30, 0x31) on the same
sda_pin/scl_pin bus.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_FREQUENCY,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    PLATFORM_ESP32,
)

CODEOWNERS = ["@local"]

CONF_DISTANCE = "distance"
CONF_OBJECT_COUNT = "object_count"
CONF_XSHUT_PIN = "xshut_pin"
CONF_SDA_PIN = "sda_pin"
CONF_SCL_PIN = "scl_pin"
CONF_DISTANCE_MODE = "distance_mode"
CONF_TIMING_BUDGET = "timing_budget"

vl53l4cx_ns = cg.esphome_ns.namespace("vl53l4cx")
VL53L4CXComponent = vl53l4cx_ns.class_("VL53L4CXComponent", cg.PollingComponent)

# Values map to VL53L4CX_DISTANCEMODE_SHORT / _MEDIUM / _LONG
DISTANCE_MODES = {
    "short": 1,
    "medium": 2,
    "long": 3,
}


def validate_timing_budget(value):
    value = cv.positive_time_period_microseconds(value)
    if value.total_microseconds < 20000 or value.total_microseconds > 500000:
        raise cv.Invalid("timing_budget must be between 20ms and 500ms")
    return value


def validate_address_needs_xshut(config):
    if config[CONF_ADDRESS] != 0x29 and CONF_XSHUT_PIN not in config:
        raise cv.Invalid(
            "An address other than 0x29 requires xshut_pin: the sensor always "
            "boots at 0x29 and must be hardware-reset before it can be moved "
            "to a new address."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VL53L4CXComponent),
            cv.Required(CONF_DISTANCE): sensor.sensor_schema(
                unit_of_measurement="mm",
                icon="mdi:arrow-expand-vertical",
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OBJECT_COUNT): sensor.sensor_schema(
                icon="mdi:counter",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SDA_PIN, default=21): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_SCL_PIN, default=22): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_FREQUENCY, default="100kHz"): cv.All(
                cv.frequency, cv.Range(min=10000, max=1000000)
            ),
            cv.Optional(CONF_ADDRESS, default=0x29): cv.i2c_address,
            cv.Optional(CONF_XSHUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DISTANCE_MODE, default="long"): cv.enum(
                DISTANCE_MODES, lower=True
            ),
            cv.Optional(CONF_TIMING_BUDGET, default="50ms"): validate_timing_budget,
        }
    ).extend(cv.polling_component_schema("1s")),
    validate_address_needs_xshut,
    cv.only_with_arduino,
    cv.only_on([PLATFORM_ESP32]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    dist = await sensor.new_sensor(config[CONF_DISTANCE])
    cg.add(var.set_distance_sensor(dist))

    if CONF_OBJECT_COUNT in config:
        objs = await sensor.new_sensor(config[CONF_OBJECT_COUNT])
        cg.add(var.set_object_count_sensor(objs))

    cg.add(var.set_sda_pin(config[CONF_SDA_PIN]))
    cg.add(var.set_scl_pin(config[CONF_SCL_PIN]))
    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))
    if CONF_XSHUT_PIN in config:
        cg.add(var.set_xshut_pin(config[CONF_XSHUT_PIN]))
    cg.add(var.set_distance_mode(config[CONF_DISTANCE_MODE]))
    cg.add(var.set_timing_budget(config[CONF_TIMING_BUDGET]))

    # ESPHome 2025+ hybrid Arduino/IDF builds no longer link the Arduino Wire
    # library automatically, so request it explicitly. This also lets the ST
    # library's `#include <Wire.h>` resolve.
    cg.add_library("Wire", None)
    # ESPHome/PlatformIO downloads the official ST driver at compile time.
    cg.add_library("stm32duino/STM32duino VL53L4CX", "1.1.0")
