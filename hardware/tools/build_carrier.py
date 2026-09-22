"""Generate the Rev A carrier PCB and schematic with KiCad's bundled Python.

Run with KiCad 10's Python (pcbnew available). Geometry is in millimetres.
All sources are local; this script does not download parts or change firmware.
"""
from pathlib import Path
import json
import math
import uuid
import pcbnew as p

ROOT = Path(__file__).resolve().parents[1] / 'rev-a'
LIB = ROOT / 'lib' / 'BreathPet.pretty'
NS = uuid.UUID('7b3b9891-b3ae-4ea5-a5b0-5dd1c06624d0')
uid = lambda name: str(uuid.uuid5(NS, name))
SCH_ID = uid('schematic')
ORIGIN = (100, 80)
mm = p.FromMM
point = lambda x, y: p.VECTOR2I(mm(x + ORIGIN[0]), mm(y + ORIGIN[1]))
board = p.BOARD()
board.SetCopperLayerCount(2)
board.GetDesignSettings().SetBoardThickness(mm(1.6))
board.GetDesignSettings().SetAuxOrigin(point(0, 0))
board.SetFileName(str(ROOT / 'carrier.kicad_pcb'))
title = p.TITLE_BLOCK()
title.SetTitle('Breath Pet - T-Display-S3 USB carrier')
title.SetRevision('A')
title.SetDate('2026-09-22')
title.SetComment(0, 'Prototype - USB powered - verify fit before ordering')
board.SetTitleBlock(title)
nets = {}
for name in ('GND', '+5V_USB', '+5V_SENSOR', '+3V3', 'MQ3_AO', 'DIV_HALF', 'ADC_GPIO1'):
    net = p.NETINFO_ITEM(board, '/' + name)
    board.Add(net)
    nets[name] = net

components = []
fps = {}

# Classic MQ package: 9.5 mm pin circle, A/B pins at 45 degrees.
# Dimensions from Hanwei MQ-3 configuration A/B. The can bought by the user
# has not been measured: a fit gauge is supplied, not a claim of physical fit.
sensor_pads = {'1': (-3.36, -3.36), '2': (-4.75, 0), '3': (-3.36, 3.36),
               '4': (3.36, 3.36), '5': (4.75, 0), '6': (3.36, -3.36)}
sensor_fp = ['(footprint "MQ3_Classic_9.5mm" (layer "F.Cu") (attr through_hole)',
             '(fp_text reference "REF**" (at 0 -11) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
             '(fp_text value "MQ3" (at 0 11) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))']
for layer, radius, width in [('F.Fab', 8.35, .1), ('F.SilkS', 8.5, .12), ('F.CrtYd', 8.85, .05)]:
    sensor_fp.append(f'(fp_circle (center 0 0) (end {radius} 0) (stroke (width {width}) (type default)) (fill none) (layer "{layer}"))')
for number, (x, y) in sensor_pads.items():
    sensor_fp.append(f'(pad "{number}" thru_hole {"rect" if number == "1" else "circle"} (at {x} {y}) (size 2.4 2.4) (drill 1.2) (layers "*.Cu" "*.Mask"))')
sensor_fp.append(')')
(LIB / 'MQ3_Classic_9.5mm.kicad_mod').write_text('\n'.join(sensor_fp) + '\n', encoding='utf-8')


