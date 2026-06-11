#include "vl53l4cx_sensor.h"

#ifdef USE_ARDUINO

#include "esphome/core/log.h"

#include <Arduino.h>
#include <cmath>
#include <cstdint>

namespace esphome {
namespace vl53l4cx {

static const char *const TAG = "vl53l4cx";
static const char *const COMPONENT_VERSION = "1.1.0";

std::vector<VL53L4CXComponent *> VL53L4CXComponent::instances_;
bool VL53L4CXComponent::bus_started_ = false;
int VL53L4CXComponent::bus_sda_ = -1;
int VL53L4CXComponent::bus_scl_ = -1;

VL53L4CXComponent::VL53L4CXComponent() { instances_.push_back(this); }

void VL53L4CXComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up VL53L4CX (component v%s)...", COMPONENT_VERSION);

  // --- One-time bus bring-up (first instance only) -------------------------
  // This component owns the I2C bus through the Arduino Wire library.
  // (ESPHome's own `i2c:` component is IDF-native in modern hybrid builds
  // and must not be configured on the same pins.)
  if (!bus_started_) {
    if (!Wire.begin(this->sda_pin_, this->scl_pin_, this->frequency_)) {
      ESP_LOGE(TAG, "Wire.begin() failed on SDA=GPIO%d SCL=GPIO%d", this->sda_pin_, this->scl_pin_);
      this->mark_failed();
      return;
    }
    bus_started_ = true;
    bus_sda_ = this->sda_pin_;
    bus_scl_ = this->scl_pin_;

    // Hold EVERY sensor with an XSHUT pin in reset. All VL53L4CX chips boot
    // at address 0x29, so with multiple sensors they must be woken and
    // re-addressed one at a time, each in its own setup() below.
    for (auto *inst : instances_) {
      if (inst->xshut_pin_ >= 0) {
        pinMode(inst->xshut_pin_, OUTPUT);
        digitalWrite(inst->xshut_pin_, LOW);
      }
    }
    delay(10);
  } else if (this->sda_pin_ != bus_sda_ || this->scl_pin_ != bus_scl_) {
    ESP_LOGE(TAG, "All vl53l4cx sensors must share one bus; it is already on SDA=GPIO%d SCL=GPIO%d",
             bus_sda_, bus_scl_);
    this->mark_failed();
    return;
  }

  // --- Multi-sensor sanity checks ------------------------------------------
  if (instances_.size() > 1) {
    if (this->xshut_pin_ < 0) {
      ESP_LOGE(TAG, "Multiple VL53L4CX sensors: every sensor needs its own xshut_pin");
      this->mark_failed();
      return;
    }
    for (auto *other : instances_) {
      if (other != this && other->address_ == this->address_) {
        ESP_LOGE(TAG, "Duplicate address 0x%02X -- give each sensor a unique address (0x29, 0x30, 0x31, ...)",
                 this->address_);
        this->mark_failed();
        return;
      }
    }
  }

  // --- Per-sensor init: ST's proven HelloWorld sequence --------------------
  //   begin() -> Off() -> On() -> probe -> InitSensor(addr) -> Start
  // Heap-allocate the driver (see note in the header).
  this->tof_ = new VL53L4CX(&Wire, this->xshut_pin_);
  this->tof_->begin();
  this->tof_->VL53L4CX_Off();  // hardware reset when XSHUT is wired, no-op otherwise
  delay(5);
  this->tof_->VL53L4CX_On();   // wake THIS sensor only; others stay in reset
  delay(10);

  // The chip always wakes at 0x29; probe there so wiring problems produce a
  // clear message instead of a cryptic driver status code.
  Wire.beginTransmission(0x29);
  uint8_t wire_err = Wire.endTransmission();
  if (wire_err != 0) {
    ESP_LOGE(TAG, "No ACK at boot address 0x29 (Wire error %u). Check SDA=GPIO%d / SCL=GPIO%d / XSHUT wiring.",
             (unsigned) wire_err, this->sda_pin_, this->scl_pin_);
    this->mark_failed();
    return;
  }

  // The ST driver uses 8-bit I2C addresses internally; ESPHome uses 7-bit.
  // InitSensor() also moves the chip from 0x29 to the configured address.
  VL53L4CX_Error status = this->tof_->InitSensor((uint8_t) (this->address_ << 1));
  if (status != 0) {
    ESP_LOGE(TAG, "InitSensor() failed, status %d.", (int) status);
    this->mark_failed();
    return;
  }

