"""Integration test against real firmware over USB. Resets only the demo pet state."""
import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM3')
parser.add_argument('--reset-demo-data', action='store_true', help='Required: this test replaces simulated history and calibration settings')
args = parser.parse_args()
if not args.reset_demo_data:
    parser.error('--reset-demo-data is required because this integration test changes saved demo data')
report = {'timestamp': datetime.now(timezone.utc).isoformat(), 'port': args.port, 'checks': []}

def check(name, condition):
    report['checks'].append({'name': name, 'pass': bool(condition)})
    print(f"{'PASS' if condition else 'FAIL'} {name}", flush=True)
    if not condition:
        raise AssertionError(name)

try:
    with serial.Serial(args.port, 115200, timeout=0.3, write_timeout=2) as device:
        # Drain USB data queued before the host opened this port.
        startup_deadline = time.monotonic()+2
        while time.monotonic() < startup_deadline:
            device.readline()

        def command(text, marker='{'):
            device.reset_input_buffer()
            device.write((text+'\n').encode())
            device.flush()
            deadline = time.monotonic()+4
            while time.monotonic() < deadline:
                line = device.readline().decode(errors='replace').strip()
                if line.startswith(marker):
                    return json.loads(line) if marker == '{' else line
            raise TimeoutError(f'No response to {text!r}')

        command('cal default')
        command('history clear')
        s = command('reset')
        check('Firmware identifies itself and has sensor disabled', s['app']=='breath-pet' and s['sensor']=='SIMULATED')
        check('320x170 framebuffer and 8MB PSRAM initialized', s['display'] and (s['width'],s['height'])==(320,170) and s['psram']>=8000000)
        check('NVS storage available', s['storage_ok'])
        report['touch_controller'] = s['touch']
        check('Default pet is happy', s['mood']=='HAPPY' and s['food']==75)
        s = command('feed')
        check('Feeding increases care meters', (s['food'],s['joy'],s['energy'],s['feeds'])==(90,90,95,1))
        s = command('cycle'); check('Fake medium sample -> wobbly', s['sample']==45 and s['mood']=='WOBBLY')
        s = command('cycle'); check('Fake high sample -> dizzy', s['sample']==85 and s['mood']=='DIZZY')
        s = command('cycle'); check('Fake sample cycle returns to happy', s['sample']==0 and s['mood']=='HAPPY')
        check('On-device logic and hardware self-test', command('selftest','SELFTEST').startswith('SELFTEST PASS'))
        check('Invalid sample rejected', command('sample 999','ERROR').startswith('ERROR'))
        check('Oversized serial input rejected', command('x'*80,'ERROR').startswith('ERROR'))
        s = command('status'); check('Invalid input leaves sample unchanged',s['sample']==0)
        before = command('reset')
        # Includes one complete care tick and exercises continuous animation.
        deadline = time.monotonic()+32
        while time.monotonic() < deadline:
            device.readline()
        after = command('status')
        check('Animation continues without reboot',after['frames']>before['frames']+200 and after['uptime_ms']>before['uptime_ms']+30000)
        check('Timed care decay runs', (after['food'],after['joy'],after['energy'])==(73,79,89))
        report['observed_status'] = after
        for i in range(18):
            command('sample '+str([0,45,85][i%3]))
        history = command('history')['history']
        check('History is capped at 16 newest samples',len(history)==16 and history[0]['raw']==85 and history[-1]['raw']==85 and history[0]['n']-history[-1]['n']==15)
        command('sample 45')
        s = command('cal zero')
        check('Zero calibration removes the current baseline',s['zero']==45 and s['sample']==0)
        s = command('sample 0')
        check('Calibrated readings never go below zero',s['sample']==0)
        s = command('sample 85')
        check('Calibration applies to new readings',s['sample']==40)
        s = command('cal minus')
        check('Span changes sensitivity',s['span']==75 and s['sample']==53)
        command('cal minus'); s = command('cal minus')
        check('Calibrated score caps at 100',s['sample']==100 and s['span']==25)
        s = command('cal minus'); check('Span lower bound',s['span']==25)
        for _ in range(9): s = command('cal plus')
        check('Span upper bound',s['span']==200)
        history = command('history')['history']
        check('Old readings preserve the score at capture',history[0]['raw']==85 and history[0]['score']==40)
        for target,num in [('readings',1),('setup',2),('pet',0)]:
            s = command('page '+target)
            check('Page navigation: '+target,s['page']==num)
        before = command('status')
        command('reboot','REBOOT')
        device.close()
        time.sleep(2)
        device.open()
        drain = time.monotonic()+2
        while time.monotonic()<drain: device.readline()
        after = command('status')
        check('Calibration survives a real reboot',after['zero']==45 and after['span']==200 and after['boot']==before['boot']+1)
        check('History survives a real reboot',command('history')['history']==history)
        command('cal default')
        command('history clear')
        for raw in (0,45,85): command('sample '+str(raw))
        report['final_status'] = command('reset')
        report['result'] = 'PASS'
        report['physical_checks'] = 'Touch alignment and physical controls require observation by user; serial page tests do not prove touch alignment.'
except Exception as exc:
    report['result'] = 'FAIL'
    report['error'] = str(exc)
    raise
finally:
    output = Path(__file__).resolve().parents[1]/'test-results.json'
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Report: {output}',flush=True)