def footprint(ref, value, filename, xy, pin_nets, rotation=0, kind=None):
    fp = p.FootprintLoad(str(LIB), filename)
    if fp is None:
        raise RuntimeError(filename)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetFPID(p.LIB_ID('BreathPet', filename))
    fp.SetPosition(point(*xy))
    fp.SetOrientationDegrees(rotation)
    if kind:
        path = p.KIID_PATH()
        path.push_back(p.KIID(SCH_ID))
        path.push_back(p.KIID(uid(ref)))
        fp.SetPath(path)
    fp.Reference().SetTextSize(p.VECTOR2I(mm(0.8), mm(0.8)))
    fp.Reference().SetTextThickness(mm(0.12))
    fp.Reference().SetTextAngle(p.EDA_ANGLE(0, p.DEGREES_T))
    fp.Value().SetVisible(False)
    for pad in fp.Pads():
        if pad.GetNumber() in pin_nets:
            pad.SetNet(nets[pin_nets[pad.GetNumber()]])
    board.Add(fp)
    fps[ref] = fp
    if kind:
        components.append({'ref': ref, 'value': value, 'footprint': filename,
                           'kind': kind, 'nets': pin_nets, 'uuid': uid(ref)})
    return fp


footprint('J1', 'LILYGO P1 / upper row', 'PinSocket_1x12_P2.54mm_Vertical', (28, 4.57), {}, 90, 'P1')
footprint('J2', 'LILYGO P2 / lower row', 'PinSocket_1x12_P2.54mm_Vertical', (28, 27.43),
          {'1': '+5V_USB', '2': 'GND', '11': 'ADC_GPIO1', '12': '+3V3'}, 90, 'P2')
# Single-pad nets match KiCad's explicit no-connect symbols; they remain isolated.
header_names = {'J1': ['3V3', 'GND', 'GND', 'NC', 'GPIO16', 'GPIO21', 'GPIO17', 'GPIO18', 'GPIO44', 'GPIO43', 'GND', 'GND'],
                'J2': ['5V', 'GND', 'NC', 'NC', 'GPIO13', 'GPIO12', 'GPIO11', 'GPIO10', 'GPIO3', 'GPIO2', 'GPIO1', '3V3']}
for ref, names in header_names.items():
    for pad in fps[ref].Pads():
        if pad.GetNetCode() == 0:
            number = pad.GetNumber()
            net = p.NETINFO_ITEM(board, f'unconnected-({ref}-{names[int(number)-1]}-Pad{number})')
            board.Add(net)
            pad.SetNet(net)
footprint('JP1', 'SENSOR POWER / fit shunt', 'PinHeader_1x02_P2.54mm_Vertical', (3, 31),
          {'1': '+5V_USB', '2': '+5V_SENSOR'}, 0, 'JP')
footprint('J3', 'MQ3 cable / 5V GND AO', 'JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical', (36, 48),
          {'1': '+5V_SENSOR', '2': 'GND', '3': 'MQ3_AO'}, 0, 'JST')
fps['J3'].SetDNP(True)
components[-1]['dnp'] = True
footprint('R1', '8.2k 1%', 'R_0805_2012Metric', (30, 35), {'1': 'MQ3_AO', '2': 'DIV_HALF'}, kind='R')
footprint('R2', '8.2k 1%', 'R_0805_2012Metric', (35, 39), {'1': 'DIV_HALF', '2': 'GND'}, kind='RV')
footprint('R3', '1k 1%', 'R_0805_2012Metric', (39, 33), {'1': 'DIV_HALF', '2': 'ADC_GPIO1'}, kind='R')
footprint('C1', '100nF X7R 50V', 'C_0805_2012Metric', (46, 36), {'1': 'ADC_GPIO1', '2': 'GND'}, kind='C')
footprint('C2', '10uF X5R 16V', 'C_0805_2012Metric', (24, 35.5), {'1': '+5V_SENSOR', '2': 'GND'}, kind='C')
footprint('C3', '100nF X7R 50V', 'C_0805_2012Metric', (25, 39), {'1': '+5V_SENSOR', '2': 'GND'}, kind='C')
footprint('D1', 'BAT54S', 'SOT-23', (52, 35), {'1': 'GND', '2': '+3V3', '3': 'ADC_GPIO1'}, kind='CLAMP')
footprint('S1', 'MQ-3 / verify can variant', 'MQ3_Classic_9.5mm', (14, 42),
          {'1': '+5V_SENSOR', '2': '+5V_SENSOR', '3': '+5V_SENSOR',
           '4': 'MQ3_AO', '5': 'GND', '6': 'MQ3_AO'}, kind='MQ3')
