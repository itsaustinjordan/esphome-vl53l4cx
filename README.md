# ESPHome VL53L4CX — Laser Distance Sensors for Home Assistant

Turn a $15 laser Time-of-Flight sensor and a $7 ESP32 board into a finished
Home Assistant device: parking guides, mailbox monitors, desk/bed presence,
water-level gauges, people counting at a doorway.

A complete ESPHome integration for the **ST VL53L4CX** (the Adafruit #5425
breakout) — the long-range, multi-target member of ST's laser family,
previously unsupported in ESPHome. **Nothing to download:** your config
references this repo and ESPHome fetches everything at build time.

## What you get per sensor

| Entity | What it does |
|---|---|
| **Distance** | Nearest target in mm (~1 mm to ~6,000 mm). Reads `unknown` when nothing is in view. |
| **Presence** | On/off occupancy, driven by the threshold slider below. The "it just works" entity for automations. |
| **Presence Threshold** | Slider (mm). Closer than this = present. Tune it live from HA. |
| **Object Count** | How many separate targets the sensor sees — unique to this chip. |
| **Distance Mode** | Dropdown: short / medium / long range profile. Applies instantly, no reflash. |
| **Timing Budget** | Slider: accuracy vs. speed. Applies instantly. |
| **Update Interval** | Slider: how often it reports. Applies instantly. |
| **Diagnostics** | WiFi signal, uptime, IP address, online status, restart button. |

All settings persist across reboots. Distance reporting is
database-friendly out of the box (reports on ≥5 mm change + 60 s heartbeat).

---

## What you need

