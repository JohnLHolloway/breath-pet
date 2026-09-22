# Breath Pet

A pocket party pet tank for the **LILYGO T-Display-S3**, with a 1.9-inch ST7789 screen. Friends pick a nickname, adopt a pet for the evening, and watch everyone's pets swim around together.

![Empty tank with Add Pet selected](docs/empty-tank.png)

![Example tank after six demo pets have been adopted](docs/swimming-tank.png)

**Pet feeding is simulated. Menu → MQ-3 setup now reads real sensor voltage for bench testing; it does not measure BAC or feed pets.**

## Join the evening

The tank starts empty. Choose **Add Pet**, choose a nickname, then adopt a species. Up to six people can join, one at a time. Existing pets swim freely around a shared aquarium; there are no placeholder pets or individual boxes.

Nicknames include Captain, Goose, Bean, Chaos, Pickle, Nugget, Bubbles, Spud, Mochi, Gremlin, Waffles, Noodle, Goblin, Peach, Squid and Biscuit. Used names are skipped to avoid confusing people's readings. Species are Blob, Axolotl, Bat, Cat, Ghost and Frog.

Choose a swimming pet to open its care screen:

- **Feed:** select a fake input (0, 25, 50 or 85), then Start demo. A three-second sample animation produces a result for that pet only.
- **History:** that pet's last 16 samples, including the input, game score and health change.
- **Rest:** recover health without taking a sample.

The roster, species, care stats, calibration and each person's history survive restarts. **Menu → Start a new evening** clears pets and history only after confirmation; Keep pets is selected by default. Calibration settings are retained. The previous solo prototype's saved data is left under its old NVS key and is not assigned to anyone in the new game.

## Two-button controls

| Button | Tap | Hold for 1.2 seconds |
| --- | --- | --- |
| **Upper / BOOT / GPIO0** | NEXT: cycle option, nickname, pet or demo value | Back / cancel |
| **Lower / GPIO14** | OK: choose the highlighted action | Open evening menu |

Hold the device landscape with USB and the two front buttons on the **left**, like the product photo. A fixed left rail labels each physical button; pressing it lights its label. The large gold bar shows exactly what OK will do. Every screen, including the tank and evening menu, uses the same gold selector and option counter (for example, 2/4). NEXT consistently cycles without confirming; OK confirms. On History, NEXT advances the page. A golden ring identifies the selected swimming pet. BOOT during power-up still enters firmware download mode.

In the tank, NEXT cycles through existing pets, Add Pet and Menu. Six is the capacity, not the starting population. Hold the upper button to go back or cancel a sample; hold the lower button for the evening menu.

Touch uses the same flows when a supported controller responds. Firmware probes CST816-family (0x15) and CST328 (0x1A) on SDA18/SCL17, reset21 and interrupt16. **The currently tested board does not respond to either address or the full I2C scan, and the user reports that touch does not work.** Everything is operable with buttons. Screen rendering does not prove touch hardware is present; LILYGO sells both variants.

## Game rules — not alcohol units

The score is `clamp((fake_input - zero) * 100 / span, 0, 100)`. It is a fictional game value, never BAC.

- Score **0**: +8 food, +6 joy. Anyone can play without alcohol.
- Score **1–69**: +15 food, +12 joy; the reward is capped across this range.
- Score **70–100**: overload; +3 food, −12 joy, and health damage `10 + floor((score - 70) / 2)`.
- Stats stay within 0–100. Rest adds up to 8 health and 4 joy, and clears the overload expression.
- Each pet has a five-second feeding cooldown and ten-second rest cooldown to prevent repeat-button accidents.
- While powered, food and joy decay by one per minute; an empty food meter also costs one health. There is no offline decay.

Menu → Demo calibration changes the fake-input baseline and span (25–200). It exercises the software response only. Earlier history entries keep the score and damage recorded at capture.

Samples carry a global sequence number plus boot number and seconds since that boot. These are session labels, not wall-clock timestamps. No Wi-Fi, network account, or external service is involved in gameplay.

## MQ-3 readiness

**Menu → MQ-3 setup** reads GPIO1 voltage with a live trend and temporary clean-air zero. It checks a ten-second window for drift and flags unusually low/high input. These checks help with bench setup; they do not certify sensor readiness. No drinking is required. Real readings stay separate from simulated pet feeding and history. Alcohol response, conditioning, recovery and a game mapping still need verification.

