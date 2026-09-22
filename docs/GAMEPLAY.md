# Gameplay guide

[Project home](../README.md) · [Screen gallery](screenshots/README.md) · [Wiring](../MQ3_WIRING.md)

## Adopt a pet

An evening starts with an empty tank. Choose Add a pet, an unused nickname, then a species. Add people individually, up to six. NEXT cycles; OK confirms. The tank selector cycles through occupied pets, Add a pet if space remains, and Menu. A gold ring marks the selected swimmer, and the bottom bar gives its name and full state. All pets move in the same tank.

The built-in nickname picker offers Captain, Goose, Bean, Chaos, Pickle, Nugget, Bubbles, Spud, Mochi, Gremlin, Waffles, Noodle, Goblin, Peach, Squid and Biscuit. Used names are skipped. Species are Blob, Axolotl, Bat, Cat, Ghost and Frog. Custom 1–8-character alphanumeric names can be added through USB; there is no on-device keyboard.

## Feed and read a reaction

In LIVE mode, choose a pet and press OK on Wake / feed:

1. **GET READY — 5 seconds.** The recent clean-air average is frozen as the baseline. This phase does not collect a peak.
2. **BLOW — 10 seconds.** Provide the sample now. The highest accepted reading in this window sets the level; the screen shows a response bar.
3. **Result.** See the game level and reaction, then return to the tank or visit the pet. One completed sample adds one entry to that pet's history.

The sensor runs in the background on every LIVE screen. The game uses the nominal clean-air band to decide readiness and the recent average for the actual baseline; it does not assume a fixed voltage. Normally there is no separate clean-air step. If Feed is requested before the signal settles, Sensor recovering waits and automatically starts the countdown once ready. OK or Back cancels that pending request. After power-up or switching to LIVE, the first window takes at least ten seconds.

Hold the upper button to cancel a countdown or capture; hold the lower button to open the menu. Cancelling saves no reading. Invalid captures show an explicit retry action and do not restart themselves. A five-second feed cooldown prevents repeated submissions; previously fed pets also have this short cooldown after reboot.

In DEMO mode, Wake / feed first offers fake values 0, 25, 50 and 85, then uses the same countdown and capture timing. Choose Change input mode in the evening menu to switch. Normal firmware returns to LIVE after reboot.

### Game sensitivity

These are provisional **game thresholds**, not BAC or intoxication estimates. All millivolt values refer to **GPIO1 after the divider**, not the module's AO pin.

| Setting | Current behavior |
| --- | --- |
| Clean-air readiness | Rolling 100-sample window, at least 10 seconds; every reading 50–250 mV; spread at most 25 mV |
| Baseline | Mean of that window, frozen when GET READY begins |
| Response gate | Peak below 400 mV gives level 0 |
| Default response span | 1200 mV; saved custom settings survive firmware updates |
| Response settings | Change span by 100 mV, within 100–2000 mV; a smaller span gives a higher level for the same rise |
| Capture rejection | Any BLOW sample below 20 or above 2700 mV, or fewer than 80 acquired samples |

For a peak of at least 400 mV:

```text
level = clamp(max(0, peak_mV - baseline_mV - 20) * 100 / span_mV, 0, 100)
```

For example, baseline 100 mV and peak 850 mV at the default span yield level 60 (integer arithmetic). The 400 mV gate is a minimum response gate, not the start of PARTY: a 400 mV peak with a 100 mV baseline gives level 23 / CHILL.

A quiet completed capture is deliberately accepted as level zero and wakes the pet. The sensor does not detect airflow or establish that someone blew. A cup test demonstrates response and recovery, not concentration calibration. [Wiring and bench testing](../MQ3_WIRING.md) explain the separate diagnostic baseline.

## State and energy

| State | Trigger and appearance |
| --- | --- |
| CHILL | Level 0–24: gentle swimming and bubbles |
| PARTY | Level 25–69: livelier swimming and more bubbles |
| WILD | Level 70–100: zooming, winking, wobbling and sparkles |
| DROWSY | 10 powered minutes since the last feeding or completed game |
| ASLEEP | 20 powered minutes, or Take a nap: closed eyes and Zzz on the tank floor |

