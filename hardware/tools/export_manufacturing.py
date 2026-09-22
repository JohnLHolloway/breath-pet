"""Export and validate prototype manufacturing files with KiCad CLI.

Usage: python export_manufacturing.py --kicad-cli PATH
Part selection / placement review at the assembly house is still required.
This script does not upload files or place an order.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import subprocess
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1] / 'rev-a'
parser = argparse.ArgumentParser()
parser.add_argument('--kicad-cli', required=True)
args = parser.parse_args()
CLI = str(Path(args.kicad_cli).resolve())
PCB = ROOT / 'carrier.kicad_pcb'
SCH = ROOT / 'carrier.kicad_sch'
MFG = ROOT / 'manufacturing'
GERBERS = MFG / 'gerbers'
GERBERS.mkdir(parents=True, exist_ok=True)


def run(*args):
    subprocess.run([CLI, *map(str, args)], check=True, cwd=ROOT)


run('pcb', 'drc', '--refill-zones', '--save-board', '--schematic-parity',
    '--exit-code-violations', '--format', 'json', '--output', ROOT / 'checks/drc.json', PCB)
run('sch', 'erc', '--exit-code-violations', '--format', 'json',
    '--output', ROOT / 'checks/erc.json', SCH)
run('sch', 'export', 'netlist', '--format', 'kicadxml',
    '--output', ROOT / 'checks/schematic.net.xml', SCH)
# Make the published report portable; the source path carries no design data.
net_path = ROOT / 'checks/schematic.net.xml'
net_text = net_path.read_text(encoding='utf-8')
start = net_text.index('<source>') + len('<source>')
end = net_text.index('</source>', start)
net_path.write_text(net_text[:start] + 'carrier.kicad_sch' + net_text[end:], encoding='utf-8', newline='\n')
run('pcb', 'export', 'gerbers', '--output', GERBERS,
    '--layers', 'F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts,F.Paste',
    '--use-drill-file-origin', '--subtract-soldermask', '--check-zones', PCB)
run('pcb', 'export', 'drill', '--output', GERBERS, '--format', 'excellon',
    '--drill-origin', 'plot', '--excellon-units', 'mm', '--excellon-separate-th', PCB)
run('pcb', 'export', 'pos', '--output', MFG / 'kicad-top-pos.csv', '--side', 'front',
    '--format', 'csv', '--units', 'mm', '--use-drill-file-origin', '--smd-only', '--exclude-dnp', PCB)
run('sch', 'export', 'svg', '--output', ROOT / 'previews', SCH)
run('pcb', 'export', 'svg', '--layers', 'F.Cu,F.Silkscreen,Edge.Cuts', '--mode-single',
    '--fit-page-to-board', '--exclude-drawing-sheet', '--output', ROOT / 'previews/pcb-top.svg', PCB)
run('pcb', 'export', 'svg', '--layers', 'B.Cu,B.Silkscreen,Edge.Cuts', '--mode-single',
    '--fit-page-to-board', '--exclude-drawing-sheet', '--output', ROOT / 'previews/pcb-bottom.svg', PCB)
run('pcb', 'export', 'step', '--force', '--board-only', '--drill-origin',
    '--output', ROOT / 'mechanical/carrier-board.step', PCB)

# Validate critical net topology independently of the generator's routing.
netlist = ET.parse(ROOT / 'checks/schematic.net.xml')
actual = {}
for net in netlist.findall('./nets/net'):
    name = net.attrib['name'].removeprefix('/')
    actual[name] = {(node.attrib['ref'], node.attrib['pin']) for node in net.findall('node')}
expected = {
    '+5V_USB': {('J2', '1'), ('JP1', '1')},
    '+5V_SENSOR': {('JP1', '2'), ('J3', '1'), ('S1', '1'), ('S1', '2'), ('S1', '3'), ('C2', '1'), ('C3', '1'), ('TP1', '1'), ('TP7', '1')},
    'MQ3_AO': {('J3', '3'), ('S1', '4'), ('S1', '6'), ('R1', '1'), ('R4', '1'), ('TP2', '1'), ('TP8', '1')},
    'DIV_HALF': {('R1', '2'), ('R2', '1'), ('R3', '1')},
    'ADC_GPIO1': {('R3', '2'), ('C1', '1'), ('D1', '3'), ('J2', '11'), ('TP3', '1'), ('TP5', '1')},
    '+3V3': {('J2', '12'), ('D1', '2')},
    'GND': {('J2', '2'), ('J3', '2'), ('S1', '5'), ('R2', '2'), ('R4', '2'), ('C1', '2'), ('C2', '2'), ('C3', '2'), ('D1', '1'), ('TP4', '1'), ('TP6', '1')}
}
for net, nodes in expected.items():
    assert actual[net] == nodes, (net, actual[net] ^ nodes)


def write_csv(path, headings, rows):
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(headings)
        writer.writerows(rows)


# These are electrical/package requirements, not an assertion of JLC stock.
# Blank LCSC numbers intentionally require explicit part matching before order.
write_csv(MFG / 'jlc-bom-direct.csv', ['Comment', 'Designator', 'Footprint', 'LCSC Part #'], [
    ['8.2k ohm 1% >=0.125W', 'R1,R2', '0805', ''],
    ['1k ohm 1% >=0.125W', 'R3', '0805', ''],
    ['6.65k ohm 1% >=0.125W - verify sensor variant', 'R4', '0805', ''],
    ['100nF X7R 50V 10%', 'C1,C3', '0805', ''],
    ['10uF X5R 16V minimum 10%', 'C2', '0805', ''],
    ['BAT54S dual series Schottky - A1 pin1 K2 pin2 common pin3', 'D1', 'SOT-23', ''],
])
rows = list(csv.DictReader((MFG / 'kicad-top-pos.csv').open(encoding='utf-8')))
smt = {'R1', 'R2', 'R3', 'R4', 'C1', 'C2', 'C3', 'D1'}
positions = []
for row in rows:
    if row['Ref'] in smt:
        positions.append([row['Ref'], row['PosX'], row['PosY'], 'Top', row['Rot']])
assert {p[0] for p in positions} == smt
assert all(0 <= float(p[1]) <= 68 and -70 <= float(p[2]) <= 0 for p in positions)
write_csv(MFG / 'jlc-cpl-direct.csv', ['Designator', 'Mid X', 'Mid Y', 'Layer', 'Rotation'], positions)
write_csv(MFG / 'hand-assembly.csv', ['Designator', 'Quantity', 'Part', 'Notes'], [
    ['J1,J2', 2, '1x12 female socket 2.54mm pitch', '8.5mm body assumed in CAD; straight top-entry; verify stack'],
    ['LILYGO headers', 2, '1x12 male header 2.54mm pitch', 'Omit if already soldered; pins face carrier; 2.54mm spacer assumed'],
    ['JP1', 1, '1x2 male header 2.54mm + shunt', 'Fit shunt for USB-powered operation'],
    ['S1', 1, 'Bare MQ-3 can - exact variant not selected', 'Classic 9.5mm pin circle; verify donor fit and electrode pairs'],
    ['J3', 0, 'JST B3B-XH-A 3pin 2.50mm', 'DNP for direct sensor; external module variant only'],
    ['PCB screws', 3, 'M2.5 x 4mm suitable for plastic pilot holes', '1.6mm PCB on printed 3mm bosses; verify engagement'],
    ['Case screws', 4, 'M2 x 6mm suitable for plastic pilot holes', '2mm cover; 1.6mm pilot holes; do not overtighten'],
])

files = sorted(p for p in GERBERS.iterdir() if p.is_file())
required = {'.gtl', '.gbl', '.gts', '.gbs', '.gto', '.gbo', '.gm1', '.gtp'}
assert required <= {p.suffix.lower() for p in files}, [p.name for p in files]
assert len([p for p in files if p.suffix == '.drl']) == 2
for p in files:
    assert p.stat().st_size > 50, p
with zipfile.ZipFile(MFG / 'breath-pet-rev-a-gerbers.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
    for p in files:
        archive.write(p, p.name)
with zipfile.ZipFile(MFG / 'breath-pet-rev-a-gerbers.zip') as archive:
    assert archive.testzip() is None

checks = {'status': 'CAD checked; physical fit and sensor variant pending',
          'critical_nets_checked': len(expected), 'smt_placements': len(positions),
          'divider_nominal_ratio': .5, 'divider_1pct_ratio_range': [.495, .505],
          'adc_max_at_5v25_input_v': 5.25 * .505,
          'direct_effective_load_ohm': round(1 / (1 / 6650 + 1 / 16400), 2),
          'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(ROOT / 'checks/manufacturing.json').write_text(json.dumps(checks, indent=2) + '\n', encoding='utf-8')
print('Validated topology, eight SMT placements, and Gerber ZIP; part matching remains manual')