footprint('R4', '6.65k 1% / see variant table', 'R_0805_2012Metric', (28, 49),
          {'1': 'MQ3_AO', '2': 'GND'}, kind='RV')
for ref, xy, net in [('TP5', (56, 45), 'ADC_GPIO1'), ('TP6', (62, 45), 'GND'),
                     ('TP7', (50, 45), '+5V_SENSOR'), ('TP8', (50, 50), 'MQ3_AO')]:
    footprint(ref, net, 'TestPoint_Pad_D1.5mm', xy, {'1': net}, kind='TP')
for ref, xy, net in [('TP1', (8, 23), '+5V_SENSOR'), ('TP2', (16, 23), 'MQ3_AO'),
                     ('TP3', (24, 23), 'ADC_GPIO1'), ('TP4', (8, 17), 'GND')]:
    footprint(ref, net, 'TestPoint_Pad_D1.5mm', xy, {'1': net}, kind='TP')
for ref, xy in [('H1', (3, 3)), ('H2', (3, 51)), ('H3', (65, 51))]:
    footprint(ref, 'M2.5 / NPTH', 'MountingHole_2.7mm_M2.5', xy, {}, kind='HOLE')


def loc(ref, pad):
    for item in fps[ref].Pads():
        if item.GetNumber() == str(pad):
            v = item.GetPosition()
            return (p.ToMM(v.x) - ORIGIN[0], p.ToMM(v.y) - ORIGIN[1])
    raise KeyError((ref, pad))


# Catch a mirrored footprint or different socket definition before routing.
for ref, y in [('J1', 4.57), ('J2', 27.43)]:
    for number in range(1, 13):
        x_actual, y_actual = loc(ref, number)
        assert abs(x_actual - (28 + (number - 1) * 2.54)) < 0.00001
        assert abs(y_actual - y) < 0.00001


def route(net, points, width=0.3, layer=p.F_Cu):
    for start, end in zip(points, points[1:]):
        if start == end:
            continue
        track = p.PCB_TRACK(board)
        track.SetStart(point(*start))
        track.SetEnd(point(*end))
        track.SetWidth(mm(width))
        track.SetLayer(layer)
        track.SetNet(nets[net])
        board.Add(track)


def via(net, xy):
    item = p.PCB_VIA(board)
    item.SetPosition(point(*xy))
    item.SetWidth(mm(0.65))
    item.SetDrill(mm(0.3))
    item.SetViaType(p.VIATYPE_THROUGH)
    item.SetLayerPair(p.F_Cu, p.B_Cu)
    item.SetNet(nets[net])
    board.Add(item)


route('+5V_USB', [loc('J2', 1), (26, 27.43), (22.43, 31), (7, 31), loc('JP1', 1)], 0.75)
route('+5V_SENSOR', [loc('JP1', 2), (6, 36.54), (6, 38.75), loc('S1', 2)], 0.75)
route('+5V_SENSOR', [loc('S1', 1), (10.64, 34), (21.5, 34), (22.5, 35), (22.5, 36.05), loc('C2', 1)], .6)
route('+5V_SENSOR', [(22.5, 36.05), (22.5, 43), (27, 47.5), (34, 47.5), loc('J3', 1)], .6)
route('+5V_SENSOR', [(22.5, 39), loc('C3', 1)], 0.6)
route('MQ3_AO', [loc('R1', 1), (27.5, 35)])
via('MQ3_AO', (27.5, 35))
route('MQ3_AO', [(27.5, 35), loc('J3', 3)], layer=p.B_Cu)
route('DIV_HALF', [loc('R1', 2), (32.95, 33), loc('R3', 1)])
route('DIV_HALF', [loc('R1', 2), (31.5, 35), (34.05, 37.55), loc('R2', 1)])
route('ADC_GPIO1', [loc('R3', 2), (43, 33), (45.05, 35.05), loc('C1', 1)])
route('ADC_GPIO1', [loc('C1', 1), (45.05, 38.5), (54.5, 38.5), (54.5, 35), loc('D1', 3)])
route('ADC_GPIO1', [loc('J2', 11), (53.4, 29.5), (56, 32.1), (56, 35), (54.5, 35)])
route('+3V3', [loc('J2', 12), (56.5, 28), (56.5, 36.5), (51, 37)], layer=p.B_Cu)
via('+3V3', (51, 37))
route('+3V3', [(51, 37), loc('D1', 2)])
for ref, pad, v in [('C1', 2, (47.7, 35.2)), ('C2', 2, (26, 36.5)),
                    ('C3', 2, (26.7, 39.8)), ('R2', 2, (37, 40)), ('D1', 1, (49.5, 34.05))]:
    route('GND', [loc(ref, pad), v])
    via('GND', v)
