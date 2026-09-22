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
    device=serial.Serial(None,115200,timeout=.3,write_timeout=2)
    device.dtr=False; device.rts=False; device.port=args.port; device.open()
    with device:
        def drain(seconds=1):
            end=time.monotonic()+seconds
            while time.monotonic()<end: device.readline()

        request_counter=[0]
        def cmd(text,marker='{'):
            request_counter[0]+=1; request_id=request_counter[0]
            # CMD frames replies; purging USB between requests can discard in-flight packets.
            device.write((f'@{request_id} '+text+'\n').encode())
            end=time.monotonic()+5
            acknowledged=False
            pending=b''
            received=[]
            acknowledged_at=0; queried=False
            while time.monotonic()<end:
                if acknowledged and not queried and not pending and marker=='{' and time.monotonic()-acknowledged_at>1:
                    # Recover a missing USB reply with a read, never by replaying an action.
                    query='history' if text=='history' else 'status'
                    device.write((f'@{request_id} '+query+'\n').encode()); queried=True
                    report.setdefault('read_only_reply_recovery',[]).append(text)
                pending+=device.readline()
                # A USB read timeout can return part of a line; wait for its newline.
                if not pending.endswith(b'\n'): continue
                line=pending.decode(errors='replace').strip()
                pending=b''
                received.append(line)
                if line.startswith('CMD'):
                    acknowledged=line==f'CMD {request_id}'
                    if acknowledged: acknowledged_at=time.monotonic()
                elif acknowledged and line.startswith('ERROR') and marker!='ERROR':
                    raise RuntimeError(f'{text}: {line}')
                elif acknowledged and line.startswith(marker):
                    try:
                        result=json.loads(line) if marker=='{' else line
                        if marker=='{' and result.get('request_id')!=request_id: continue
                        report['last_reply']={'command':text,'result':result}
                        return result
                    except json.JSONDecodeError:
                        report['invalid_reply']={'command':text,'line':line}
                        raise
            report['timeout_reply']={'command':text,'lines':received[-4:],'partial':pending.decode(errors='replace')}
            raise TimeoutError(text)

        drain(2)
        check('Isolated test storage enabled',cmd('status').get('test_mode') is True)
        cmd('night new CONFIRM'); cmd('test fun reset'); cmd('cal default')
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
        s=cmd('ui select'); check('Feed begins a five-second countdown',s['page']=='countdown')
        drain(15.4); s=cmd('status')
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
        s=cmd('sample 85'); check('High response gives wild energy without damage',s['health']==100 and s['food']==85 and s['joy']==82 and s['score']==85 and s['state']=='WILD')
        goose_history=cmd('history')['history']
        s=cmd('select 0'); check('Other person is unaffected',s['health']==100 and s['food']==85 and s['feeds']==1)
        check('Other person history is unaffected',cmd('history')['history']==captain_history)
        cmd('select 1'); s=cmd('rest'); check('Nap sleeps without damage or a sample',s['health']==100 and s['feeds']==1 and s['state']=='ASLEEP')
        s=cmd('rest'); check('Rest is debounced with cooldown',s['health']==100)
        s=cmd('cal zero'); check('Calibration zero uses last fake raw sample',s['zero']==85)
        s=cmd('cal minus'); check('Calibration span can change',s['span']==75)
        check('Stored reading keeps score at capture',cmd('history')['history']==goose_history)
        boot=s['boot']; cmd('reboot','REBOOT'); device.close(); time.sleep(2); device.open(); drain(2)
        s=cmd('status'); check('Roster and calibration survive restart',s['players']==2 and s['zero']==85 and s['span']==75 and s['boot']==boot+1 and s['page']=='tank')
        s=cmd('select 1'); check('Owner, pet stats and history persist',s['name']=='GOOSE' and s['health']==100 and cmd('history')['history']==goose_history)
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
        cmd('ui next'); s=cmd('ui select'); check('Demo bench exit stops acquisition',s['page']=='menu' and not s['mq3_active'])
        n=s['mq3_samples']; drain(.3); check('Demo mode keeps ADC stopped outside bench screen',cmd('status')['mq3_samples']==n)
        cmd('test sensor 100'); cmd('input live'); s=cmd('cal default')
        check('Live default is the gentler 1200 mV span',s['sensor_span_mv']==1200)
        check('Live acquisition starts in the background on the tank',s['page']=='tank' and s['mq3_active'] and not s['feed_ready'])
        cmd('select 0'); before=cmd('status'); s=cmd('ui select')
        check('Unsettled input only shows recovery when feeding is requested',s['page']=='feed' and s['feeds']==before['feeds'])
        s=cmd('ui back'); check('Pending recovery can be cancelled',s['page']=='pet')
        drain(11.2); s=cmd('status'); check('Clean-air window settles while viewing a pet',s['page']=='pet' and s['feed_ready'] and s['mq3_samples']>=100)
        n=s['mq3_samples']; s=cmd('ui select')
        check('One Feed press starts countdown using background baseline',s['page']=='countdown' and s['feed_baseline_mv']==100 and s['mq3_samples']>=n)
        cmd('test sensor 900'); drain(3); s=cmd('status'); check('Countdown ignores early exposure',s['page']=='countdown' and s['feed_peak_mv']==100)
        cmd('test sensor 400'); drain(2.3); s=cmd('status'); check('Five seconds opens BLOW capture',s['page']=='sampling')
        cmd('test sensor 400'); drain(10.4); s=cmd('status')
        check('Measured rise feeds the correct pet once',s['page']=='result' and s['feeds']==before['feeds']+1 and s['score']==23 and s['health']==before['health'])
        live_history=cmd('history')['history']; r=live_history[0]
        check('Real history stores provenance and original voltages',r['source']=='MQ3' and r['baseline_mv']==100 and r['peak_mv']==400 and r['span_mv']==1200)
        check('Old demo history stays explicitly demo',live_history[1]['source']=='DEMO')
        check('Fake serial feed is rejected in live mode',cmd('sample 85','ERROR').startswith('ERROR'))
        n=s['mq3_samples']; drain(.4); s=cmd('status'); check('Sensor keeps recovering on the result screen',s['page']=='result' and s['mq3_active'] and s['mq3_samples']>n)
        cmd('select 1'); check('Other owner keeps original history',cmd('history')['history']==goose_history)
        cmd('ui select'); drain(11.2); s=cmd('status')
        check('Lingering vapor cannot feed the next owner',s['page']=='feed' and s['recovering'] and s['feeds']==1)
        cmd('test sensor 100'); drain(11.2); s=cmd('status')
        check('Queued feeding starts automatically after recovery',s['page']=='countdown')
        cmd('ui back'); drain(.3); s=cmd('status'); check('Cancelling never records a live feed',s['page']=='pet' and s['feeds']==1)
        cmd('ui select'); drain(5.2); cmd('test sensor high'); drain(.4); s=cmd('status')
        check('Out-of-range input aborts without history',s['page']=='feed' and s['feeds']==1)
        check('Rejected live samples leave history unchanged',cmd('history')['history']==goose_history)
        cmd('test sensor 100'); drain(11.2); s=cmd('status'); check('Rejected captures do not retry automatically',s['page']=='feed' and s['feeds']==1 and s['feed_ready'])
        cmd('ui select'); s=cmd('ui back'); check('Explicit retry starts and can be cancelled',s['page']=='pet' and s['feeds']==1)
        cmd('test sensor off'); boot=s['boot']; cmd('reboot','REBOOT'); device.close(); time.sleep(2); device.open(); drain(2)
        cmd('select 0'); check('Live and demo provenance survive restart',cmd('history')['history']==live_history)
        # The entire party loop uses the physical button handlers, with test-only time/bubble controls.
        s=cmd('status'); check('First check-in awards and equips a cup',s['items']&2 and s['checkins']==1)
        check('Shared check-ins unlock tank toys',s['upgrades']&1)
        history_before_play=cmd('history')['history']
        cmd('test minutes 20'); s=cmd('status'); check('Neglected pet sleeps without health loss',s['state']=='ASLEEP' and s['energy']==0 and s['health']==100)
        cmd('select 0')
        for _ in range(4): cmd('ui next')
        s=cmd('ui select'); check('Play is reachable through pet actions',s['page']=='play')
        cmd('test bubble 0'); s=cmd('ui select'); check('Bubble miss does not award a catch',s['catches']==0)
        cmd('test bubble 50')
        for _ in range(3): s=cmd('ui select')
        check('Three catches wake and energize the pet',s['page']=='pet' and s['energy']==100 and s['games']==1 and s['state']=='PARTY')
        check('Playing awards a spaced check-in without a sensor sample',s['checkins']==2 and cmd('history')['history']==history_before_play)
        previous=s['checkins']
        for _ in range(4): cmd('ui next')
        cmd('ui select')
        for _ in range(3): s=cmd('ui select')
        check('Repeated play cannot farm rewards inside ten minutes',s['games']==2 and s['checkins']==previous and s['loot']==0)
        cmd('test minutes 9'); s=cmd('status'); check('Pet stays awake before ten minutes',s['idle_minutes']==9 and s['energy']==55)
        cmd('test minutes 1'); s=cmd('status'); check('Ten minutes makes pet drowsy',s['state']=='DROWSY' and s['reward_ready'])
        cmd('test minutes 10'); s=cmd('status'); check('Twenty minutes makes pet sleep',s['state']=='ASLEEP' and s['naps']>=2)
        # A zero reading is a complete check-in and has identical loot eligibility.
        if s['cooldown_ms']: drain(s['cooldown_ms']/1000+.15) # Accelerated game time does not skip the real reboot cooldown.
        s=cmd('sample 0'); check('Zero response wakes and earns third check-in',s['state']=='CHILL' and s['energy']==100 and s['checkins']==3)
        check('Third spaced check-in guarantees a hat',s['hats']>1)
        cmd('select 0'); cmd('ui next'); cmd('ui next'); cmd('ui next'); s=cmd('ui select')
        check('Wardrobe opens from pet menu',s['page']=='wardrobe')
        original_hat=s['hat']; s=cmd('ui select'); check('Wardrobe cycles only unlocked hats',s['hat']!=original_hat and s['hats']&(1<<s['hat']))
        cmd('ui next'); s=cmd('ui select'); check('Hand item can be changed independently',s['items']&(1<<s['item']))
        closet={k:s[k] for k in ('hats','items','colours','hat','item','colour','games','checkins','naps','upgrades')}
        cmd('ui menu')
        for _ in range(5): cmd('ui next')
        s=cmd('ui select'); check('Evening awards are reachable',s['page']=='awards')
        s=cmd('ui next'); check('Award selection cycles',s['cursor']==1)
        cmd('ui select')
        for _ in range(6): cmd('ui next')
        s=cmd('ui select'); check('Shared tank upgrades are reachable',s['page']=='decor')
        cmd('reboot','REBOOT'); device.close(); time.sleep(2); device.open(); drain(2)
        s=cmd('select 0'); check('Clothes and party progress survive restart',all(s[k]==v for k,v in closet.items()) and s['fun_storage_ok'])
        cmd('ui menu'); cmd('ui next'); cmd('ui next'); cmd('ui select')
        s=cmd('ui select')
        check('Keep evening preserves earned clothes and decorations',all(s[k]==v for k,v in closet.items()))
        settings={k:s[k] for k in ('zero','span','sensor_span_mv')}
        cmd('ui next'); cmd('ui next'); cmd('ui select'); cmd('ui next'); s=cmd('ui select')
        check('Confirmed UI reset clears all pets and tank decorations',s['players']==0 and s['upgrades']==0 and s['page']=='tank' and s['cursor']==6)
        check('Evening reset preserves device sensitivity',all(s[k]==v for k,v in settings.items()))
        cmd('reboot','REBOOT'); device.close(); time.sleep(2); device.open(); drain(2)
        s=cmd('status'); check('Empty evening and locked decorations survive reboot',s['players']==0 and s['upgrades']==0 and s['fun_storage_ok'] and s['storage_ok'])
        s=cmd('join CAPTAIN 1')
        check('Returning nickname starts with an empty wardrobe',all(s[k]==1 for k in ('hats','items','colours','visits')) and all(s[k]==0 for k in ('hat','item','colour')))
        check('New evening clears all personal progress and readings',all(s[k]==0 for k in ('checkins','naps','games','encounters','history_count','feeds','idle_minutes')) and s['reward_ready'])
        s=cmd('sample 0')
        check('Fresh first reward gives a cup without old shared progress',s['items']==3 and s['item']==1 and s['checkins']==1 and s['upgrades']==0)
        s=cmd('join GOOSE 2')
        check('Reset also clears other nickname collections',all(s[k]==1 for k in ('hats','items','colours','visits')) and all(s[k]==0 for k in ('hat','item','colour','history_count')))
        cmd('ui menu')
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