- **Sensor:** Adafruit VL53L4CX (product #5425), or any VL53L4CX breakout
- **Board:** an ESP32 dev board (defaults assume a Wemos/LOLIN D1 Mini ESP32; any ESP32 works — override `board:`)
- 4 jumper wires (or a STEMMA QT cable with the Adafruit board)
- Home Assistant with the **ESPHome Device Builder** add-on

## Quick start (10 minutes)

### 1 — Wire it

| Sensor pin | ESP32 pin |
|---|---|
| VIN | 3V3 |
| GND | GND |
| SDA | GPIO21 *(any free pin — just match your YAML)* |
| SCL | GPIO22 *(same)* |
| XSHUT | not needed for a single sensor |

STEMMA QT cable: red→3V3, black→GND, blue→SDA, yellow→SCL.

> ⚠️ **Peel the clear protective film off the sensor window.** Everyone
> misses it, and it ruins readings.

### 2 — Add your secrets (one time, covers all future sensors)

ESPHome dashboard → three-dot menu (top right) → **Secrets**. Paste and
fill in:

```yaml
wifi_ssid: "YourNetworkName"
wifi_password: "YourWifiPassword"
tof_api_key: "PASTE-A-GENERATED-KEY-HERE"   # openssl rand -base64 32
fallback_ap_password: "ChangeMe123"
```

Keep the key — Home Assistant asks for it once per device.

### 3 — Create the device file

In `/config/esphome/`, create `tof-kitchen.yaml` containing exactly this
(it's the entire config):

```yaml
substitutions:
  name: tof-kitchen              # lowercase + hyphens, unique per device
  friendly_name: Kitchen ToF     # how it appears in Home Assistant
  sda_pin: GPIO21                # match your wiring
  scl_pin: GPIO22

packages:
  tof_device: github://itsaustinjordan/esphome-vl53l4cx/packages/tof_sensor.yaml@main
```

### 4 — Install

The device appears as a card in the ESPHome dashboard. Click **Install** →
**Plug into this computer** (USB, Chrome/Edge) for the first flash;
**Wirelessly** ever after. First build takes a few minutes. Healthy logs end
with:

```
[C][vl53l4cx]: VL53L4CX at 0x29 is ranging
[D][sensor]: 'Distance': Sending state 1234.0 mm
```

### 5 — Add to Home Assistant

Settings → **Devices & Services** → the discovered card → **Configure** →
paste your `tof_api_key`. Everything appears immediately: sensors up top,
the tuning controls under **Configuration**, health info under
**Diagnostic**.

> If it errors with "Unable to connect," restart Home Assistant and try
> again — HA caches discovery info. Still stuck? **+ Add Integration** →
> ESPHome → Host: `tof-kitchen.local`, Port `6053`.

---

## Using it

**Presence out of the box:** open the device page, set **Presence
Threshold** to your trigger distance (e.g. 800 mm for "car is parked"), and
automate on the **Presence** entity. The slider applies live — stand in
front of the sensor and drag until it flips.

**Tuning:** jittery readings? Raise **Timing Budget**. Need faster
reaction? Lower **Update Interval** (keep it above the timing budget).
Short-range high-accuracy job? Set **Distance Mode** to `short`. All from
the HA UI, no reflashing, and the values survive reboots.

## Adding another sensor

Wire another board, copy your device file, change `name`, `friendly_name`,
and pins, hit Install, paste the same key in HA. Two minutes per sensor.

## Two sensors on ONE board (advanced)

Supported — see [`multi-sensor-example.yaml`](multi-sensor-example.yaml).
The rules:

- Every sensor's **XSHUT** pin wired to its own GPIO (`xshut_pin:`)
- Every sensor gets a **unique `address:`** (`0x29`, `0x30`, `0x31`, …)
- All sensors share the same `sda_pin`/`scl_pin`

At boot the component holds all sensors in reset, then wakes and
re-addresses them one at a time, and refuses to start (with a clear log
message) if these rules aren't met. (The HA tuning controls in the standard
package are wired to a single sensor; multi-sensor boards configure the
component directly as shown in the example.)

## Configuration reference

Substitutions you can override in your device file:

| Substitution | Default | Notes |
|---|---|---|
| `name` / `friendly_name` | — | Device identity; `name` must be unique |
| `board` | wemos_d1_mini32 | Any ESP32 PlatformIO board ID |
| `sda_pin` / `scl_pin` | GPIO21 / GPIO22 | The component owns these pins — **do not** also define an ESPHome `i2c:` block on them |
| `distance_mode` | long | Initial value; changeable live in HA |
| `timing_budget_ms` | "50" | Initial value (bare number); changeable live in HA |
| `update_interval_ms` | "500" | Initial value (bare number); changeable live in HA |
| `presence_threshold_mm` | "1000" | Initial slider value |
| `presence_off_delay` | 2s | Presence off-flicker smoothing |

Component-level extras (under `- platform: vl53l4cx`, see the multi-sensor
example): `address` (default 0x29; non-default requires `xshut_pin`),
`xshut_pin`, `frequency` (default 100kHz), `object_count` (optional block).

## Troubleshooting

| Symptom | Fix |
|---|---|
| Build can't fetch the repo | Check the `github://` line for typos. |
| Changed settings on GitHub aren't picked up | Device card → three-dot menu → **Clean Build Files** → Install (remote files are cached up to a day). |
| `No ACK at boot address 0x29` | Wiring: SDA/SCL swapped, wrong pins in YAML, loose 3V3/GND. Power-cycle the sensor fully. |
| Distance stuck at a small constant | Protective film still on, or an enclosure edge in the laser's view. |
| Jittery readings | Raise the **Timing Budget** slider; lengthen **Update Interval**. |
| Presence flickers | Raise `presence_off_delay` (device file) or move the threshold away from the resting distance. |
| `Unable to connect` when adding to HA | Restart Home Assistant, then Configure again with the key. |
| HA never asks for the key | Device was flashed before the key existed in secrets — Install again, restart HA. |
| Distance `unknown` | Normal when nothing is in range. Point it at a wall to test. |

## Automation ideas

Automate on **Presence** for arrive/leave. For numeric logic, **Distance**
is a normal sensor — Numeric State triggers ("below 800 for 5 s" = car
pulled in far enough). **Object Count** going `1 → 2` at a doorway makes a
fun people counter.

## Offline / no-GitHub install

Prefer not to depend on this repo at build time? Download it (green **Code**
button → Download ZIP), copy the `components/` and `packages/` folders into
`/config/esphome/`, and in `packages/tof_sensor.yaml` change the
`external_components` source from the `github://` line to `components`.
Device files then use `packages: tof_device: !include packages/tof_sensor.yaml`.

## Pinning a version

Releases are tagged — see [CHANGELOG.md](CHANGELOG.md). For configs that
never change underneath you, pin the tag:

```yaml
packages:
  tof_device: github://itsaustinjordan/esphome-vl53l4cx/packages/tof_sensor.yaml@v1.2.0
```

## Credits & history

Built on ST's official `STM32duino VL53L4CX` driver (BSD-3-Clause, fetched
at compile time). This chip went unsupported in ESPHome for four years
because of a stack of separate traps: a driver object that must be
heap-allocated on ESP32 (esphome/issues#3869), a mandatory interrupt-clear
after every read, ESPHome removing the old `custom:` component system, and
2025+ hybrid Arduino-on-IDF builds no longer auto-linking the Arduino `Wire`
library. This component handles all of it. MIT licensed — see LICENSE.