route('+5V_SENSOR', [loc('TP1', 1), (8, 25)], layer=p.F_Cu)
via('+5V_SENSOR', (8, 25))
route('+5V_SENSOR', [(8, 25), (9, 26), (9, 34.54), loc('JP1', 2)], layer=p.B_Cu)
route('MQ3_AO', [loc('TP2', 1), (16, 25)])
via('MQ3_AO', (16, 25))
route('MQ3_AO', [(16, 25), (18, 27), loc('J3', 3)], layer=p.B_Cu)
route('ADC_GPIO1', [loc('TP3', 1), (24, 25)])
via('ADC_GPIO1', (24, 25))
route('ADC_GPIO1', [(24, 25), (25, 26), (25, 30), (39.95, 30), (39.95, 31.5)], layer=p.B_Cu)
via('ADC_GPIO1', (39.95, 31.5))
# Terminate at a separate via above the resistor, not a via in its solder pad.
route('ADC_GPIO1', [(39.95, 31.5), loc('R3', 2)])
route('GND', [loc('TP4', 1), (8, 19)])
via('GND', (8, 19))
route('+5V_SENSOR', [loc('S1', 1), (9.25, 40.03), loc('S1', 2), (9.25, 43.97), loc('S1', 3)], .6)
route('MQ3_AO', [loc('S1', 4), (23, 46), (32, 46), (34, 44), (41, 44), loc('J3', 3)], layer=p.B_Cu)
route('MQ3_AO', [loc('S1', 4), (20.5, 45.36), (20.5, 38.64), loc('S1', 6)])
route('MQ3_AO', [loc('R4', 1), (27.05, 52), (41, 52), loc('J3', 3)])
route('GND', [loc('R4', 2), (31, 49)])
via('GND', (31, 49))
route('ADC_GPIO1', [(39.95, 31.5), (42, 31.5), (48, 37.5), (56, 43.5), loc('TP5', 1)], layer=p.B_Cu)
via('ADC_GPIO1', (56, 45))
route('GND', [loc('TP6', 1), (62, 47)])
via('GND', (62, 47))
route('+5V_SENSOR', [loc('J3', 1), (34.5, 46.5), (34.5, 43), (48, 43), loc('TP7', 1)], .6)
route('MQ3_AO', [loc('J3', 3), (43, 50), loc('TP8', 1)])


def line(start, end, layer, width=0.15):
    shape = p.PCB_SHAPE(board)
    shape.SetShape(p.SHAPE_T_SEGMENT)
    shape.SetStart(point(*start))
    shape.SetEnd(point(*end))
    shape.SetLayer(layer)
    shape.SetWidth(mm(width))
    board.Add(shape)


