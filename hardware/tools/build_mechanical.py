"""Parametric prototype enclosure and fit gauges. Requires CadQuery 2.8.

PCB XY is converted to mechanical XY by negating Y. Z=0 is bottom of the
case; the bare PCB sits at Z=5. Models of loose parts are envelopes only.
No claim of physical fit or thermal testing is made by this generator.
"""
from pathlib import Path
import json
import math
import cadquery as cq

ROOT = Path(__file__).resolve().parents[1] / 'rev-a'
OUT = ROOT / 'mechanical'
OUT.mkdir(exist_ok=True)
D = json.loads((ROOT / 'design.json').read_text())
W, H, THICK = D['size_mm']
PCB_Z = 5.0
SOCKET_HEIGHT = 8.5
MALE_SPACER = 2.54
BOARD_REAR = THICK + SOCKET_HEIGHT + MALE_SPACER
CASE_TOP = 25.0
HOLES = [(x, -y) for x, y, diameter in D['mount_holes_mm']]
COVER_HOLES = [(-2, -35), (70, -35), (16, -72), (52, -72)]


def box(x, y, z, dx, dy, dz):
    return cq.Workplane('XY').box(dx, dy, dz, centered=(False, False, False)).translate((x, y, z))


def cylinder(x, y, z, r, height):
    return cq.Workplane('XY').center(x, y).circle(r).extrude(height).translate((0, 0, z))


def holes(shape, coords, radius, z=-1, height=40):
    for x, y in coords:
        shape = shape.cut(cylinder(x, y, z, radius, height))
    return shape


def export(shape, name, stl=True):
    solids = shape.solids().vals()
    assert len(solids) == 1, (name, len(solids))
    assert solids[0].isValid(), name
    assert solids[0].Volume() > 0, name
    cq.exporters.export(shape, str(OUT / (name + '.step')))
    if stl:
        cq.exporters.export(shape, str(OUT / (name + '.stl')), tolerance=.05, angularTolerance=.15)
    bb = solids[0].BoundingBox()
    return {'name': name, 'solid_count': 1, 'valid': True,
            'size_mm': [round(bb.xlen, 3), round(bb.ylen, 3), round(bb.zlen, 3)],
            'volume_mm3': round(solids[0].Volume(), 3)}


# Separate gauges make testing header pitch inexpensive, without powering a board.
gauge = box(0, -32, 0, W, 32, 1.2)
header_xy = [(28 + i * 2.54, -y) for y in D['header_y_mm'] for i in range(12)]
gauge = holes(gauge, header_xy, .65)
gauge = gauge.cut(box(-1, -21, -1, 14, 10, 4))  # USB end notch
sensor_gauge = cylinder(0, 0, 0, 12, 1.2)
sensor_gauge = holes(sensor_gauge, [(x, -y) for x, y in D['sensor_pads_mm'].values()], .7)
sensor_gauge = sensor_gauge.cut(box(-13, -1, -1, 3, 2, 4))  # heater-axis notch

# Walls and floor: print flat, all vertical holes. Open-top USB slot avoids supports.
base = box(-4, -74, 0, 76, 78, CASE_TOP)
base = base.cut(box(-.6, -70.6, 2, 69.2, 71.2, CASE_TOP + 2))
for x, y in HOLES:
    base = base.union(cylinder(x, y, 2, 2.5, 3))
    base = base.cut(cylinder(x, y, 1.2, 1.05, 5))
base = base.cut(box(-5, -27, 14, 8, 23, CASE_TOP))
for x, y in COVER_HOLES:
    # The posts attach to the side walls outside the PCB outline.
    base = base.union(cylinder(x, y, 2, 1.7, CASE_TOP - 2))
    base = base.cut(cylinder(x, y, CASE_TOP - 7, .8, 10))
# Floor ventilation is outside PCB mounting bosses, under the sensor wing.
for x in (20, 25, 30, 35, 40, 45):
    base = base.cut(box(x, -64, -1, 2, 15, 4))

# Flat cover: screen and both board buttons remain directly accessible.
# Main aperture uses board envelope; it is intentionally not a tight bezel.
cover = box(-4, -74, 0, 76, 78, 2)
cover = cover.cut(box(1.5, -30, -1, 64.5, 31, 4))
cover = cover.cut(cylinder(34, -56, -1, 14, 4))
cover = holes(cover, COVER_HOLES, 1.1, -1, 4)
for x in (9, 15, 51, 57):
    cover = cover.cut(box(x, -62, -1, 2, 12, 4))

reports = [export(base, 'case-base'), export(cover, 'case-cover'),
           export(gauge, 'header-fit-gauge'), export(sensor_gauge, 'sensor-fit-gauge')]

# Exact PCB shape and source LILYGO geometry, plus stated part envelopes.
pcb = cq.importers.importStep(str(OUT / 'carrier-board.step'))
vendor_file = ROOT.parent / 'reference' / 't-display-s3-full.step'
lilygo = cq.importers.importStep(str(vendor_file))
lilygo = lilygo.rotate((0, 0, 0), (1, 1, 0), 180).translate((3.9968, .0104 - 16, BOARD_REAR))
socket = [box(26.73, -y - 1.27, THICK, 30.48, 2.54, SOCKET_HEIGHT) for y in D['header_y_mm']]
spacers = [box(26.73, -y - 1.27, THICK + SOCKET_HEIGHT, 30.48, 2.54, MALE_SPACER) for y in D['header_y_mm']]
# Conservative classic MQ can envelope: 20 mm diameter, 17 mm above 2 mm stand-off.
sensor = cylinder(34, -56, THICK + 2, 10, 17)
sensor_base = cylinder(34, -56, THICK + 2, 10, 2)
sensor_leads = [cylinder(34 + x, -56 - y, -.5, .5, THICK + 2.5)
                for x, y in D['sensor_pads_mm'].values()]
