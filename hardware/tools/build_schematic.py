"""Create the editable, self-contained Rev A KiCad schematic from design.json."""
from pathlib import Path
import json
import uuid

ROOT = Path(__file__).resolve().parents[1] / 'rev-a'
NS = uuid.UUID('7b3b9891-b3ae-4ea5-a5b0-5dd1c06624d0')
uid = lambda name: str(uuid.uuid5(NS, name))
q = lambda value: json.dumps(str(value))
SCH_ID = uid('schematic')
design = json.loads((ROOT / 'design.json').read_text())
effects = '(effects (font (size 1.27 1.27)))'
symbols = {}
pins = {}


def pin(number, name, x, y, angle, length=2.54):
    return f'(pin passive line (at {x} {y} {angle}) (length {length}) (name {q(name)} {effects}) (number {q(number)} {effects}))'


def rect(x1, y1, x2, y2):
    return f'(rectangle (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.254) (type default)) (fill (type none)))'


def line(points):
    return '(polyline (pts ' + ' '.join(f'(xy {x} {y})' for x, y in points) + ') (stroke (width 0.254) (type default)) (fill (type none)))'


def symbol(kind, body, pin_defs):
    pins[kind] = {str(n): (x, y) for n, name, x, y, angle in pin_defs}
    symbols[kind] = f'''(symbol "{kind}" (pin_names (offset 0.508)) (in_bom {'no' if kind in ('TP', 'HOLE') else 'yes'}) (on_board yes)
      (property "Reference" "X" (at 0 5 0) {effects})
      (property "Value" "{kind}" (at 0 -5 0) {effects})
      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))
      (symbol "{kind}_0_1" {body})
      (symbol "{kind}_1_1" {''.join(pin(*d) for d in pin_defs)}))'''


horizontal = [('1', '~', -5.08, 0, 0), ('2', '~', 5.08, 0, 180)]
vertical = [('1', '~', 0, 5.08, 270), ('2', '~', 0, -5.08, 90)]
symbol('R', rect(-2.54, 1.016, 2.54, -1.016), horizontal)
symbol('RV', rect(-1.016, 2.54, 1.016, -2.54), vertical)
symbol('C', line([(-2.54, 0.762), (2.54, 0.762)]) + line([(-2.54, -0.762), (2.54, -0.762)]) +
       line([(0, 2.54), (0, 0.762)]) + line([(0, -2.54), (0, -0.762)]), vertical)
symbol('JP', rect(-2.54, 1.27, 2.54, -1.27), horizontal)
symbol('JST', rect(-5.08, 5.08, 5.08, -5.08),
       [('1', '5V', -7.62, 2.54, 0), ('2', 'GND', -7.62, 0, 0), ('3', 'AO', -7.62, -2.54, 0)])
symbol('MQ3', rect(-10.16, 10.16, 10.16, -10.16),
       [('1', 'A', -12.7, 7.62, 0), ('2', 'H', -12.7, 0, 0), ('3', 'A', -12.7, -7.62, 0),
        ('6', 'B', 12.7, 7.62, 180), ('5', 'H', 12.7, 0, 180), ('4', 'B', 12.7, -7.62, 180)])
symbol('CLAMP', rect(-5.08, 5.08, 5.08, -5.08),
       [('1', 'A1', 0, -7.62, 90), ('2', 'K2', 0, 7.62, 270), ('3', 'K1/A2', -7.62, 0, 0)])
symbol('TP', '(circle (center 0 1.27) (radius 1.27) (stroke (width 0.254) (type default)) (fill (type none)))',
       [('1', '~', 0, -2.54, 90)])
symbol('HOLE', '(circle (center 0 0) (radius 1.27) (stroke (width 0.254) (type default)) (fill (type none)))', [])
headers = {'P1': ['3V3', 'GND', 'GND', 'NC', 'GPIO16', 'GPIO21', 'GPIO17', 'GPIO18', 'GPIO44', 'GPIO43', 'GND', 'GND'],
           'P2': ['5V', 'GND', 'NC', 'NC', 'GPIO13', 'GPIO12', 'GPIO11', 'GPIO10', 'GPIO3', 'GPIO2', 'GPIO1', '3V3']}