  status = this->tof_->VL53L4CX_SetDistanceMode((VL53L4CX_DistanceModes) this->distance_mode_);
  if (status != 0)
    ESP_LOGW(TAG, "SetDistanceMode() failed, status %d (continuing with default)", (int) status);

  status = this->tof_->VL53L4CX_SetMeasurementTimingBudgetMicroSeconds(this->timing_budget_us_);
  if (status != 0)
    ESP_LOGW(TAG, "SetTimingBudget() failed, status %d (continuing with default)", (int) status);

  status = this->tof_->VL53L4CX_StartMeasurement();
  if (status != 0) {
    ESP_LOGE(TAG, "StartMeasurement() failed, status %d", (int) status);
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "VL53L4CX at 0x%02X is ranging", this->address_);
}

void VL53L4CXComponent::update() {
  if (this->is_failed() || this->tof_ == nullptr)
    return;

  // Non-blocking: if the current measurement isn't finished yet we simply
  // skip this cycle instead of spin-waiting.
  uint8_t data_ready = 0;
  VL53L4CX_Error status = this->tof_->VL53L4CX_GetMeasurementDataReady(&data_ready);
  if (status != 0) {
    ESP_LOGW(TAG, "GetMeasurementDataReady() failed, status %d", (int) status);
    this->status_set_warning();
    return;
  }

  if (!data_ready) {
    if (++this->not_ready_count_ >= 5) {
      // Watchdog for a stalled ranging engine: kick it back into gear.
      ESP_LOGW(TAG, "No data for %u cycles; restarting measurement", (unsigned) this->not_ready_count_);
      this->tof_->VL53L4CX_ClearInterruptAndStartMeasurement();
      this->not_ready_count_ = 0;
    }
    return;
  }
  this->not_ready_count_ = 0;

  VL53L4CX_MultiRangingData_t data;
  status = this->tof_->VL53L4CX_GetMultiRangingData(&data);
  if (status == 0) {
    int n_found = data.NumberOfObjectsFound;
    int max_targets = (int) (sizeof(data.RangeData) / sizeof(data.RangeData[0]));
    if (n_found > max_targets)
      n_found = max_targets;

    int valid = 0;
    int32_t nearest_mm = INT32_MAX;
    for (int i = 0; i < n_found; i++) {
      const auto &r = data.RangeData[i];
      ESP_LOGV(TAG, "  target %d: status=%d distance=%dmm", i, (int) r.RangeStatus, (int) r.RangeMilliMeter);
      // RangeStatus 0 == valid measurement
      if (r.RangeStatus == 0 && r.RangeMilliMeter >= 0) {
        valid++;
        if (r.RangeMilliMeter < nearest_mm)
          nearest_mm = r.RangeMilliMeter;
      }
    }

    if (this->distance_sensor_ != nullptr) {
      if (valid > 0) {
        this->distance_sensor_->publish_state((float) nearest_mm);
      } else {
        this->distance_sensor_->publish_state(NAN);  // nothing valid in view
      }
    }
    if (this->object_count_sensor_ != nullptr)
      this->object_count_sensor_->publish_state((float) valid);

    this->status_clear_warning();
  } else {
    ESP_LOGW(TAG, "GetMultiRangingData() failed, status %d", (int) status);
    this->status_set_warning();
  }

  // Mandatory after every read: clears the interrupt and arms the next
  // measurement. Without it, data-ready never fires again.
  this->tof_->VL53L4CX_ClearInterruptAndStartMeasurement();
}

void VL53L4CXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "VL53L4CX (v%s):", COMPONENT_VERSION);
  ESP_LOGCONFIG(TAG, "  SDA: GPIO%d, SCL: GPIO%d @ %u Hz", this->sda_pin_, this->scl_pin_,
                (unsigned) this->frequency_);
  ESP_LOGCONFIG(TAG, "  I2C address: 0x%02X", this->address_);
  if (this->xshut_pin_ >= 0) {
    ESP_LOGCONFIG(TAG, "  XSHUT pin: GPIO%d", this->xshut_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  XSHUT pin: not connected");
  }
  ESP_LOGCONFIG(TAG, "  Distance mode: %u (1=short 2=medium 3=long)", this->distance_mode_);
  ESP_LOGCONFIG(TAG, "  Timing budget: %u us", (unsigned) this->timing_budget_us_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Distance", this->distance_sensor_);
  LOG_SENSOR("  ", "Object count", this->object_count_sensor_);
  if (this->is_failed())
    ESP_LOGE(TAG, "  Communication with VL53L4CX FAILED!");
}

}  // namespace vl53l4cx
}  // namespace esphome

#endif  // USE_ARDUINO