A completed sample of **any level**, or a completed bubble game, resets idle time, restores energy to 100 and plays a wake-up animation. Energy decreases by five per powered minute. Sleep overrides the reaction label until the pet wakes. There is no neglect damage or high-reading penalty. Timers pause when powered off; they are not a wall clock. Take a nap has a ten-second button cooldown.

The tank shows each pet's compact state label, with Zzz for sleepers. The pet page shows its personality, equipped hat/item/colour and **one energy bar**. Old health, food and joy fields remain in storage and USB diagnostics for compatibility; they are no longer separate care meters in the UI.

## Pet actions

| Action | What it does |
| --- | --- |
| Wake / feed | Run a LIVE capture or choose a DEMO value |
| History | Browse the pet's last 16 readings, three per page; NEXT changes page, OK returns |
| Take a nap | Put the pet to sleep immediately |
| Wardrobe | NEXT selects Hats, Hand items, Colours or Back; OK cycles unlocked choices |
| Play bubble catch | Press OK while the moving bubble is in the gold zone; three catches win and wake the pet; NEXT selects Back |

History on the device shows level, source, reaction and age. USB history also includes original baseline, peak and response span for MQ3 entries. Old DEMO entries remain DEMO and old saved health deltas are preserved. Changing sensitivity does not rewrite earlier scores. A bubble-game win does not create a sensor-history entry.

## Clothes and personalities

A completed feeding or bubble game earns at most **one rewarded check-in per pet per ten powered minutes**. Extra play or feeding still wakes the pet but earns no extra loot. Sensor level never determines reward eligibility or which item is awarded.

- If the nickname has never collected a red cup, its first rewarded check-in supplies and equips one.
- Later eligible check-ins choose a random unowned accessory. The third rewarded check-in of an evening guarantees a hat if any remain locked. New accessories equip automatically.
- Hats: party cone, cowboy hat, crown, sunglasses and top hat. Hand items: red cup, pizza slice, floatie and bubble wand. Wardrobe also allows no hat or no hand item.
- Returning with the same nickname on evenings two, three and four unlocks sunshine, lilac and ocean colours. Rebooting is not a new evening. Up to 32 nickname collections are retained.

Personalities are stable by nickname. Captain leads parades, Goose plays hat pranks, and Bean is a shy cup buddy; other names get repeatable personalities. Non-sleeping pets periodically gather for a greeting or parade. A borrowed visual hat does not transfer inventory. Friendly encounters accumulate for a pair every two powered minutes when at least two pets are not ASLEEP; DROWSY pets can participate.

## Shared tank and evening menu

Shared rewarded check-ins unlock the jukebox at **2**, pirate ship at **5**, and disco ball at **8**. These totals persist across evenings. An eligible check-in can unlock the disco ball early and trigger 15 seconds of confetti when there are at least two pets, every pet has checked in this evening, and everyone has been active within the last ten powered minutes.

| Evening menu option | Purpose |
| --- | --- |
| Back to tank | Return to the shared aquarium |
| Response settings | Adjust game sensitivity or restore the default; DEMO has separate fake-value zero/span settings |
| Start new evening | Confirm before clearing the current roster and readings; Keep is the default |
| MQ-3 setup | Inspect voltage, ADC counts, trend and a temporary diagnostic baseline |
| Change input mode | Switch LIVE / DEMO for this boot |
| Evening awards | NEXT cycles Best Dressed, Most Naps and Social Butterfly; OK returns |
| Tank upgrades | NEXT browses the three shared decorations; OK returns |

Best Dressed counts all owned cosmetics, including those collected on previous evenings. Most Naps and Social Butterfly use this evening's naps and friendly encounters. Ties are identified. Awards update during play; view them before starting a new evening. There is no automatic awards archive.

## What gets saved

Pets, their individual histories, sensitivity settings, collected/equipped items, current party stats and shared upgrades survive restart. A new evening clears active pets, their readings and evening stats but retains nickname collections and shared decorations. Removing power does not simulate time passing. The LIVE/DEMO choice and temporary bench zero are not retained across reboot.