The intended module is the [ACEIRMC MQ-3 board](https://www.amazon.com/dp/B0978KZQVY). See [MQ3_WIRING.md](MQ3_WIRING.md) for the wiring diagram using eight 2 kΩ resistors, 5 V power and GPIO1, plus multimeter checks and clean-air testing. Verify the divider voltage before connecting GPIO1. This prototype does not convert voltage to BAC.

## Build and upload

Install Python and Git, clone this repository, then run from its directory:

```powershell
.\dev.ps1 build
.\dev.ps1 upload -Port COM3
.\dev.ps1 monitor -Port COM3
```

The helper creates a local `.venv` on first use. Build products go into `.pio` and compiler downloads into `.cache/platformio`; both are ignored by Git. For an existing environment after pulling changes, run `.venv\Scripts\python.exe -m pip install -r requirements-dev.txt`.

If PowerShell blocks scripts, use `powershell -ExecutionPolicy Bypass -File .\dev.ps1 build` for that invocation. Close the monitor before flashing; Ctrl+C exits it. If upload cannot connect, hold BOOT, press/release RST, release BOOT, and retry. Press RST afterward if needed.

On Linux/macOS, create and activate a Python virtual environment, install `requirements-dev.txt`, then use `pio run` and `pio run -t upload --upload-port <your-port>`.

The build pins `espressif32@6.5.0` / Arduino-ESP32 2.0.14, following LILYGO's working parallel TFT setup. Display power: GPIO15; backlight: GPIO38; buttons: GPIO0/GPIO14. Hardware: 16MB flash, 8MB OPI PSRAM.

## Tests and diagnostics

```powershell
# Temporarily flashes a test build with separate saved data, then restores normal firmware.
.\dev.ps1 test -Port COM3
```

Tests use the separate `breath-test` NVS namespace; your normal pets in `breath-pet` are preserved. The script refuses to reset data unless the test firmware reports `test_mode: true`. The helper restores the normal firmware even when tests fail (leave USB connected). `-ResetDemoData` remains accepted for older scripts but is no longer required.

The device test drives the same navigation handlers as the physical buttons. It checks adoption, taken-name skipping, six-player capacity, timed feeding, cooldowns, owner isolation, overload/recovery, input validation, persistent stats/history and calibration, animation, and new-evening confirmation. A separate on-device test exercises stat bounds and per-person history rollover. Test output is saved to ignored `test-results.json`.

Newline-terminated serial commands at 115200 baud:

```text
status                   selftest                 touchscan
join CAPTAIN 0           select 0                 history
sample 25                rest                     tank
ui next                  ui select                ui back             ui menu
cal zero                 cal default              cal minus           cal plus
night new CONFIRM        screen                   reboot
sensor open              sensor zero              sensor clear
```

`join` accepts a unique 1–8-character alphanumeric nickname and a species index 0–5. `select` takes an occupied slot 0–5. `sample` accepts fake units 0–100 and observes the selected pet's cooldown. `night new CONFIRM` clears the evening. A `CMD` line precedes each command's reply, separating it from unsolicited status JSON.

`screen` exports the actual RGB565 framebuffer, useful for layout QA but not a physical panel readback. For example:

```powershell
.venv\Scripts\python.exe tools\capture_screen.py artifacts\tank.png --command tank
```

## Files and licensing

`src/party.h` holds per-person state, persistence and game rules. `src/main.cpp` handles navigation, input and feeding. `src/ui.h` draws the shared selection UI and screens. `src/mq3.h` handles the isolated ADC bench monitor. `src/hardware.h` contains the display initialization, button debouncing and touch drivers.

Original code is MIT licensed. TFT_eSPI 2.5.43, TouchLib, the board definition and ST7789 initialization come from [LILYGO's T-Display-S3 repository](https://github.com/Xinyuan-LilyGO/T-Display-S3), commit `ec889e789b3cf093412689a143f7f37b42b56af7`, with vendor licenses retained in `lib/`.

No factory flash dump, local logs or saved player readings are published. LILYGO provides [factory firmware](https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/main/firmware).