def text(value, xy, layer=p.F_SilkS, size=1, angle=0):
    size = max(size, 0.8)
    item = p.PCB_TEXT(board)
    item.SetText(value)
    item.SetPosition(point(*xy))
    item.SetTextSize(p.VECTOR2I(mm(size), mm(size)))
    item.SetTextThickness(mm(0.15 if size >= 1 else 0.12))
    item.SetTextAngle(p.EDA_ANGLE(angle, p.DEGREES_T))
    item.SetLayer(layer)
    if layer == p.B_SilkS:
        item.SetMirrored(True)
    board.Add(item)


# Chamfered corners; three mounting holes avoid placing metal near the antenna.
outline = [(2, 0), (66, 0), (68, 2), (68, 52), (66, 54), (2, 54), (0, 52), (0, 2), (2, 0)]
for a, b in zip(outline, outline[1:]):
    line(a, b, p.Edge_Cuts, 0.05)
for a, b in [((3.9968, 3.1), (64.779, 3.1)), ((64.779, 3.1), (64.779, 28.9)),
             ((64.779, 28.9), (3.9968, 28.9)), ((3.9968, 28.9), (3.9968, 3.1))]:
    line(a, b, p.Dwgs_User)
text('LILYGO DISPLAY FACES UP', (38, 11), size=1)
text('USB <', (8, 11), size=0.85)
text('BREATH PET / REV A', (39, 16), size=1.2)
text('USB POWER ONLY', (39, 19.5), size=0.9)
text('ANTENNA', (62, 14), size=0.8, angle=90)
text('NO COPPER / METAL', (65, 15), size=0.7, angle=90)
text('J1 / P1', (20, 4.57), size=0.8)
text('J2 / P2', (20, 27.43), size=0.8)
text('1', (28, 7.3), size=0.7)
text('1', (28, 24.7), size=0.7)
text('5V', (28, 30), size=0.7)
text('1', (53.4, 24.6), size=0.7)
text('3V', (55.94, 24.6), size=0.7)
text('5V  GND  AO', (38.5, 52.5), size=0.7)
text('ON', (3, 37), size=0.7)
text('5V', (8, 21), size=0.7)
text('AO', (16, 21), size=0.7)
text('ADC', (24, 21), size=0.7)
text('GND', (8, 15), size=0.7)
text('ADC = AO / 2   |   GPIO1', (33, 11), p.B_SilkS, 1.1)
text('68 x 54 mm / 1.6 mm / 2 layers', (33, 15), p.B_SilkS, 0.9)
text('REMOVE POWER BEFORE PLUGGING', (33, 19), p.B_SilkS, 0.85)
text('S1 OR EXTERNAL MODULE - NEVER BOTH', (33, 26), p.B_SilkS, .8)
text('MQ-3 / HOT', (14, 52), size=0.8)
for value, xy in [('ADC', (56, 42.5)), ('GND', (62, 42.5)), ('5V', (47, 45)), ('AO', (47, 50))]:
    text(value, xy, size=.85)
text('CLASSIC 9.5mm PIN CIRCLE - VERIFY FIT', (33, 32), p.B_SilkS, .9)

# Full copper keepout under and beyond the LILYGO antenna.
keep = p.ZONE(board)
keep.SetLayerSet(p.LSET.AllCuMask(2))
keep.SetIsRuleArea(True)
keep.SetDoNotAllowTracks(True)
keep.SetDoNotAllowVias(True)
keep.SetDoNotAllowPads(True)
keep.SetDoNotAllowZoneFills(True)
keep.SetZoneName('LILYGO_ANTENNA_KEEPOUT')
poly = keep.Outline()
poly.NewOutline()
for xy in [(57.5, 0), (68, 0), (68, 31.5), (57.5, 31.5)]:
    v = point(*xy)
    poly.Append(v.x, v.y)
board.Add(keep)