for kind, names in headers.items():
    symbol(kind, rect(-5.08, 16.51, 5.08, -16.51),
           [(str(i + 1), name, 7.62, 13.97 - i * 2.54, 180) for i, name in enumerate(names)])

lib = '(kicad_symbol_lib (version 20241209) (generator "breath_pet")\n' + '\n'.join(symbols.values()) + '\n)\n'
(ROOT / 'lib/BreathPet.kicad_sym').write_text(lib, encoding='utf-8')
cached = '\n'.join(s.replace(f'(symbol "{kind}"', f'(symbol "BreathPet:{kind}"', 1) for kind, s in symbols.items())
out = [f'''(kicad_sch (version 20250114) (generator "breath_pet")
 (uuid "{SCH_ID}") (paper "A3")
 (title_block (title "Breath Pet - T-Display-S3 USB carrier") (date "2026-09-22") (rev "A")
   (comment 1 "Unbuilt prototype - USB only - direct MQ-3 or module assembly variants"))
 (lib_symbols {cached})''']
layout = {'J2': (43.18, 60.96), 'J1': (43.18, 132.08), 'JP1': (116.84, 45.72),
          'J3': (248.92, 48.26), 'R1': (116.84, 93.98), 'R3': (160.02, 93.98),
          'R2': (139.7, 121.92), 'C1': (185.42, 121.92), 'C2': (167.64, 60.96),
          'C3': (198.12, 60.96), 'D1': (236.22, 121.92),
          'TP1': (96.52, 154.94), 'TP2': (124.46, 154.94), 'TP3': (152.4, 154.94),
          'TP4': (180.34, 154.94), 'H1': (220.98, 154.94), 'H2': (241.3, 154.94), 'H3': (261.62, 154.94)}
layout.update({'S1': (335.28, 60.96), 'R4': (363.22, 109.22),
               'TP5': (96.52, 208.28), 'TP6': (147.32, 208.28), 'TP7': (198.12, 208.28), 'TP8': (248.92, 208.28)})
coords = {}
seq = 0


def wire(a, b):
    global seq
    seq += 1
    out.append(f'(wire (pts (xy {a[0]:.4f} {a[1]:.4f}) (xy {b[0]:.4f} {b[1]:.4f})) (stroke (width 0.1524) (type default)) (uuid "{uid("wire"+str(seq))}"))')


def label(name, xy):
    global seq
    seq += 1
    out.append(f'(label {q(name)} (at {xy[0]:.4f} {xy[1]:.4f} 0) (effects (font (size 1.016 1.016)) (justify left bottom)) (uuid "{uid("label"+str(seq))}"))')


def note(value, xy, size=1.27):
    out.append(f'(text {q(value)} (at {xy[0]} {xy[1]} 0) (effects (font (size {size} {size})) (justify left top)) (uuid "{uid("text"+value)}"))')


for c in design['components']:
    ref, kind = c['ref'], c['kind']
    x, y = layout[ref]
    # Header labels above the tall symbol; vertical components get labels on right.
    if kind in ('P1', 'P2'):
        rx, ry, vx, vy = x, y - 21.59, x, y - 19.05
    elif kind in ('C', 'RV'):
        rx, ry, vx, vy = x + 12.7, y - 1.27, x + 12.7, y + 1.27
    else:
        rx, ry, vx, vy = x, y - (17.78 if kind == 'MQ3' else 10.16 if kind in ('CLAMP', 'JST') else 6.35), x, y - (15.24 if kind == 'MQ3' else 7.62 if kind in ('CLAMP', 'JST') else 3.81)
    out.append(f'''(symbol (lib_id "BreathPet:{kind}") (at {x} {y} 0) (unit 1)
      (in_bom {'no' if kind in ('TP', 'HOLE') else 'yes'}) (on_board yes) (dnp {'yes' if c.get('dnp') else 'no'}) (uuid "{c['uuid']}")
      (property "Reference" "{ref}" (at {rx} {ry} 0) {effects})
      (property "Value" {q(c['value'])} (at {vx} {vy} 0) (effects (font (size 1.016 1.016))))
      (property "Footprint" "BreathPet:{c['footprint']}" (at {x} {y} 0) (effects (font (size 1.27 1.27)) hide))
      {''.join(f'(pin "{n}" (uuid "{uid(ref+"pin"+n)}"))' for n in pins[kind])}
      (instances (project "carrier" (path "/{SCH_ID}" (reference "{ref}") (unit 1)))))''')
    for number, (px, py) in pins[kind].items():
        xy = (x + px, y - py)
        coords[ref, number] = xy
        net = c['nets'].get(number)
        if net:
            if (ref, number) in [('R1', '2'), ('R3', '1'), ('R3', '2'), ('R2', '1'), ('C1', '1')]:
                continue
            if kind in ('P1', 'P2'):
                end = (xy[0] + 10.16, xy[1])
            elif kind in ('C', 'RV', 'TP') or (kind == 'CLAMP' and number != '3'):
                end = (xy[0], xy[1] + (5.08 if py < 0 else -5.08))
            else:
                end = (xy[0] + (5.08 if px > 0 else -10.16), xy[1])
            wire(xy, end)
            label(net, end)
        else:
            out.append(f'(no_connect (at {xy[0]:.4f} {xy[1]:.4f}) (uuid "{uid(ref+"nc"+number)}"))')

