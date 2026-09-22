# Hardware sources and provenance

Checked 2026-09-22. Measurements from CAD are nominal; they do not measure the user's physical parts.

## LILYGO

- [Product page](https://lilygo.cc/en-us/products/t-display-s3) and [board wiki](https://wiki.lilygo.cc/products/t-display-series/t-display-s3/).
- [Vendor repository](https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/ec889e789b3cf093412689a143f7f37b42b56af7), pinned commit `ec889e789b3cf093412689a143f7f37b42b56af7`.
- The vendor `T_Display_S3.pdf` schematic establishes P1/P2 pin assignments. Its board STEP, full STEP, pinout image and `SD-Shield.DXF` establish orientation and geometry. The reference full STEP is redistributed unchanged as [reference/t-display-s3-full.step](reference/t-display-s3-full.step), with [vendor MIT license](reference/LILYGO-LICENSE.txt).
- Header pitch **2.54 mm** and row spacing **22.86 mm** are present in the shield drawing and consistent with STEP hole centers. In the source STEP the rows lie near x=-11.4404 and +11.4196; first pin is near y=24.0032 and USB near y=0. Source board thickness is 1.2 mm. Source rear components rise about 3.5 mm from its back surface.
- Carrier coordinates are intentionally not a back-view tracing: **display faces up, USB left, P1 above P2**. The CAD transformation is encoded in `tools/build_mechanical.py`. The model's tiny row-y difference is below one micron after aligning to the nominal grid.
- J2/P2 pins 1/2/11/12 are 5V/GND/GPIO1/3V3. No assumption is made that the header's 5 V rail is a regulated 5 V supply when running from a single-cell battery.

## MQ sensor references

- [Hanwei MQ-3 manufacturer datasheet, hosted by SparkFun](https://cdn.sparkfun.com/assets/6/a/1/7/b/MQ-3.pdf): classic 9.5 mm pin-circle geometry, 45-degree A/B locations, package alternatives and older 200 kΩ load circuit. The project custom footprint follows that geometry with enlarged 1.2 mm finished holes. The donor's pin geometry still needs a physical fit check.
- User measurement on 2026-09-22: orange sensor outside diameter **16.7 mm**, installed height **9.82 mm above the blue module PCB**. These measurements drive the body envelope, compact carrier and case opening. The rendered underside gap and mesh diameter are illustrative, not separately measured.
- [Winsen MQ-3 v1.3 manual, hosted by SparkFun](https://cdn.sparkfun.com/datasheets/Sensors/Biometric/MQ-3%20ver1.3%20-%20Manual.pdf): 5 V heater, up to 900 mW, example curves with a 4.7 kΩ load, conditioning and handling guidance. Its package drawing differs from the classic large can: it does **not** prove donor-footprint compatibility.
- [Winsen MQ-3B product](https://www.winsen-sensor.com/product/mq-3b.html) and [manual](https://www.winsen-sensor.com/d/files/manual/mq-3b.pdf) provide another current variant, not an identification of the ACEIRMC module's can.
- The custom sensor footprint uses heater pins 2/5 and electrode pairs 1/3 and 4/6. Top/bottom-view mirroring exchanges the equivalent electrode groups and heater polarity, but the heater axis must still match. Verify continuity on the actual donor with power off.

None of these manuals establishes BAC calibration or interchangeability between unidentified modules. The selected default load is an engineering starting point for bench characterization.

## Other parts / formats

- [Nexperia BAT54S datasheet](https://assets.nexperia.com/documents/data-sheet/BAT54S.pdf): pin 1=A1, pin 2=K2, pin 3=K1/A2. The carrier's series-diode clamp follows that pinout.
- [JST XH series datasheet](https://www.jst-mfg.com/product/pdf/eng/eXH.pdf): optional B3B-XH-A connector uses **2.50 mm** pitch, not 2.54 mm.
- Standard footprints are vendored from [KiCad/kicad-footprints](https://github.com/KiCad/kicad-footprints/tree/7ebfa6b23cc292a56f751b7b5f4a0e12eeef69dd), commit `7ebfa6b23cc292a56f751b7b5f4a0e12eeef69dd`. [ORIGIN.json](rev-a/lib/ORIGIN.json) lists paths; [LICENSE.md](rev-a/lib/LICENSE.md) retains KiCad's CC-BY-SA terms and board-design exception. The MQ footprint and project symbols are original project work under the root MIT license.
- [KiCad CLI documentation](https://docs.kicad.org/10.0/en/cli/cli.html) and [file formats](https://dev-docs.kicad.org/en/file-formats/sexpr-intro/index.html). The release was exported and checked by KiCad **10.0.6**.
- [JLCPCB BOM requirements](https://jlcpcb.com/help/article/bill-of-materials-for-pcb-assembly) and [assembly FAQs](https://jlcpcb.com/help/article/pcb-assembly-faqs). The BOM/CPL are prepared for part matching; no bare-MQ-3 inventory, assembly quote or production commitment has been verified.

The repository contains no proprietary factory flash image, photos of private hardware setups, personal sample readings or downloaded manufacturer PDFs. All geometry/previews here are source-derived CAD or original design work.