smd = []
for x, y in [(30, 35), (35, 39), (39, 33), (46, 36), (24, 35.5), (25, 39), (51, 47)]:
    smd.append(box(x - 1, -y - .625, THICK, 2, 1.25, 1.3))
diode = box(50.5, -36.25, THICK, 3, 2.5, 1.1)
jumper = box(5.73, -35.81, THICK, 2.54, 5.08, 7)

parts = [('carrier', pcb, (.06, .35, .23)), ('display-board', lilygo, (.13, .16, .18)),
         ('sensor-can-envelope', sensor, (.7, .73, .76)), ('sensor-base', sensor_base, (.12, .15, .18)),
         ('diode', diode, (.1, .1, .1)), ('power-jumper', jumper, (.12, .12, .12))]
parts += [(f'socket-{i}', p, (.1, .1, .12)) for i, p in enumerate(socket)]
parts += [(f'header-spacer-{i}', p, (.15, .15, .15)) for i, p in enumerate(spacers)]
parts += [(f'lead-{i}', p, (.7, .7, .7)) for i, p in enumerate(sensor_leads)]
parts += [(f'smd-{i}', p, (.3, .28, .22)) for i, p in enumerate(smd)]

assembly = cq.Assembly(name='breath-pet-rev-a-prototype')
for name, shape, color in parts:
    assembly.add(shape, name=name, color=cq.Color(*color))
assembly.export(str(OUT / 'electronics-assembly.step'))

# Collision checks use nominal envelopes, not solder joints / real tolerances.
# Cover is pierced around the whole display and can, so no support is trapped.
interferences = []
for name, shape, color in parts:
    placed = shape.translate((0, 0, PCB_Z))
    for other_name, other in [('base', base), ('cover', cover.translate((0, 0, CASE_TOP)))]:
        v = placed.intersect(other).val().Volume()
        if v > .001:
            interferences.append({'part': name, 'case': other_name, 'volume_mm3': round(v, 5)})
assert not interferences, interferences

full = cq.Assembly(name='case-fit-prototype')
full.add(base, name='base', color=cq.Color(.08, .12, .19))
full.add(cover.translate((0, 0, CASE_TOP)), name='cover', color=cq.Color(.1, .18, .25))
full.add(assembly, name='electronics', loc=cq.Location(cq.Vector(0, 0, PCB_Z)))
full.export(str(OUT / 'case-assembly.step'))

(ROOT / 'checks/mechanical.json').write_text(json.dumps({
    'cadquery': cq.__version__, 'physical_fit_verified': False,
    'envelope_interferences': interferences, 'parts': reports,
    'assumed_socket_height_mm': SOCKET_HEIGHT, 'assumed_male_spacer_mm': MALE_SPACER,
    'pcb_bottom_above_case_floor_mm': PCB_Z, 'lid_bottom_z_mm': CASE_TOP,
    'sensor_envelope_mm': {'diameter': 20, 'height_above_pcb': 19},
    'notes': ['Print gauges before PCB order.', 'Case is a vented prototype, not liquid sealed.',
              'Verify heater temperature and real stack height before enclosed operation.']
}, indent=2) + '\n', encoding='utf-8')

# Render actual CAD triangulations with VTK; no generated concept image.
import vtk


def render(items, name, eye=(125, -160, 160)):
    renderer = vtk.vtkRenderer()
    renderer.SetBackground(.93, .95, .97)
    for label, shape, color in items:
        vertices, triangles = shape.val().tessellate(.12, .2)
        points = vtk.vtkPoints()
        for v in vertices:
            points.InsertNextPoint(v.x, v.y, v.z)
        cells = vtk.vtkCellArray()
        for a, b, c in triangles:
            cells.InsertNextCell(3)
            for index in (a, b, c):
                cells.InsertCellPoint(index)
        mesh = vtk.vtkPolyData()
        mesh.SetPoints(points)
        mesh.SetPolys(cells)
        mapper = vtk.vtkPolyDataMapper()
        mapper.SetInputData(mesh)
        actor = vtk.vtkActor()
        actor.SetMapper(mapper)
        actor.GetProperty().SetColor(*color)
        actor.GetProperty().SetAmbient(.3)
        actor.GetProperty().SetDiffuse(.7)
        renderer.AddActor(actor)
    camera = renderer.GetActiveCamera()
    camera.SetPosition(*eye)
    camera.SetFocalPoint(34, -35, 12)
    camera.SetViewUp(0, 0, 1)
    camera.ParallelProjectionOn()
    renderer.ResetCamera()
    camera.Zoom(.88)
    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(1400, 1100)
    window.AddRenderer(renderer)
    window.Render()
    capture = vtk.vtkWindowToImageFilter()
    capture.SetInput(window)
    capture.Update()
    writer = vtk.vtkPNGWriter()
    writer.SetFileName(str(ROOT / 'previews' / (name + '.png')))
    writer.SetInputConnection(capture.GetOutputPort())
    writer.Write()
    window.Finalize()


render(parts, 'electronics')
render([('base', base, (.08, .12, .19)), ('cover', cover.translate((0, 0, CASE_TOP)), (.1, .18, .25))]
       + [(n, s.translate((0, 0, PCB_Z)), c) for n, s, c in parts], 'case')
print('Saved valid STEP/STL prototypes, nominal collision checks, and CAD previews')
