# JLCPCB prototype ordering packet

**This is an unbuilt prototype. Confirm sensor geometry and assembly variant before fabrication.** The Gerber packet is complete; the assembly BOM/CPL are prepared for manual part selection. No JLC quote, stock reservation, upload or paid order has been made.

## Recommended first batch

Use **JLCPCB for the bare PCB and optionally the eight small SMT parts**, then solder the display sockets, power jumper and a verified bare sensor yourself. No bare MQ-3 assembly-library match has been established. A complete MQ-3 module is not interchangeable with the six-pin can footprint. Full factory assembly may be possible after identifying and sourcing an exact sensor, but it is not included in this packet.

The existing five-pack provides donors, subject to package/electrical checks and successful desoldering. Keep one module intact for comparison. A fresh bare can with a reliable datasheet is preferable for a repeatable second revision.

## PCB settings

Upload [breath-pet-rev-a-gerbers.zip](breath-pet-rev-a-gerbers.zip) to the PCB quote tool and inspect the preview:

- **2 layers**, FR-4, **68 × 70 mm**, **1.6 mm** thickness, **1 oz copper**.
- Five individual boards are a reasonable prototype quantity. No panelization is included.
- Standard green solder mask and lead-free HASL are sufficient design choices; ENIG is optional.
- Separate plated and non-plated drill files are included. Three 2.7 mm NPTH mounting holes and six 1.2 mm sensor holes should be visible.
- Gerbers, drills and placements share the same upper-left origin. The manufacturing Y coordinates are negative below that origin. Do not mirror a layer or manually change one file's origin.
- The clear region at the top right is the display antenna keepout. Do not add copper, metal fixings, a factory order number, or assembly tooling there without review.

The ZIP contains both copper layers, both masks, both silkscreens, the outline, front paste, two drill files and a Gerber job file. The native PCB is also provided for the manufacturer's engineering review if required. The actual checkout price includes manufacturing, assembly setup, components, shipping and any taxes; no price estimate here is an order quote.

## SMT assembly

Choose **top-side SMT assembly**. Upload [jlc-bom-direct.csv](jlc-bom-direct.csv) and [jlc-cpl-direct.csv](jlc-cpl-direct.csv) for the default direct-can variant.

The BOM's LCSC-number column is deliberately blank: **choose and verify each actual catalog part before ordering**. It specifies 0805 imperial / 2012 metric passives and a SOT-23 **BAT54S dual-series** diode. Do not substitute BAT54A or BAT54C, which connect the diodes differently. C2 may be 16 V or higher voltage rating if its dimensions and effective capacitance are suitable. All resistors are 1%, at least 0.125 W.

These files place only **R1–R4, C1–C3, D1**. No sensor, socket, test pad or mounting hole belongs in the SMT pick-and-place list. J3 is DNP in the direct-sensor build. JP1, J1/J2 and S1 are hand assembled. Select the [sensor/load variant](../../README.md#sensor-choice-and-load-resistor) before submitting the BOM; the supplied values are not universal across MQ-3 cans.

### Placement review

CPL rotations are KiCad's native rotations, not a guarantee of the assembly library's zero-angle convention. Verify the factory preview before accepting it:

| Part | Top-view physical pad positions in mm from upper-left origin |
| --- | --- |
| D1 pin 1, **GND / A1** | x=51, y=-34.05 |
| D1 pin 2, **3V3 / K2** | x=51, y=-35.95 |
| D1 pin 3, **ADC / common** | x=53, y=-35 |

In the PCB's top view with USB to the left, D1 has its two-pin side to the left and single-pin side to the right. The passive components have no polarity. Check centers as well as orientation. JLC tooling rails/fiducials and any handling constraints need to be resolved in the manufacturer's review; this is a single-board layout, not an assembly panel.

For the external-module variant, omit **S1 and R4**, populate J3, and remove R4 from BOM and CPL. The cable's carrier-side order is **1=5V, 2=GND, 3=AO**, at 2.50 mm JST-XH pitch. Re-pin the module end to its printed labels; do not assume the module uses that order. Leave DO disconnected. Never attach an external module while a bare S1 is fitted.

## Before approving an order

Check the printed gauges, exact can/electrode layout, chosen resistor variant, socket height and board preview. Match every assembly part and review the BAT54S placement. If a factory question changes a footprint or connection, update the native design and regenerate all files together. A passed CAD check is not a substitute for the first physical prototype.

JLCPCB's current file guidance: [BOM](https://jlcpcb.com/help/article/bill-of-materials-for-pcb-assembly), [component placement file](https://jlcpcb.com/help/article/pick-place-file-for-pcb-assembly), [assembly FAQs](https://jlcpcb.com/help/article/pcb-assembly-faqs). Consult the quote portal for current options and availability.
