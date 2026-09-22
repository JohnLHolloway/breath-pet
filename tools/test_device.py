"""Exercises party ownership and the actual button state machine on the ESP32."""
import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path
import serial

p=argparse.ArgumentParser()
p.add_argument('--port',default='COM3')
p.add_argument('--reset-demo-data',action='store_true')
p.add_argument('--keep-demo-roster',action='store_true')
args=p.parse_args()
if not args.reset_demo_data: p.error('--reset-demo-data is required: this replaces the isolated test evening')
report={'timestamp':datetime.now(timezone.utc).isoformat(),'checks':[]}

def check(name,condition):
    report['checks'].append({'name':name,'pass':bool(condition)})
    print(('PASS ' if condition else 'FAIL ')+name,flush=True)
    if not condition: raise AssertionError(name)

try:
    with serial.Serial(args.port,115200,timeout=.3,write_timeout=2) as device:
        def drain(seconds=1):
            end=time.monotonic()+seconds
            while time.monotonic()<end: device.readline()

        def cmd(text,marker='{'):
            device.reset_input_buffer()
            device.write((text+'\n').encode()); device.flush()
            end=time.monotonic()+5
            acknowledged=False
            pending=b''
            while time.monotonic()<end:
                pending+=device.readline()
                # A USB read timeout can return part of a line; wait for its newline.
                if not pending.endswith(b'\n'): continue
                line=pending.decode(errors='replace').strip()
                pending=b''
                if line=='CMD': acknowledged=True
                elif acknowledged and line.startswith(marker):
                    try: return json.loads(line) if marker=='{' else line
                    except json.JSONDecodeError:
                        report['invalid_reply']={'command':text,'line':line}
                        raise
            raise TimeoutError(text)

        drain(2)
        check('Isolated test storage enabled',cmd('status').get('test_mode') is True)
        cmd('night new CONFIRM'); cmd('cal default')
        s=cmd('status')
        check('Empty tank starts with Add Pet selected',s['players']==0 and s['page']=='tank' and s['selected']==-1 and s['cursor']==6)
        check('Display, PSRAM and storage initialized',s['display'] and s['psram']>8000000 and s['storage_ok'])
        check('All sample input is simulated',s['sensor']=='SIMULATED')
        report['touch_controller']=s['touch']
        s=cmd('ui select'); check('Add Pet opens nickname picker',s['page']=='name' and s['picker_name']=='CAPTAIN')
        s=cmd('ui select'); check('Nickname selection opens pet picker',s['page']=='choose_pet')
        s=cmd('ui next'); check('Pet picker cycles species',s['picker_pet']==1)
        s=cmd('ui select'); check('Adoption assigns name and chosen pet',s['page']=='pet' and s['name']=='CAPTAIN' and s['pet_type']==1 and s['players']==1)
        s=cmd('ui select'); check('Pet action opens Feed your pet',s['page']=='feed' and s['demo_raw']==25)
        s=cmd('ui select'); check('Feed begins a timed sample',s['page']=='sampling')
        drain(3.4); s=cmd('status')
        check('Timed sample is captured once with capped benefit',s['page']=='result' and s['food']==85 and s['joy']==82 and s['health']==100 and s['feeds']==1)
        check('Repeated feeding is rejected during cooldown',cmd('sample 85','ERROR').startswith('ERROR'))
        s=cmd('status'); check('Rejected feed changes no state',s['feeds']==1 and s['health']==100)
        s=cmd('ui next'); check('Result cycles action without leaving screen',s['page']=='result' and s['cursor']==1)
        s=cmd('ui select'); check('Result confirms Visit your pet',s['page']=='pet')
        cmd('ui next'); s=cmd('ui select'); check('Highlighted history action opens history',s['page']=='history')
        s=cmd('ui next'); check('Single history page wraps safely',s['page']=='history')
        s=cmd('ui select'); check('History confirms return to pet',s['page']=='pet')
        s=cmd('history'); check('History records correct owner and score',s['owner']=='CAPTAIN' and len(s['history'])==1 and s['history'][0]['score']==25)
        captain_history=s['history']
        cmd('tank'); cmd('ui next'); s=cmd('ui select')
        check('Taken nickname is skipped for next person',s['picker_name']=='GOOSE')
        cmd('ui select'); s=cmd('ui select')
        check('Second pet has independent fresh stats',s['name']=='GOOSE' and s['food']==70 and s['history_count']==0)
        s=cmd('sample 85'); check('Overload damages only the selected pet',s['health']==83 and s['food']==73 and s['joy']==58 and s['score']==85)
        goose_history=cmd('history')['history']
        s=cmd('select 0'); check('Other person is unaffected',s['health']==100 and s['food']==85 and s['feeds']==1)
        check('Other person history is unaffected',cmd('history')['history']==captain_history)
        cmd('select 1'); s=cmd('rest'); check('Rest recovers health without a sample',s['health']==91 and s['feeds']==1)
        s=cmd('rest'); check('Rest is debounced with cooldown',s['health']==91)
        s=cmd('cal zero'); check('Calibration zero uses last fake raw sample',s['zero']==85)
        s=cmd('cal minus'); check('Calibration span can change',s['span']==75)
        check('Stored reading keeps score at capture',cmd('history')['history']==goose_history)
        boot=s['boot']; cmd('reboot','REBOOT'); device.close(); time.sleep(2); device.open(); drain(2)
        s=cmd('status'); check('Roster and calibration survive restart',s['players']==2 and s['zero']==85 and s['span']==75 and s['boot']==boot+1 and s['page']=='tank')
        s=cmd('select 1'); check('Owner, pet stats and history persist',s['name']=='GOOSE' and s['health']==91 and cmd('history')['history']==goose_history)
        cmd('select 0'); check('Both histories survive restart separately',cmd('history')['history']==captain_history)
        for bad in ('select 9','select x','sample -1','sample 101','join BAD! 0','join TOOLONGNAME 0','join CAPTAIN 2','night new','x'*80):
            check('Reject invalid input: '+bad[:25],cmd(bad,'ERROR').startswith('ERROR'))
        for name,kind in [('BEAN',2),('CHAOS',3),('PICKLE',4),('NUGGET',5)]: s=cmd(f'join {name} {kind}')
        check('Tank supports six separate players',s['players']==6)
        check('Seventh player is rejected',cmd('join WAFFLES 0','ERROR').startswith('ERROR'))
        check('Game rules, saturation and per-person ring-buffer self-test',cmd('selftest','SELFTEST').startswith('SELFTEST PASS'))
        cmd('cal default'); cmd('tank')
        before=cmd('status'); drain(3); after=cmd('status')
        check('Full tank animates without reboot',after['frames']>before['frames']+20 and after['boot']==before['boot'])
        # Verify explicit destructive confirmation and a safe default.
        cmd('ui menu'); cmd('ui next'); cmd('ui next'); s=cmd('ui select')
        check('New evening opens confirmation with keep selected',s['page']=='new_night' and s['cursor']==0)
        s=cmd('ui select'); check('Default confirmation preserves all pets',s['players']==6 and s['page']=='menu')
        cmd('ui next'); cmd('ui next'); cmd('ui next'); s=cmd('ui select')
        check('Shared menu selector opens MQ-3 bench test',s['page']=='sensor' and s['mq3_active'])
        check('Early sensor baseline is rejected',cmd('sensor zero','ERROR').startswith('ERROR'))
        drain(1.2); s=cmd('status')
        check('Sensor monitor samples ADC while game input stays simulated',s['mq3_samples']>=8 and 0<=s['mq3_adc']<=4095 and 0<=s['mq3_mv']<=3300 and s['sensor']=='SIMULATED')
        check('Bench readings never create pet history',s['feeds']==0 and s['history_count']==0)
        cmd('ui next'); s=cmd('ui select'); check('Baseline clear action leaves no zero',s['mq3_baseline_mv']==-1)
        cmd('ui next'); s=cmd('ui select'); check('Monitor exit stops acquisition',s['page']=='menu' and not s['mq3_active'])
        n=s['mq3_samples']; drain(.3); check('ADC stays stopped outside bench screen',cmd('status')['mq3_samples']==n)
        if not args.keep_demo_roster:
            cmd('ui select') # Back to tank (menu reset cursor is zero).
            cmd('night new CONFIRM')
            s=cmd('status'); check('Confirmed new evening clears roster',s['players']==0 and s['page']=='tank')
        else: cmd('tank')
        report['final_status']=cmd('status')
        report['result']='PASS'
        report['physical_checks']='Serial UI commands exercise the same handlers as physical buttons; screen and actual buttons still need a user check. Touch is not implied by passing tests.'
except Exception as exc:
    report['result']='FAIL'; report['error']=str(exc); raise
finally:
    output=Path(__file__).resolve().parents[1]/'test-results.json'
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Report: {output}',flush=True)
