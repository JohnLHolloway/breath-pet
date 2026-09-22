# Hardware verification

Status: **2026-09-22, firmware version 7**. [Project home](README.md) · [Development](docs/DEVELOPMENT.md) · [Current gallery](docs/screenshots/README.md)

## Current evidence

| Area | Evidence | Scope |
| --- | --- | --- |
| Build and flash | Normal and isolated test firmware built/uploaded successfully; upload flash hashes verified | LILYGO T-Display-S3, ESP32-S3 revision 0.2, 16 MB flash, 8 MB PSRAM |
| Device integration suite | **99 / 99 checks passed**, run timestamp `2026-09-22T17:48:02Z` | Latest firmware source; includes synthetic ADC and accelerated game time |
| Firmware self-test | Passed on normal firmware | Rule boundaries, migration, stat limits and history rollover |
| Physical display/buttons | User confirmed the displayed pet and both buttons | Current screen layout was additionally reviewed via framebuffer exports |
| Touch | No controller responded to probes or a full I2C scan | Operation on a touch-equipped variant remains unverified |
| Physical MQ-3 response | Cup vapor caused a rise and recovery; real-source history was saved | Qualitative response only, not concentration/BAC calibration |
| Background baseline | Physical sensor started a queued countdown at a 113 mV baseline; a subsequent request started immediately | Both attempts cancelled before BLOW, without creating history |
| Current gallery | **20 fresh device framebuffer captures**, fictional test pets and injected ADC | All images reviewed; no real player readings published |
| Normal restoration | After gallery capture, normal firmware reported LIVE, `test_mode: false`, and healthy game/collection saves | Roster, all eight existing history entries, sensitivity and collected/equipped items matched private before/after snapshots |

The documentation refresh did not modify firmware source. The 99-check result predates the refresh; it is not presented as a new full-suite run. Both firmware environments were rebuilt/uploaded during gallery capture, and restoration/data-preservation checks ran afterward. Private serial reports and normal snapshots remain in ignored local artifacts.

## Automated coverage

The suite drives the actual navigation handlers and checks:

- Adoption, unique nicknames, capacity, menu selections, owner isolation and confirmation before clearing an evening.
- Five seconds of GET READY followed by ten seconds of BLOW, excluding countdown exposure from the peak.
- Background rolling baselines, immediate Feed, queued recovery, cancellation, explicit retry after invalid input and sample-count/voltage rejection.
- Source labeling and original sensor metadata, calibration/history persistence, malformed commands and animation progress.
- Zero-response wakeups, no high-score damage, DROWSY/ASLEEP boundaries, mini-game misses and three-catch wins.
- Reward cooldowns, cup/hat rewards, wardrobe, shared upgrades, awards, returning-name colours and collection persistence.

Test saves use `breath-test` and `fun-test`; normal saves use `breath-pet` and `pet-fun`. Synthetic ADC, accelerated time and bubble-position controls exist only in test firmware. Normal firmware previously rejected the test-only time and bubble commands. The test and gallery helpers attempt to restore normal firmware even after a failure; a disconnected board still needs reconnection and a successful upload.

## Screen verification

The [gallery](docs/screenshots/README.md) replaces screenshots from earlier interfaces. It covers adoption, tank states, pet belongings, wardrobe, bubble catch, countdown, capture, result/history, recovery, menu, sensitivity, bench diagnostics, awards, upgrades and new-evening confirmation. Every selectable page uses the same physical-button rail and gold action bar. The pet page has one energy meter; tank labels show each pet's state.

These are **rendered framebuffer exports**, not photos or physical panel readback. The TEST badge identifies isolated firmware. Even when an image says MQ3, the gallery's sensor input is injected; it is not a human reading. [The manifest](docs/screenshots/manifest.json) records the capture date, firmware version, source revision and binary/image hashes. Original source for this capture is commit `28a285d`.

## Persistence

The current save layout stores party/history data and collection/progress data separately. An earlier low-space condition in the 20 KB NVS partition was fixed by removing only obsolete `party-v1` after a valid `party-v2` record is loaded. Existing v2 pets/history were preserved; no full NVS erase was performed. Save health, restart behavior and collection persistence passed subsequent checks.

The gallery utility compares normal saved fields and exact histories before and after flashing its fixture, without copying private readings into public documentation. Up to 32 nickname collections and shared upgrades survive starting a new evening; the active roster and its history do not.

## Physical limits and next checks

The [Rev A carrier PCB and printed case](hardware/README.md) are a separate, **unbuilt** hardware prototype. KiCad ERC/DRC/parity checks, netlist assertions, manufacturing exports and nominal CAD collision checks pass; no fabricated carrier or printed enclosure has been tested. These CAD checks do not extend the breadboard's 99-check firmware result to the new hardware. Donor-can identification, fit gauges, component matching, meter checks, cup response and heat testing remain.

The planned assembly uses 5 V module power, common ground and two 8 kΩ resistor chains between AO, GPIO1 and GND. **Divider voltages have not been independently verified with a meter.** See the [wiring checks](MQ3_WIRING.md#verify-the-divider).

Earlier cup testing observed approximately 107 mV in clean air, 414 mV during exposure to sake vapor and 106 mV after removal. Subsequent live feeding also recorded higher responses and stored the original baseline/peak values. This supports qualitative response, recovery and persistence, not a calibrated alcohol measurement.

The current full five-plus-ten-second workflow was checked with injected ADC values. A fresh user-operated physical cup capture through that complete workflow remains to be checked. The physical background-readiness tests above were deliberately cancelled before sampling. A completed quiet capture is accepted by design, and the MQ-3 cannot prove airflow or identify who supplied a sample. Touch hardware, battery-powered sensor operation, long-duration sensor stability and BAC accuracy are not verified.
