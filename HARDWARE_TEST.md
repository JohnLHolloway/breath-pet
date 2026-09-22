# Hardware verification - 2026-09-22

Firmware version 7 was built and flashed to the LILYGO T-Display-S3, ESP32-S3 revision 0.2, 16MB flash and 8MB PSRAM. Flash hashes verified. Normal firmware boots successfully with both game and collection saves healthy.

## Validation

The isolated device suite passed **99 checks**. Coverage includes adoption and nickname ownership, six-pet capacity, persistent history/calibration, malformed commands, animation, menu navigation, clean-air readiness and sensor rejection. The new party checks cover:

- A five-second countdown followed by a ten-second sampling window; early exposure does not contribute to the peak.
- Real-source history metadata, cancelled attempts, invalid voltage and isolation between owners.
- No damage from high scores; zero-response check-ins also wake pets.
- Awake/drowsy/asleep boundaries at 10 and 20 powered minutes.
- Mini-game misses, three-catch wins, wakeup without a history entry, and the ten-minute loot cooldown.
- First cup, third-check-in hat, independent wardrobe controls and shared decorations.
- Awards and upgrades navigation, saved clothing/progress across restart, returning-name colour unlock and new-evening behavior.

Tests use separate game and collection namespaces (`breath-test`, `fun-test`). Test-only ADC injection, accelerated time and bubble-position commands are absent from normal firmware. The normal build explicitly rejected time/bubble test commands. `dev.ps1 test` restores normal firmware even after failure.

Screen layout, status labels and hiding diagnostic values in gameplay were also checked using actual exported ESP32 framebuffers. The final normal self-test passed, both save areas reported success, and all five existing user history entries matched the pre-update snapshot exactly. No synthetic readings were written to normal history.

## Display and controls

Upper front GPIO0 cycles NEXT (hold Back); lower GPIO14 confirms OK (hold Menu), with USB on the left. Every selectable page shares the gold action bar. The user previously confirmed the physical panel and buttons work; current layout QA uses rendered framebuffers, not physical panel readback.

The latest captures in `docs/screenshots/party-*.png` use fictional test pets. They cover tank states, sleeping pets, wardrobe, outfit detail with one energy bar, bubble catch, awards, countdown, BLOW meter, results and history. Sleeping tank labels use compact Zzz; the selected pet's full name/state stays in the bottom bar. Older live-* captures document the earlier version.

No touch controller responded to probing or a full I2C scan. The listing depicts the standard board without advertised touch; the UI remains fully button-operable.

## Persistence fix

Adding collections initially exposed insufficient free space in the board's 20KB NVS partition. After validating an existing party-v2 record, startup now removes only that namespace's obsolete party-v1 key. This reclaims migrated data without clearing current pets, history or settings. Normal and isolated test saves then passed, including clothing persistence and restart tests. Collections are stored separately under `pet-fun` on normal firmware. Up to 32 nickname collections and shared tank upgrades survive new evenings.

## Physical sensor evidence and limits

The MQ-3 uses GPIO1 and the planned eight-resistor 8 kohm/8 kohm divider, 5 V power and common ground. The user did not perform the requested multimeter check; physical divider voltage has not been independently verified.

A cup-vapor bench test observed approximately 107 mV in clean air, 414 mV exposed to 25% sake, and 106 mV after removal. Later user-started feeds saved actual MQ3 baseline/peak records, including 139/852 mV and 153/804 mV. Subsequent threshold testing recorded additional physical responses successfully. This demonstrates qualitative response, recovery and storage, not BAC calibration.

Version 7 retains the provisional 50-250 mV clean-air window, 400 mV response gate and 1200 mV scale. High levels now change animation rather than damage health. The new five-plus-ten-second capture path was verified with injected ADC values; it has not yet had a fresh user-operated physical cup test. A completed quiet sample is intentionally accepted, and the MQ-3 does not verify airflow or whether a person blew.

## Background recovery revision

Version 7 continuously acquires the ADC in live mode, including the tank, pet, wardrobe and result screens. Feed freezes the existing quiet rolling average and immediately starts the countdown. A requested feeding waits only if the signal has not settled; recovery automatically starts its countdown, while OK or Back can cancel the request. Failed captures require an explicit retry and do not automatically repeat. Demo-mode acquisition remains limited to the bench screen.

The nominal band remains 50-250 mV; its recent average provides the actual baseline. Navigation no longer resets acquisition. Cold start or enabling live mode still needs the first ten-second window. The queued-start path refreshes the loop timestamp after resetting the countdown timer, avoiding unsigned timer underflow at a millisecond boundary.

The revised 99-check suite passed. After the timer adjustment, scoped checks verified the recovery Cancel button and the normal physical sensor automatically starting a queued countdown with a 113 mV baseline. A second request immediately entered countdown from the background window. Both normal-device attempts were cancelled before BLOW; history matched the pre-update snapshot. The normal self-test and both save areas passed. The recovery screen was reviewed through its exported framebuffer.
