"""Capture fictional pets on the ESP32, then restore normal firmware and check saves.

Requires an attached board running normal firmware. Only the isolated device-test
namespaces are reset. Private before/after snapshots stay in ignored artifacts/.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import serial
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


class Device:
    def __init__(self, port):
        self.serial = serial.Serial(None, 115200, timeout=0.3, write_timeout=2)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port
        self.serial.open()
        self.request_id = 0

    def close(self):
        self.serial.close()

    def command(self, command):
        self.request_id += 1
        rid = self.request_id
        self.serial.write(f'@{rid} {command}\n'.encode())
        deadline = time.monotonic() + 6
        acknowledged = False
        pending = b''
        retry_at = None
        recovered = False
        while time.monotonic() < deadline:
            # Never replay mutations when USB loses a reply.
            if acknowledged and not pending and not recovered and time.monotonic() > retry_at:
                query = 'history' if command == 'history' else 'status'
                self.serial.write(f'@{rid} {query}\n'.encode())
                recovered = True
            pending += self.serial.readline()
            if not pending.endswith(b'\n'):
                continue
            line = pending.decode(errors='replace').strip()
            pending = b''
            if line.startswith('CMD'):
                acknowledged = line == f'CMD {rid}'
                if acknowledged:
                    retry_at = time.monotonic() + 1
            elif acknowledged and line.startswith('ERROR'):
                raise RuntimeError(f'{command}: {line}')
            elif acknowledged and line.startswith('{'):
                reply = json.loads(line)
                if reply.get('request_id') == rid:
                    return reply
        raise TimeoutError(command)

    def wait_for(self, predicate, seconds=20):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            status = self.command('status')
            if predicate(status):
                return status
            time.sleep(0.15)
        raise TimeoutError(f'Expected device state; last status: {status}')

    def frame(self, path):
        time.sleep(0.12)  # Let the render loop draw the requested page.
        self.serial.write(b'screen\n')
        deadline = time.monotonic() + 6
        pending = b''
        while time.monotonic() < deadline:
            pending += self.serial.readline()
            if not pending.endswith(b'\n'):
                continue
            line, pending = pending.strip(), b''
            if line.startswith(b'FRAME '):
                if line != b'FRAME 320 170 RGB565BE':
                    raise RuntimeError(f'Unexpected framebuffer: {line!r}')
                break
        else:
            raise TimeoutError('Framebuffer header')
        raw = bytearray()
        deadline = time.monotonic() + 10
        while len(raw) < 320 * 170 * 2 and time.monotonic() < deadline:
            raw.extend(self.serial.read(320 * 170 * 2 - len(raw)))
        if len(raw) != 320 * 170 * 2:
            raise RuntimeError(f'Truncated framebuffer: {len(raw)}')
        pixels = []
        for i in range(0, len(raw), 2):
            value = (raw[i] << 8) | raw[i + 1]
            pixels.append((((value >> 11) & 31) * 255 // 31,
                           ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
        image = Image.new('RGB', (320, 170))
        image.putdata(pixels)
        image.save(path)


def snapshot(device):
    state = device.command('status')
    if state.get('test_mode') is not False:
        raise RuntimeError('Normal firmware required for save-preservation check')
    result = {'status': state, 'pets': []}
    for slot in range(state['players']):
        pet = device.command(f'select {slot}')
        result['pets'].append({'status': pet, 'history': device.command('history')['history']})
    device.command('tank')
    return result


def saved_fields(snapshot):
    # Exclude boot counters, uptime, animation, and ordinary powered-time decay.
    keys = ('name', 'pet_type', 'feeds', 'history_count', 'hat', 'item', 'colour',
            'hats', 'items', 'colours', 'visits', 'checkins', 'naps', 'games')
    state = snapshot['status']
    return {'settings': {k: state[k] for k in ('players', 'night', 'zero', 'span', 'sensor_span_mv', 'upgrades')},
            'pets': [{'saved': {k: pet['status'][k] for k in keys}, 'history': pet['history']}
                     for pet in snapshot['pets']]}


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def flash(environment, port, log_dir):
    print(f'Flashing {environment}...', flush=True)
    log = log_dir / f'{environment}-upload.log'
    with log.open('w', encoding='utf-8') as output:
        subprocess.run([sys.executable, '-m', 'platformio', 'run', '-e', environment,
                        '--target', 'upload', '--upload-port', port], cwd=ROOT,
                       stdout=output, stderr=subprocess.STDOUT, check=True)
    time.sleep(3)


def capture(device, destination):
    initial = device.command('status')
    if initial.get('test_mode') is not True:
        raise RuntimeError('Refusing to reset anything outside isolated test firmware')
    device.command('night new CONFIRM')
    device.command('test fun reset')
    device.command('cal default')
    manifest = {'captured_at': datetime.now(timezone.utc).isoformat(),
                'firmware_version': initial['version'], 'size': [320, 170],
                'origin': 'ESP32 RGB565 framebuffer; not a physical panel readback',
                'fixture': 'Fictional isolated test pets; injected ADC values, not human readings',
                'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'test_firmware_sha256': hashlib.sha256((ROOT / '.pio/build/device-test/firmware.bin').read_bytes()).hexdigest(),
                'images': []}

    def shot(name, page, caption):
        status = device.command('status')
        if status['page'] != page:
            raise RuntimeError(f'{name}: expected {page}, got {status["page"]}')
        path = destination / f'{name}.png'
        device.frame(path)
        manifest['images'].append({'file': path.name, 'page': page, 'caption': caption,
                                   'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        print(f'Captured {name}', flush=True)

    def pet_action(index):
        device.command('select 0')
        for _ in range(index):
            device.command('ui next')
        device.command('ui select')

    def menu_action(index):
        device.command('ui menu')
        for _ in range(index):
            device.command('ui next')
        device.command('ui select')

    shot('tank-empty', 'tank', 'An empty evening: add pets one at a time.')
    device.command('ui select')
    shot('nickname', 'name', 'Choose an unused nickname.')
    device.command('ui select')
    device.command('ui next')
    shot('species', 'choose_pet', 'Choose one of six species.')
    device.command('ui select')
    pet_action(0)
    shot('demo', 'feed', 'Optional DEMO picker; normal firmware starts in LIVE mode.')
    device.command('sample 50')
    device.command('join GOOSE 2')
    device.command('sample 85')
    device.command('join BEAN 5')
    device.command('sample 0')
    # Earn real fixture rewards through game rules, without changing normal saves.
    for _ in range(2):
        device.command('test minutes 10')
        for slot in range(3):
            device.command(f'select {slot}')
            device.wait_for(lambda s: s['cooldown_ms'] == 0, 7)
            device.command(f'sample {[50, 85, 0][slot]}')
    device.command('tank')
    shot('tank-party', 'tank', 'Three pets swim freely with reactions, accessories and shared decorations.')
    device.command('select 2')
    device.command('rest')
    device.command('test minutes 10')
    device.command('select 0')
    device.wait_for(lambda s: s['cooldown_ms'] == 0, 7)
    device.command('sample 50')
    device.command('tank')
    shot('tank-states', 'tank', 'PARTY, DROWSY and sleeping pets share the same tank.')
    device.command('select 0')
    shot('pet', 'pet', 'Pet detail: personality, equipped belongings and one energy meter.')
    pet_action(3)
    shot('wardrobe', 'wardrobe', 'Cycle unlocked hats, hand items and colours.')
    pet_action(4)
    device.command('test bubble 50')
    device.command('ui select')
    shot('bubble-catch', 'play', 'Catch three bubbles in the gold zone to wake your pet.')
    device.command('ui back')
    device.command('test sensor 100')
    device.command('input live')
    device.command('cal default')
    device.wait_for(lambda s: s['feed_ready'], 15)
    pet_action(0)
    shot('countdown', 'countdown', 'Five seconds to get ready; the peak window has not started.')
    device.wait_for(lambda s: s['page'] == 'sampling', 7)
    device.command('test sensor 850')
    time.sleep(0.4)
    shot('blow', 'sampling', 'Ten seconds of BLOW with a game-response meter, using injected 850 mV.')
    device.wait_for(lambda s: s['page'] == 'result', 13)
    shot('result', 'result', 'Level 60 / PARTY from the synthetic MQ-3 capture; this is not BAC.')
    pet_action(1)
    shot('history', 'history', 'Per-pet readings show level and source; older DEMO samples stay labeled.')
    device.wait_for(lambda s: s['cooldown_ms'] == 0, 7)
    pet_action(0)
    shot('recovery', 'feed', 'Recovery appears only when Feed is requested before the signal settles.')
    device.command('ui back')
    device.command('test sensor 100')
    device.command('ui menu')
    shot('menu', 'menu', 'The evening menu uses the same selector and physical-button labels.')
    menu_action(1)
    shot('response-settings', 'calibration', 'Adjust game sensitivity; these settings do not calibrate BAC.')
    device.command('sensor open')
    device.wait_for(lambda s: s['mq3_can_zero'], 15)
    device.command('sensor zero')
    shot('mq3-setup', 'sensor', 'Bench diagnostics retain millivolts and a trace; this fixture injects 100 mV.')
    menu_action(5)
    shot('awards', 'awards', 'Evening awards summarize clothing, naps and friendly encounters.')
    menu_action(6)
    shot('tank-upgrades', 'decor', 'Shared decorations remain unlocked across evenings.')
    menu_action(2)
    shot('new-evening', 'new_night', 'Keep this evening is the default before clearing pets and readings.')
    write_json(destination / 'manifest.json', manifest)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM3')
    args = parser.parse_args()
    private = ROOT / 'artifacts/gallery'
    private.mkdir(parents=True, exist_ok=True)
    stage = private / datetime.now().strftime('%Y%m%d-%H%M%S')
    stage.mkdir()
    device = Device(args.port)
    try:
        before = snapshot(device)
    finally:
        device.close()
    write_json(private / 'normal-before.json', before)
    try:
        flash('device-test', args.port, private)
        device = Device(args.port)
        try:
            manifest = capture(device, stage)
        finally:
            device.close()
    finally:
        # Runs even on a failed upload or interrupted gallery capture.
        flash('breath-pet', args.port, private)
        device = Device(args.port)
        try:
            after = snapshot(device)
        finally:
            device.close()
        write_json(private / 'normal-after.json', after)
        if saved_fields(before) != saved_fields(after):
            raise RuntimeError('Normal saved fields changed; inspect private artifacts/gallery snapshots')
        if not after['status']['storage_ok'] or not after['status']['fun_storage_ok']:
            raise RuntimeError('Normal save storage check failed after restore')
        print('Normal firmware restored; roster, histories, settings and collected items preserved.', flush=True)
    target = ROOT / 'docs/screenshots'
    target.mkdir(parents=True, exist_ok=True)
    for entry in manifest['images']:
        shutil.copy2(stage / entry['file'], target / entry['file'])
    shutil.copy2(stage / 'manifest.json', target / 'manifest.json')
    print(f'Published {len(manifest["images"])} captures to {target}', flush=True)


if __name__ == '__main__':
    main()
