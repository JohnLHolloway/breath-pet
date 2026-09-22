# Development

[Project home](../README.md) · [Gameplay](GAMEPLAY.md) · [Verification record](../HARDWARE_TEST.md)

## Toolchain and board

The target is the LILYGO T-Display-S3, ESP32-S3 with 16 MB flash, 8 MB OPI PSRAM and a 320 × 170 ST7789 parallel display. The build pins `espressif32@6.5.0` / Arduino-ESP32 2.0.14. Python development dependencies are pinned in `requirements-dev.txt`; display/touch libraries are vendored in `lib/`.

| Function | GPIO |
| --- | --- |
| Display power / backlight | 15 / 38 |
| Upper NEXT/BOOT / lower OK button | 0 / 14 |
| MQ-3 divided analog input | 1 / ADC1_CH0 |
| Optional touch SDA / SCL / reset / interrupt | 18 / 17 / 21 / 16 |

Landscape orientation uses display rotation 3, placing USB and buttons on the left. Firmware probes CST816-family address `0x15` and CST328 address `0x1A`. No controller was detected on the tested board; touch support is unverified on actual touch hardware.

## Build, upload and monitor

On Windows, with Python and Git installed:

```powershell
.\dev.ps1 build
.\dev.ps1 upload -Port COM3
.\dev.ps1 monitor -Port COM3
```

The helper creates `.venv` and installs requirements on first use. After pulling changed dependencies into an existing checkout, update them explicitly:

```powershell
.venv\Scripts\python.exe -m pip install -r requirements-dev.txt
```

If PowerShell blocks scripts, use a per-invocation override such as `powershell -ExecutionPolicy Bypass -File .\dev.ps1 build`. Ctrl+C exits the monitor; close it before any upload, test or capture because each needs exclusive serial-port access. To list ports, run `.venv\Scripts\python.exe -m platformio device list`.

If upload cannot connect, hold BOOT, press/release RST, release BOOT, and retry. If needed, unplug USB, hold BOOT while reconnecting, then release it. The port may change; check again. Press RST after upload if the application does not start.

