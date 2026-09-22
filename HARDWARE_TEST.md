# Hardware verification — 2026-09-22

Tested on a USB-connected LILYGO T-Display-S3: ESP32-S3 revision 0.2, 16MB flash, approximately 8MB PSRAM. Firmware compiled and flashed successfully, with flash data hash verification.

- The user confirmed the animated pet was visible and both physical buttons worked in the first version.
- The expanded version passed all **28 automated device checks**, including a 32-second animation/decay run, simulated reactions, input validation, calibration bounds, 16-entry history rollover, and history/settings persistence across an actual reboot.
- The device test leaves default calibration, a reset pet, and three explicitly simulated samples (0, 45, 85) for browsing.
- **Touch remains unverified on this board.** Resetting GPIO21 and scanning the I2C bus on SDA18/SCL17 returned no device, including at 0x15 and 0x1A. No physical touch event has been observed. This may be the non-touch variant; do not treat the successful rendering or serial navigation tests as evidence that touch works. Both vendor touch-controller families are supported in the source, but need testing on responding hardware.
- All screens can be operated with the buttons. The extended button menu navigation still needs a user check.
- No MQ-3 is wired or sampled; no actual alcohol measurement or sensor calibration has been performed.
