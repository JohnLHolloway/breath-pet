# Planned MQ-3 input — not enabled yet

The ACEIRMC module's listing specifies **5 V VCC**, GND, AO (analog output) and DO (threshold output). We want **AO**, since DO only indicates whether a threshold was crossed. Follow the labels on your actual board, not an assumed header order. [Module listing](https://www.amazon.com/dp/B0978KZQVY)

## Proposed USB-powered wiring

Power off before wiring. Use a breadboard/jumper wires and two 10 kΩ resistors:

| Module | Connection |
| --- | --- |
| VCC | LILYGO **5V** pin while powered by USB |
| GND | LILYGO **GND** |
| AO | Through the divider below to **GPIO1** |
| DO | Leave disconnected |

```text
MQ-3 AO ---- 10 kΩ ----+---- GPIO1 (ADC1_CH0)
                      |
                    10 kΩ
                      |
                     GND
```

Equal resistor values halve the analog voltage: a 5 V module output becomes 2.5 V at GPIO1. **Do not connect AO directly to an ESP32 input.** Configure that ADC input for its widest attenuation range; Espressif documents up to approximately 3.1 V for the ESP32-S3 at 11 dB. [Espressif ADC documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html)

GPIO1 is exposed as ADC1_CH0 on LILYGO's header and is unused by this firmware. The sensor's heater should use the 5 V supply, not a GPIO or the 3 V pin. [LILYGO pin map](https://github.com/Xinyuan-LilyGO/T-Display-S3/blob/main/image/T-DISPLAY-S3-TOUCH.png)

A typical four-band 10 kΩ resistor is **brown–black–orange**, followed by its tolerance band (often gold). Verify resistance with a meter when available; other band counts use a different reading scheme. We have not identified the user's resistors or verified any wiring yet.

## Remaining implementation and testing

1. Confirm the module labels and divider values; measure the divider output before attaching GPIO1.
2. Add ADC acquisition and a distinct sensor mode, keeping raw sensor readings separate from fake game units.
3. Condition/warm the sensor and observe a stable clean-air baseline. Initial conditioning is substantial: the Winsen MQ-3 manual specifies more than 48 hours under its standard test conditions. The exact sensor fitted to this reseller module is unverified, so follow its own datasheet when available. [Winsen MQ-3 manual](https://cdn.sparkfun.com/datasheets/Sensors/Biometric/MQ-3%20ver1.3%20-%20Manual.pdf)
4. Record raw and baseline-relative response through the timed feeding flow, test repeatability and recovery, then choose a game-response mapping.

The current zero/span screen calibrates only simulated game input. It is not a sensor calibration, and the MQ-3 response is not being converted into BAC. The current firmware never calls `analogRead`.