On Linux/macOS:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
python -m platformio run -e breath-pet
python -m platformio run -e breath-pet -t upload --upload-port /dev/ttyACM0
```

Replace the example port for your system and ensure serial-device access. `.pio/`, `.cache/platformio/` and `.venv/` are local and ignored by Git.

## Isolated tests

With the board attached and serial monitor closed:

```powershell
.\dev.ps1 test -Port COM3
```

This builds/flashes `device-test`, runs `tools/test_device.py --reset-demo-data`, and restores `breath-pet` in a `finally` block even when a test fails. Keep USB connected. If restoration fails, run `dev.ps1 upload` again. Normal firmware boots LIVE; test firmware boots DEMO.

| Saved data | Normal firmware | Test firmware |
| --- | --- | --- |
| Roster, histories, calibration | `breath-pet` | `breath-test` |
| Nickname collections, party progress | `pet-fun` | `fun-test` |

The runner refuses its reset unless the device reports `test_mode: true`. `-ResetDemoData` on the PowerShell helper is retained for compatibility but is not required. The latest device suite has **99 checks**, covering navigation, capacity, owner isolation, persistence, rolling baseline/recovery, five-plus-ten-second capture, rejection/cancellation, sleep, game wins/misses, loot timing, wardrobe, awards and returning collections. `selftest` adds firmware-side rule, migration and ring-buffer checks. See [hardware verification](../HARDWARE_TEST.md) for the scope and limits of the result. The full private run report goes to ignored `test-results.json`.

Test firmware alone accepts ADC injection, accelerated game time and a fixed bubble position. Advancing game minutes does **not** skip real-time feed or nap button cooldowns. These commands are absent from normal builds:

| Command | Test-only effect |
| --- | --- |
| `test sensor 100` | Inject a chosen integer ADC millivolt value, 0–999 |
| `test sensor high` | Inject 2900 mV for rejection checks |
| `test sensor off` | Resume physical ADC readings |
| `test minutes 10` | Advance the powered game clock |
| `test bubble 50` | Fix bubble position, 0–100; 50 is a catch, 0 a miss |
| `test fun reset` | Reset isolated collections/progress |

## Screenshots

Capture one current screen without switching firmware:

```powershell
.venv\Scripts\python.exe tools\capture_screen.py artifacts\tank.png --port COM3 --command tank
```

Repeated `--command` arguments navigate before capturing. Avoid commands that create normal readings if you are only reviewing layout. This exports the rendered framebuffer, not a camera photo or panel readback.

To regenerate the public gallery:

```powershell
.venv\Scripts\python.exe tools\capture_gallery.py --port COM3
```

Start with **normal firmware** on the board. The script:

1. Saves a private normal-state snapshot under ignored `artifacts/gallery/`.
2. Flashes isolated test firmware, verifies its mode, resets only test data, and creates fictional pets through game commands.
3. Captures the current pages, using synthetic ADC values for sensor screens.
4. Restores normal firmware in `finally`, checks the normal roster, exact histories, sensitivity, equipped/owned items and key stats against the snapshot, and checks both save areas.
5. Publishes the successful PNG set and `manifest.json` under `docs/screenshots/` only after restoration and verification succeed.

A failed or interrupted run still attempts restoration; a disconnected board may need a manual upload. The script leaves the normal board on the tank. Private normal snapshots and upload logs are never copied into the gallery. It replaces isolated test pets/collections and does not preserve a previous test fixture.

All public screenshots are 320 × 170 actual device frames. The manifest records timestamp, firmware version, source commit, test binary SHA-256 and individual image SHA-256 values. Capture time, random accessory rewards and animation frames may differ between runs. **TEST** indicates isolated firmware; an MQ3 source label in a gallery image uses injected values, not a real person's reading. Review the images visually and update [gallery captions](screenshots/README.md) when the UI changes. Remove superseded images and broken references before publishing.

## Serial protocol

Use newline-terminated UTF-8 commands at **115200 baud**. Unsolicited status JSON is emitted periodically. Commands can be prefixed with an integer request ID, for example `@123 status`. The device acknowledges with `CMD 123`, and JSON replies echo `request_id: 123`; unsolicited JSON has ID zero. Match IDs and accumulate partial lines before parsing. If an action's reply is lost, recover with a read-only status/history query rather than repeating the action.

| Commands | Behavior |
| --- | --- |
| `status`, `history` | State JSON; selected pet's full stored reading metadata |
| `selftest`, `touchscan` | Firmware rule checks; diagnostic I2C scan |
| `join CAPTAIN 1` | Add an unused 1–8-character alphanumeric name and species 0–5 |
| `select 0`, `tank` | Open an occupied slot 0–5 or return to tank |
| `ui next`, `ui select`, `ui back`, `ui menu` | Drive the same navigation handlers as the physical controls |
| `sample 25` | Instant fake capture, 0–100, in DEMO only; observes feed cooldown |
| `rest` | Nap the selected pet; observes nap cooldown |
| `input live`, `input demo` | Switch input for this boot |
| `cal default`, `cal minus`, `cal plus` | Restore or adjust the active mode's response span |
| `cal zero` | Set fake-value zero in DEMO; rejected in LIVE |
| `sensor open`, `sensor zero`, `sensor clear` | Bench screen and independent temporary diagnostic baseline |
| `night new CONFIRM` | Clear this evening's roster/history; retain collections/upgrades |
| `screen` | Binary framebuffer export |
| `reboot`, `help` | Restart; print supported normal commands |

LIVE baseline capture is automatic. `sensor zero` does not alter gameplay sensitivity or readiness. History includes source (`MQ3` or `DEMO`), score, original raw value, baseline/peak/span, boot/uptime and legacy health delta. Do not interpret game levels as BAC.

`screen` emits `FRAME 320 170 RGB565BE`, a newline, then 108800 binary bytes (big-endian RGB565). The payload is followed by a newline and `END_FRAME`. Read exactly that many bytes before returning to line-oriented parsing; `tools/capture_screen.py` handles conversion to PNG.

## Source and persistence

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Navigation, input, feed timing and serial protocol |
| `src/ui.h` | Tank animation, creatures, shared controls and screen rendering |
| `src/party.h` | Per-player state, ring-buffer history, calibration and save migration |
| `src/fun.h` | Energy/sleep, collections, rewards, personalities, decorations and awards |
| `src/mq3.h` | ADC acquisition, rolling window, capture and rejection checks |
| `src/hardware.h` | Display setup, button debouncing and touch input |
| `tools/test_device.py` | Device integration suite |
| `tools/capture_screen.py`, `tools/capture_gallery.py` | Single frame and isolated gallery capture |

Current party state uses the versioned `party-v2` record; collections/progress use a separate `fun-v1` record. Startup removes an obsolete `party-v1` key **only after validating the v2 record**, reclaiming space in the 20 KB NVS partition. Do not erase or format NVS as routine troubleshooting. Normal and test namespaces must remain separate. `storage_ok` and `fun_storage_ok` in status report save health.

Keep `.env`, flash dumps, local logs, private snapshots and real player readings out of commits. Documentation screenshots must use fictional isolated fixtures.
