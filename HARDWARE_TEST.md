# Hardware verification — 2026-09-22

Tested on a USB-connected LILYGO T-Display-S3: ESP32-S3 revision 0.2, 16MB flash, approximately 8MB PSRAM. Party firmware compiled and flashed successfully, including flash hash verification.

The version 3 party implementation passed **42 automated device checks**. These exercise the actual button action handlers: empty tank/Add Pet, nickname/species selection, timed feeding, duplicate-input cooldown, per-person history and stat isolation, overload damage, rest, calibration, persistence across a real reboot, malformed inputs, six-person capacity, animation and new-evening confirmation. The on-device self-test also checks stat bounds, capped rewards, calibration bounds and a separate 16-record history ring for each pet.

The final UI review used the ESP32's rendered framebuffer for the empty tank, populated swimming tank, nickname picker, species picker, selected pet, feed prompt, overload result and personal history. These captures verify layout, not physical panel or touch behavior. Tests and captures used only fictional nicknames and fake inputs. The test roster was cleared afterward.

The user confirmed the original physical pet display and both buttons worked. The new Add Pet flow is available for user verification. No touch controller responded at either expected address or elsewhere on the I2C bus after reset, and the user reports touch does not work. This may be the non-touch variant; hardware presence/alignment remain unverified. The application remains fully usable through the two buttons.

No MQ-3 has been wired, read or calibrated. The module listing and proposed GPIO1 divider wiring were checked, but resistor values and physical wiring still need confirmation. All calibration in this build is explicitly for simulated game input.
