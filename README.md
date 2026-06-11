# ESPHome VL53L4CX — Laser Distance Sensors for Home Assistant

Turn a $15 laser Time-of-Flight sensor and a $7 ESP32 board into a polished
Home Assistant distance sensor: parking guides, mailbox monitors, desk/bed
presence, water-level gauges, people counting at a doorway.

This is a complete ESPHome integration for the **ST VL53L4CX** (the Adafruit
#5425 breakout) — the long-range, multi-target member of ST's laser family,
previously unsupported in ESPHome. **Nothing to download:** your config
references this repo and ESPHome fetches everything at build time.

Each sensor gives you two entities in Home Assistant:

| Entity | What it does |
|---|---|
| **Distance** | Nearest target, in millimeters, ~1 mm to ~6,000 mm. Reads `unknown` when nothing is in view — perfect for presence triggers. |
| **Object Count** | How many separate targets the sensor sees at once. Unique to this chip — the cheaper VL53L0X/L1X can't do it. |

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
paste your `tof_api_key`. Both entities appear immediately.

> If it errors with "Unable to connect," restart Home Assistant and try
> again — HA caches discovery info. Still stuck? **+ Add Integration** →
> ESPHome → Host: `tof-kitchen.local`, Port `6053`.

---

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
message) if these rules aren't met.

## Configuration reference

Override any of these as substitutions in your device file (simple) or use
the component directly under `sensor:` for full control (see the multi-sensor
example):

| Option | Default | Notes |
|---|---|---|
| `name` / `friendly_name` | — | Device identity; `name` must be unique |
| `board` | wemos_d1_mini32 | Any ESP32 PlatformIO board ID |
| `sda_pin` / `scl_pin` | GPIO21 / GPIO22 | The component owns these pins — **do not** also define an ESPHome `i2c:` block on them |
| `distance_mode` | long | `short` / `medium` / `long` (~6 m) |
| `timing_budget` | 50ms | 20–500 ms; longer = steadier readings |
| `update_interval` | 500ms | Must be longer than `timing_budget` |

Component-level extras (under `- platform: vl53l4cx`): `address` (default
0x29; non-default requires `xshut_pin`), `xshut_pin`, `frequency` (default
100kHz), `object_count` (optional sensor block).

## Troubleshooting

| Symptom | Fix |
|---|---|
| Build can't fetch the repo | Check the `github://` line for typos and that the repo is public. |
| Changed settings on GitHub aren't picked up | Device card → three-dot menu → **Clean Build Files** → Install (remote files are cached up to a day). |
| `No ACK at boot address 0x29` | Wiring: SDA/SCL swapped, wrong pins in YAML, loose 3V3/GND. Power-cycle the sensor fully. |
| Distance stuck at a small constant | Protective film still on, or an enclosure edge in the laser's view. |
| Jittery readings | Raise `timing_budget` to `100ms`, `update_interval` to `1s`. |
| `Unable to connect` when adding to HA | Restart Home Assistant, then Configure again with the key. |
| HA never asks for the key | Device was flashed before the key existed in secrets — Install again, restart HA. |
| Entities `unknown` | Normal when nothing is in range. Point it at a wall to test. |

## Automation ideas

`Distance` is a normal numeric sensor — Numeric State triggers ("below 800
for 5 s" = car parked far enough in) or template binary sensors for
presence. `Object Count` going `1 → 2` at a doorway makes a fun people
counter. Distance reads `unknown` when the view is clear, so "state changed
from unknown" is itself a clean arrival trigger.

## Offline / no-GitHub install

Prefer not to depend on this repo at build time? Download it (green **Code**
button → Download ZIP), copy the `components/` and `packages/` folders into
`/config/esphome/`, and in `packages/tof_sensor.yaml` change the
`external_components` source from the `github://` line to `components`.
Device files then use `packages: tof_device: !include packages/tof_sensor.yaml`.

## Pinning a version

Releases are tagged. For configs that never change underneath you, pin the
tag in both lines of your setup:

```yaml
packages:
  tof_device: github://itsaustinjordan/esphome-vl53l4cx/packages/tof_sensor.yaml@v1.1.0
```