wire(coords['R1', '2'], (139.7, 93.98))
wire((139.7, 93.98), coords['R3', '1'])
wire((139.7, 93.98), coords['R2', '1'])
label('DIV_HALF', (139.7, 93.98))
wire(coords['R3', '2'], (185.42, 93.98))
wire((185.42, 93.98), coords['C1', '1'])
label('ADC_GPIO1', (185.42, 93.98))
for name, xy in [('divider', (139.7, 93.98))]:
    out.append(f'(junction (at {xy[0]} {xy[1]}) (diameter 0) (color 0 0 0 0) (uuid "{uid(name)}"))')
note('USB POWER + SENSOR CABLE', (88.9, 25.4), 1.8)
note('JP1 shunt fitted for normal operation; remove to isolate sensor power.\nJ3 is OPTIONAL: fit only when S1 and R4 are not installed.\nJ3 cable order is 5V / GND / AO; leave module DO open.', (88.9, 70), 1.1)
note('ANALOG INPUT: 1/2 SCALE + RC + RAIL CLAMP', (88.9, 81.28), 1.6)
note('BAT54S: pin 1 = A1 (GND), pin 2 = K2 (3V3), pin 3 = junction.\nProtection is supplemental; never bypass the divider or feed AO externally.', (93.98, 139.7), 1.016)
note('J1 is mechanical only; all 12 pins intentionally isolated.\nHeader pin 1 is nearest USB. Top PCB view: USB left, P1 above P2.\nNormal firmware already uses GPIO1; no firmware change required.', (20.32, 173.99), 1.016)
note('R1/R2: matched nominal values, 1% tolerance.\nC1 reservoir: ~0.51 ms nominal RC with R3 + R1||R2.\nUSB only: the LILYGO 5V pin is not a battery boost supply.', (170.18, 173.99), 1.016)
note('DIRECT SENSOR / ASSEMBLY VARIANTS', (289.56, 25.4), 1.6)
note('Default: R4 = 6.65k; effective load = R4 || (R1 + R2) = 4.73k.\nThis is a starting point for a low-resistance MQ-3.\nDo not assume all cans marked MQ-3 are electrically identical.\nSee README variant table before buying parts.', (289.56, 134.62), 1.1)
note('S1: verify 9.5 mm pin circle and A/H/B connections on donor.\nThe mirrored A/B groups and heater polarities are interchangeable.\nUse either S1 or J3, never both. J3 is unpopulated by default.\nR4 is unpopulated in the external-module variant.', (289.56, 165.1), 1.1)
note('ACCESSIBLE SENSOR-WING TEST PADS', (88.9, 190.5), 1.6)
note('Re-baseline and characterize the new sensor before changing game thresholds.\nThis circuit measures a relative gas response. It does not measure blood alcohol.', (88.9, 233.68), 1.27)
out.append(')\n')
(ROOT / 'carrier.kicad_sch').write_text('\n'.join(line.rstrip() for line in '\n'.join(out).splitlines()) + '\n', encoding='utf-8', newline='\n')
print('Saved native schematic and local symbol library')
