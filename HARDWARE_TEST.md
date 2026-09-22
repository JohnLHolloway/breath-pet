# Hardware verification - 2026-09-22

Tested on a USB-connected LILYGO T-Display-S3: ESP32-S3 revision 0.2, 16MB flash, approximately 8MB PSRAM. Firmware compiled and flashed successfully, including flash hash verification.

The physical-button redesign puts NEXT and OK in a fixed left rail, aligned to the front buttons with USB on the left. GPIO0 (upper) cycles choices; GPIO14 (lower) confirms the large gold action bar. Holds retain Back and Menu. All screens use this convention, including result choices, per-person history, adoption and calibration. Only the selected swimming pet shows its name, avoiding colliding labels.

The final build passed **48 automated device checks**, and the helper restored normal firmware with the saved CHAOS pet intact. Device tests exercise adoption, taken-name skipping, six-player capacity, timed feeding, cooldowns, owner isolation, overload/recovery, persistent stats/history and calibration, malformed input, animation, result navigation, history navigation and new-evening confirmation. The on-device self-test checks stat bounds, capped rewards and the 16-record history ring.

Tests now run under a separate `breath-test` NVS namespace. The test script checks `test_mode` before clearing test data, and `dev.ps1 test` restores normal firmware in a `finally` block. Failed diagnostic runs verified this restoration path; the normal saved pet was still present afterward. Tests do not replace the normal evening.

Framebuffer review covered the empty and populated tanks, nickname and species pickers, pet care, feed, overload result, history, menu, calibration and new-evening confirmation. Captures are actual ESP32 rendering, not physical panel readback. Published screenshots use fictional test data and are labeled TEST. Screenshot capture now avoids resetting the board when opening USB and rejects failed navigation commands.

Verification exposed the test reader parsing a partial USB line after a read timeout. It now accumulates fragments until the newline arrives. Firmware status also writes as a single buffered record, with a short USB transmit-lock wait instead of dropping writes immediately when busy.

The user previously confirmed the physical display and both buttons work. The new physical placement is based on the vendor pin map; the new mapping still needs the user's hands-on check. No touch controller responds after reset, including a full I2C scan. The supplied listing depicts the standard board and does not advertise touch; evidence points to the non-touch variant.

No MQ-3 has been wired, read or calibrated. Eight of the user's 2 kohm resistors can form the proposed 8 kohm/8 kohm divider; see MQ3_WIRING.md. All current readings and calibration remain simulated game input.
