# MQ-3 wiring and bench testing

[Project home](README.md) · [Gameplay and thresholds](docs/GAMEPLAY.md) · [Hardware verification](HARDWARE_TEST.md)

This USB-powered setup uses an ACEIRMC MQ-3 module, a LILYGO T-Display-S3 and **eight 2 kΩ resistors**. A ninth resistor can remain spare. Follow the labels on your actual boards; the diagram is a logical connection map, not header order.

For the custom PCB, use the separate [Rev A carrier guide](hardware/README.md). That unbuilt design includes its own divider and can take a bare sensor; do not stack this breadboard divider onto it. Its optional module connector is a different assembly variant with an explicit pin order.

![MQ-3 to LILYGO: 5 V supply, common ground and an eight-resistor divider to GPIO1](docs/mq3-wiring.png)

Dots mark connections. Crossings without dots are not connected.

## Connections

Power off before changing wiring. Four 2 kΩ resistors in series make the upper 8 kΩ chain; four more make the lower 8 kΩ chain.

| MQ-3 module pin | Connection |
| --- | --- |
| VCC | LILYGO **5V**, with the board powered by USB |
| GND | LILYGO **GND**, shared with the bottom of the divider |
| AO | Upper resistor chain, then the divider junction to **GPIO1** |
| DO | Leave disconnected |

```text
MQ-3 AO -- 2k -- 2k -- 2k -- 2k --+-- GPIO1 (ADC1_CH0)
                                 |
                                 +-- 2k -- 2k -- 2k -- 2k -- GND
```

Equal resistor chains divide the voltage by two: an ideal 5.0 V AO signal becomes 2.5 V at GPIO1. **Never connect AO or DO directly to an ESP32 input.** The sensor's heater takes 5 V power, not a GPIO or the 3V pin. Use the printed board labels and [LILYGO's board documentation](https://github.com/Xinyuan-LilyGO/T-Display-S3) to locate the header pins. This guide does not establish that battery-only operation supplies the module's 5 V requirement.

The firmware uses 12-bit ADC readings and 11 dB attenuation. Espressif documents an approximate 0–3.1 V measurable range for the ESP32-S3 at that attenuation; it does not make the input 5 V tolerant. [ADC documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html)

A typical four-band 2 kΩ resistor is red–black–red plus a tolerance band, often gold. Check resistance with a meter if the value is uncertain; five-band codes are read differently. The [module purchase listing](https://www.amazon.com/dp/B0978KZQVY) identifies the selected module, but follow its printed pin labels rather than assuming the photographed order.

## Verify the divider

1. Unplug USB and leave the junction disconnected from GPIO1 while assembling the divider.
2. Power the board/module by USB. In DC voltage mode, put the black probe on common GND.
3. Measure AO and the junction. The junction should be approximately **half of AO** and no higher than about **2.6 V** in this nominal 5 V setup. If it is not, disconnect power and inspect the connections.
4. If both points read zero, that alone does not verify the divider. With power removed, check the two resistor chains, or test the isolated divider against a known 5 V source while GPIO1 remains disconnected.
5. Unplug USB, connect the verified junction to GPIO1, then power up again.

The heater normally gets hot. Keep the sensor dry and clear of loose wires; do not immerse it or let liquid contact it. Physical voltage-divider measurements have not yet been recorded for the tested assembly. Observing an ADC response does not replace this electrical check.

## Bench screen

Hold the lower button for the evening menu, then select **MQ-3 setup**. The screen shows GPIO1 millivolts, raw ADC counts, a 0–3100 mV trend graph and elapsed **acquisition-active time**. In LIVE mode the timer can predate opening the screen; it is not a heater warm-up timer.

The ADC averages four conversions every 100 ms. In LIVE mode acquisition continues on every screen, retaining the rolling window when you navigate. In DEMO it runs only on the bench screen; reopening that screen starts a fresh window.

NEXT cycles **Zero in clean air**, **Clear air baseline** and **Back to menu**. OK performs the action. Zero accepts a temporary diagnostic baseline only after 100 samples, with a latest reading of 50–2700 mV and a window spread of at most 50 mV. Once set, the screen shows the difference from that window average. Rebooting clears the bench zero. In LIVE mode navigation preserves it; in DEMO reopening the bench resets it.

**Bench zero is independent of feeding.** It does not change the game baseline, clean-air band or sensitivity. The bench's wider acceptance range is useful for diagnostics; a quiet bench trace does not automatically mean Feed is ready. Feeding requires every reading in the window to be 50–250 mV, with at most 25 mV spread.

## Test with cup vapor

No drinking is needed to check sensor response.

1. Let the powered sensor settle in clean air. Watch the trace on MQ-3 setup.
2. Bring a cup of beer, wine or sake near the dry sensor so vapor can reach it, without contact with the liquid. Observe the rise, then remove the cup and watch recovery.
3. To record a game sample, select a pet and choose **Wake / feed**. A ready background window starts GET READY immediately; otherwise Sensor recovering waits automatically.
4. Wait through the **five-second countdown**. Bring vapor near the sensor only at **BLOW**, which lasts **ten seconds**. Remove the cup when the result appears.
5. Check the pet's History. The entry is labeled MQ3. Recovery continues while you browse; a new feeding waits only when needed.

Keep cups away during baseline collection. Neither cup vapor nor ordinary breath establishes a BAC scale: humidity and exposure distance also affect response. The current thresholds are game settings. See the [gameplay guide](docs/GAMEPLAY.md#game-sensitivity) for the 400 mV gate, default 1200 mV span and exact score formula. All app voltages are measured after the divider at GPIO1.

A new sensor also needs conditioning before repeatable comparisons. The Winsen MQ-3 manual specifies more than 48 hours of preheat under its standard test conditions. The exact sensing element in a reseller module has not been verified; use its own datasheet when available. A quiet ten-second trace is not a substitute for conditioning. [Winsen MQ-3 manual](https://cdn.sparkfun.com/datasheets/Sensors/Biometric/MQ-3%20ver1.3%20-%20Manual.pdf)

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Sensor recovering immediately after startup | Allow the first full ten-second window to fill |
| Recovery continues after exposure | Remove the cup and allow the entire rolling window to return to 50–250 mV with no more than 25 mV spread |
| Low input / no response | Check power, common ground, AO wiring and the divider with a meter |
| High input | Disconnect power and inspect the divider before reconnecting GPIO1 |
| Level zero despite a small rise | Peaks below 400 mV intentionally score zero; a zero-level sample still wakes the pet |
| Level reaches 100 too easily | In Response settings, increase the span; this changes game sensitivity only |
| A rejected capture | No history or reward was saved; correct the signal and explicitly retry |
| Bench zero works but Feed waits | Bench zero and feeding use different readiness limits |

Cup testing has demonstrated qualitative response, recovery and stored samples. See the [verification record](HARDWARE_TEST.md) for what was physically observed versus tested with synthetic ADC values.
