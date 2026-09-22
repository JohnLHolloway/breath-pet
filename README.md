# Breath Pet

A small virtual pet for the **LILYGO T-Display-S3** (ESP32-S3, 1.9-inch ST7789, 170×320). Animated pixel creature, food/joy/energy meters, saved sample history, and calibration controls. Runs from USB power without Wi-Fi or additional wiring.

**Prototype: MQ-3 input is disabled. Every sample is simulated, in arbitrary game units. This software does not measure alcohol or BAC.**

## Screens

- **Pet:** tap the creature to feed it. Tap the meters to capture the next fake sample (0 → 45 → 85 → 0). The pet reacts to the calibrated score with happy, wobbly, or dizzy expressions.
- **Readings:** the latest 16 simulated samples, newest first, with raw input and the score at capture. Prev/Next page through them. Clear history requires a second tap.
- **Setup:** Zero current stores the selected fake input as the baseline. Span −25/+25 adjusts sensitivity (25–200); a smaller span increases the score. Defaults restores zero=0, span=100. These settings exercise the software workflow; they do not calibrate a real MQ-3.

Tap the bottom tabs to switch screens on a supported touch board. Firmware probes both CST816-family (0x15) and CST328 (0x1A) controllers on SDA18/SCL17, with reset21 and interrupt16. Touch uses LILYGO's coordinate swap for landscape. **Touch functionality and alignment must be checked on the physical board; the standard non-touch version has the same LCD.**

## Button controls

Every feature is also available with the two front buttons:

| Action | BOOT / GPIO0 | Other button / GPIO14 |
| --- | --- | --- |
| Pet screen, tap | Feed | Capture next fake sample |
| Readings or Setup, tap | Activate the outlined option | Move the outline to the next option |
| Hold for 1.2 seconds | Reset pet on Pet; return to Pet elsewhere | Next screen |

Taps trigger on release. Holding BOOT **during power-up** enters firmware download mode; the hold gestures above apply while the application is running.

Food, joy, and energy decay every 30 seconds; feeding replenishes them. The pet itself resets on restart. History and calibration settings survive restarts in NVS. Resetting the pet does not erase history or calibration.

## What gets saved

The most recent 16 samples store a sequence number, raw fake input, calibrated score, boot number, and seconds since that boot. There is no real-time clock or network time: boot/seconds are session-relative labels, not dates or times of day. Old scores are preserved when calibration changes. Settings and history write only on user actions, with one additional boot-counter write per restart.

The demo score is `clamp((raw - zero) * 100 / span, 0, 100)`, using integer arithmetic. Simulated thresholds are 30 for wobbly and 70 for dizzy. This has no physical alcohol units. MQ-3 wiring, warm-up, raw acquisition, and actual sensor characterization remain future work.

## Build and upload

Install Python and Git, clone this repository, then run from its directory:

```powershell
.\dev.ps1 build
.\dev.ps1 upload -Port COM3
.\dev.ps1 monitor -Port COM3
```

The helper creates a project-local `.venv` on first use and installs pinned development tools. PlatformIO downloads compilers into `.cache/platformio`; build products go into `.pio`. Both directories are ignored by Git. If PowerShell blocks local scripts, use `powershell -ExecutionPolicy Bypass -File .\dev.ps1 build` for that invocation.

On other operating systems:

```sh
python -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
pio run
pio run -t upload --upload-port /dev/ttyACM0
```

Change the port to match your machine. Close any serial monitor before flashing or testing. If upload cannot connect, hold BOOT, press/release RST, release BOOT, then upload. Press RST afterward if needed.

The build pins `espressif32@6.5.0` / Arduino-ESP32 2.0.14, following LILYGO's known-working setup for this parallel TFT. Display power is GPIO15, backlight GPIO38, buttons GPIO0/GPIO14. The board has 16MB flash and 8MB OPI PSRAM. No sensor pin is configured.

## Device tests and serial interface

```powershell
# Replaces saved fake readings/settings, reboots the board, then leaves three demo samples.
.\dev.ps1 test -Port COM3 -ResetDemoData
```

The integration test checks rendering progress, stat changes and bounds, sample reactions, invalid commands, history rollover, calibration boundaries, navigation, and NVS persistence across a real reboot. Tests save an ignored `test-results.json`. Touch detection is reported separately: successful serial page navigation does not prove touch alignment.

At 115200 baud, send newline-terminated commands:

```text
status                 feed                  cycle
sample 0               sample 45             sample 85
reset                  selftest              help
history                history clear
cal zero               cal default           cal minus            cal plus
page pet               page readings         page setup
touchscan              reboot
```

Status repeats every five seconds as JSON. It includes touch controller, touch/button counters, screen, memory, calibration and storage health. `touchscan` resets/probes the touch hardware and scans the I2C bus. `reboot` restarts the device. The serial connection is optional; gameplay does not wait for a host.

## Source and licensing

Original project code is MIT licensed. The trimmed TFT_eSPI 2.5.43 and TouchLib libraries, board definition, and updated ST7789 panel initialization come from [LILYGO's T-Display-S3 repository](https://github.com/Xinyuan-LilyGO/T-Display-S3), commit `ec889e789b3cf093412689a143f7f37b42b56af7`. Vendor license notices remain in `lib/`.

No factory flash dump, local logs, or saved readings are included. LILYGO supplies [factory firmware](https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/main/firmware) if you want to return to its demo.