zone = p.ZONE(board)
zone.SetLayer(p.B_Cu)
zone.SetNet(nets['GND'])
zone.SetLocalClearance(mm(0.25))
zone.SetThermalReliefGap(mm(0.3))
zone.SetThermalReliefSpokeWidth(mm(0.35))
zone.SetPadConnection(p.ZONE_CONNECTION_THERMAL)
zone.SetMinThickness(mm(0.2))
zone.SetZoneName('GND_BACK')
poly = zone.Outline()
poly.NewOutline()
for xy in [(0.5, 0.5), (57, 0.5), (57, 32), (67.5, 32), (67.5, 53.5), (0.5, 53.5)]:
    v = point(*xy)
    poly.Append(v.x, v.y)
board.Add(zone)

# Keep reference legends readable in the narrow component strip.
reference_xy = {'J1': (42, 1.2), 'J2': (42, 24.5), 'J3': (38.5, 44.5), 'JP1': (3, 28.5),
                'R1': (30, 32), 'R2': (35, 41.5), 'R3': (39, 35.5),
                'C1': (46, 33.5), 'C2': (24, 33.1), 'C3': (25, 41.4), 'D1': (52, 31),
                'TP1': (8, 26.5), 'TP2': (16, 26.5), 'TP3': (24, 26.5), 'TP4': (12, 17),
                'R4': (28, 51.5), 'S1': (14, 32), 'TP5': (56, 48), 'TP6': (62, 48),
                'TP7': (50, 42.5), 'TP8': (54, 50)}
for ref, xy in reference_xy.items():
    fps[ref].Reference().SetPosition(point(*xy))
for ref in ('H1', 'H2', 'H3'):
    fps[ref].Reference().SetVisible(False)

board.BuildConnectivity()
p.ZONE_FILLER(board).Fill(board.Zones())
p.SaveBoard(str(ROOT / 'carrier.kicad_pcb'), board)

# Project-local libraries make the design editable without library installation.
(ROOT / 'fp-lib-table').write_text('(fp_lib_table\n (lib (name "BreathPet")(type "KiCad")(uri "${KIPRJMOD}/lib/BreathPet.pretty")(options "")(descr "Pinned KiCad footprints"))\n)\n', encoding='utf-8')
(ROOT / 'sym-lib-table').write_text('(sym_lib_table\n (lib (name "BreathPet")(type "KiCad")(uri "${KIPRJMOD}/lib/BreathPet.kicad_sym")(options "")(descr "Carrier symbols"))\n)\n', encoding='utf-8')
project = {'meta': {'filename': 'carrier.kicad_pro', 'version': 1},
           'board': {'design_settings': {'rules': {'min_clearance': 0.25, 'min_track_width': 0.25,
                                                 'min_through_hole_diameter': 0.3,
                                                 'min_via_annular_width': 0.15,
                                                 'min_copper_edge_clearance': 0.3}}},
           'net_settings': {'classes': [{'name': 'Default', 'clearance': 0.25, 'track_width': 0.3,
                                         'via_diameter': 0.65, 'via_drill': 0.3}], 'meta': {'version': 3}}}
(ROOT / 'carrier.kicad_pro').write_text(json.dumps(project, indent=2) + '\n', encoding='utf-8')
(ROOT / 'design.json').write_text(json.dumps({'revision': 'A', 'size_mm': [68, 54, 1.6],
                                             'header_pitch_mm': 2.54, 'header_rows_mm': 22.86,
                                             'header_first_x_mm': 28, 'header_y_mm': [4.57, 27.43],
                                             'mount_holes_mm': [[3, 3, 2.7], [3, 51, 2.7], [65, 51, 2.7]],
                                             'sensor_center_mm': [14, 42], 'sensor_pads_mm': sensor_pads,
                                             'sensor_diameter_mm': 16.7, 'sensor_installed_height_mm': 9.82,
                                             'sensor_measurement_source': 'User measurement, 2026-09-22; height above module PCB',
                                             'components': components}, indent=2) + '\n', encoding='utf-8')
print('Saved carrier PCB; schematic generated by build_schematic.py')
