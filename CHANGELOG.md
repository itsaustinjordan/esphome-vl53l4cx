# Changelog

## v1.2.0

- **Presence entity** (occupancy) with an adjustable **Presence Threshold**
  slider in Home Assistant -- no automations or math needed for parking,
  desk, bed, or mailbox use cases. Reacts to the raw reading instantly.
- **Live tuning from the HA UI:** Distance Mode (short/medium/long),
  Timing Budget, and Update Interval are now controls on the device page.
  Changes apply to the chip on the fly -- no reflash, no reboot -- and
  persist across restarts.
- **Diagnostics:** WiFi Signal, Uptime, IP Address, Status, and a Restart
  button, all filed under the device's Diagnostic/Configuration sections.
- **Recorder-friendly reporting:** Distance now reports on >= 5 mm change
  plus a 60 s heartbeat instead of flooding the HA database twice a second.
- **Smarter stall recovery:** the ranging watchdog is now time-based and
  scales with the configured timing budget.
- Device info in HA now shows the project name and version.
- **Breaking (package users only):** substitutions `timing_budget` /
  `update_interval` are renamed `timing_budget_ms` / `update_interval_ms`
  and take bare numbers (e.g. `"50"`). Device files that only set
  name/friendly_name/pins are unaffected.

## v1.1.0

- First public release: ESPHome external component for the VL53L4CX with
  distance + object count, multi-sensor-on-one-board support (coordinated
  XSHUT re-addressing), remote package for 8-line device configs.
