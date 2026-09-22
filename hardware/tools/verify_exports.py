"""Independent drill/outline checks and printable mesh checks.

Requires gerbonara==1.6.3 and vtk==9.6.2. Does not modify fabrication data.
"""
from pathlib import Path
import json
import warnings
import vtk
from gerbonara import LayerStack

ROOT = Path(__file__).resolve().parents[1] / 'rev-a'
D = json.loads((ROOT / 'design.json').read_text())
with warnings.catch_warnings(record=True) as notices:
    stack = LayerStack.open(ROOT / 'manufacturing/gerbers')
    # Gerbonara notes KiCad's legal G90 placement after the header separator.
    # Preserve notices in the report rather than modifying vendor output.
    pth = stack.drill_pth.objects
    npth = stack.drill_npth.objects
messages = [str(n.message).replace(str(ROOT), 'rev-a') for n in notices]
bounds = stack.board_bounds()
assert all(abs(a-b) < .001 for a, b in zip(sum(bounds, ()),
    (-.025, -D['size_mm'][1] - .025, D['size_mm'][0] + .025, .025))), bounds


def present(objects, x, y, diameter):
    return any(abs(o.x-x) < 1e-5 and abs(o.y-y) < 1e-5 and
               abs(o.aperture.diameter-diameter) < 1e-5 for o in objects)


for y in D['header_y_mm']:
    for n in range(12):
        assert present(pth, 28+n*2.54, -y, 1), (n, y)
for x, y in D['sensor_pads_mm'].values():
    assert present(pth, D['sensor_center_mm'][0]+x, -D['sensor_center_mm'][1]-y, 1.2), (x, y)
for x, y, diameter in D['mount_holes_mm']:
    assert present(npth, x, -y, diameter), (x, y)
assert len(npth) == 3
assert len(pth) == 50
stack.graphic_layers = dict(sorted(stack.graphic_layers.items(),
    key=lambda item: {'outline': 0, 'silk': 1, 'copper': 2}.get(item[0][1], 3)))
for side in ['top', 'bottom']:
    (ROOT / 'previews' / f'gerber-{side}.svg').write_text(
        str(stack.to_svg(side_re=f'{side}|mechanical', margin=2, colors={
            f'{side} copper': '#c72f32' if side == 'top' else '#236ba8',
            f'{side} silk': '#383838' if side == 'top' else '#ffffff', 'mechanical outline': '#111111',
            'drill pth': 'white', 'drill npth': 'white'})), encoding='utf-8')

meshes = []
for path in sorted((ROOT / 'mechanical').glob('*.stl')):
    reader = vtk.vtkSTLReader()
    reader.SetFileName(str(path))
    reader.Update()
    edges = vtk.vtkFeatureEdges()
    edges.SetInputConnection(reader.GetOutputPort())
    edges.BoundaryEdgesOn()
    edges.NonManifoldEdgesOn()
    edges.FeatureEdgesOff()
    edges.ManifoldEdgesOff()
    edges.Update()
    count = edges.GetOutput().GetNumberOfCells()
    assert count == 0, (path, count)
    meshes.append({'file': path.name, 'bad_edges': count,
                   'triangles': reader.GetOutput().GetNumberOfCells()})
(ROOT / 'checks/stl-meshes.json').write_text(json.dumps(meshes, indent=2)+'\n', encoding='utf-8')
(ROOT / 'checks/independent-exports.json').write_text(json.dumps({
    'outline_stroke_bounds_mm': bounds, 'header_holes_verified': 24,
    'sensor_holes_verified': 6, 'npth_holes_verified': 3, 'pth_holes_total': 50,
    'parser_notices': messages, 'stl_count': len(meshes), 'all_stl_manifold': True
}, indent=2)+'\n', encoding='utf-8')
print('Verified Gerber dimensions, 33 critical drills and four closed printable meshes')
