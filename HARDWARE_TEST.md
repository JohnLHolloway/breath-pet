# Hardware verification - 2026-09-22

Tested on a USB-connected LILYGO T-Display-S3: ESP32-S3 revision 0.2, 16MB flash and approximately 8MB PSRAM. Normal firmware version 4 compiled, flashed with hash verification, and booted successfully.

## Controls and display

With USB on the left, GPIO0 (upper front button) cycles NEXT and holds for Back; GPIO14 (lower) confirms OK and holds for Menu. Every selectable page uses the same gold action bar and position counter. The tank animates freely swimming pets and labels only the selected pet. The user previously confirmed the physical panel and both buttons work; latest layouts were reviewed through exported framebuffers, not physical panel readback.

Framebuffer review covers the tank, adoption, care, menus, calibration, history and confirmation pages, plus the new live-feed preparation, timed sampling, result and mixed-source history screens. New captures in `docs/screenshots/` use fictional isolated test data and are labeled TEST.

No touch controller responded after reset or a full I2C scan. The supplied listing depicts the standard board without advertised touch; evidence points to the non-touch variant.

## Automated and persistence checks

The isolated device suite passed **69 checks**. It covers adoption, taken-name skipping, six-player capacity, timed feeding, cooldowns, owner isolation, overload/recovery, persistent history/calibration, malformed input, animation, menus, confirmation and ADC bench behavior. Live-feeding checks use synthetic ADC injection compiled only into the test build: fresh baseline gating, timed peak capture, source and voltage metadata, cancellation, voltage rejection, recovery shared across owners, and persistence after restart. Normal firmware rejects the injection command.

The on-device self-test also passed on normal firmware, including legacy schema migration, stat bounds, score limits, timing, recovery guards and history rollover. Actual legacy saved data migrated successfully: the existing pet's name, type, stats, feed count and all original history fields were preserved; the old reading gained an explicit DEMO label. Normal data was never cleared. A subsequent firmware update preserved that migrated history again.

After the suite, a small serial-calibration mapping correction was verified directly on normal firmware: minus changed 600 to 500 mV; plus restored 600; default restored 600 after another decrement; manual zero was rejected without changing settings. The normal self-test was repeated successfully. These scoped checks supplement the 69-check suite.

Tests use the separate `breath-test` NVS namespace. The test script checks `test_mode` before clearing its data, and `dev.ps1 test` restores normal firmware in a `finally` block. USB replies are transmitted in bounded chunks; clients accumulate partial lines and correlate request IDs. Missing replies are recovered only by read-only queries, never by replaying a feeding action. The successful 69-check run needed no read-only reply recovery.

## Physical sensor evidence and remaining check

The MQ-3 is connected to GPIO1 through the planned 8 kohm/8 kohm divider made from eight 2 kohm resistors, with 5 V power and common ground. The user did not perform the requested multimeter check, so divider wiring and voltage are not independently verified.

A real cup-vapor bench test observed approximately 107 mV in clean air, 414 mV after exposure to 25% sake, and 106 mV after removal. The user confirmed exposure and return to clean air. This establishes qualitative response and recovery, not alcohol concentration or BAC calibration.

Normal firmware now feeds from the real ADC. Its clean-air preparation was verified around 112-114 mV with a quiet window and readiness enabled. The complete physical cup-to-pet feeding remains pending a user-started capture. The automated live-feed tests establish game behavior with synthetic readings; they do not substitute for that physical end-to-end check.

Each live feeding freezes a fresh clean-air baseline, observes 12 seconds of peak voltage, stores MQ3 provenance and the original baseline/peak/span, and then requires recovery before another feeding. The bench monitor remains separate and never writes pet history. All displayed scores are game units, never BAC.
