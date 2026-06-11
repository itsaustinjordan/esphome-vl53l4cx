#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <Wire.h>
#include <vl53l4cx_class.h>
#include <vector>

namespace esphome {
namespace vl53l4cx {

class VL53L4CXComponent : public PollingComponent {
 public:
  VL53L4CXComponent();

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_distance_sensor(sensor::Sensor *s) { this->distance_sensor_ = s; }
  void set_object_count_sensor(sensor::Sensor *s) { this->object_count_sensor_ = s; }
  void set_sda_pin(int pin) { this->sda_pin_ = pin; }
  void set_scl_pin(int pin) { this->scl_pin_ = pin; }
  void set_frequency(uint32_t hz) { this->frequency_ = hz; }
  void set_i2c_address(uint8_t address) { this->address_ = address; }
  void set_xshut_pin(int pin) { this->xshut_pin_ = pin; }
  void set_distance_mode(uint8_t mode) { this->distance_mode_ = mode; }
  void set_timing_budget(uint32_t budget_us) { this->timing_budget_us_ = budget_us; }

  // ---- Runtime controls (callable from YAML lambdas / HA UI) ----
  // Safe to call at any time: before setup() they just stage the value,
  // after setup() they pause ranging, apply, and resume.
  void apply_distance_mode(uint8_t mode);
  void apply_timing_budget_ms(uint32_t budget_ms);
  void apply_update_interval_ms(uint32_t interval_ms);

 protected:
  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *object_count_sensor_{nullptr};

  int sda_pin_{21};
  int scl_pin_{22};
  uint32_t frequency_{100000};
  uint8_t address_{0x29};      // 7-bit I2C address (ESPHome convention)
  int xshut_pin_{-1};          // -1 = XSHUT not wired
  uint8_t distance_mode_{3};   // 1=short, 2=medium, 3=long
  uint32_t timing_budget_us_{50000};

  bool ranging_{false};        // true once setup() finished successfully
  uint32_t last_data_ms_{0};   // for the stalled-ranging watchdog

  // CRITICAL: the ST driver object is heap-allocated with `new` in setup().
  // Embedding it by value (or constructing it before setup) is exactly what
  // broke every previous attempt on ESP32 -- ESPHome's setup() and loop()
  // can run on different FreeRTOS tasks, and the driver's internal state
  // must live in heap memory shared between them. See esphome/issues#3869.
  VL53L4CX *tof_{nullptr};

  // Multi-sensor coordination. All instances register themselves so the
  // first setup() can hold every sensor in reset (they all boot at 0x29)
  // before assigning unique addresses one at a time.
  static std::vector<VL53L4CXComponent *> instances_;
  static bool bus_started_;
  static int bus_sda_;
  static int bus_scl_;
};

}  // namespace vl53l4cx
}  // namespace esphome

#endif  // USE_ARDUINO
