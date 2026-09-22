"""Export the ESP32's rendered framebuffer, not a photo or panel readback."""
import argparse
import json
import time
from pathlib import Path
import serial
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('output',type=Path)
p.add_argument('--port',default='COM3')
p.add_argument('--command',action='append',default=[])
args=p.parse_args()
device=serial.Serial(None,115200,timeout=2,write_timeout=2)
device.dtr=False
device.rts=False
device.port=args.port
device.open()
with device:
    end=time.monotonic()+1
    while time.monotonic()<end: device.readline()
    for request_id,command in enumerate(args.command,1):
        device.write((f'@{request_id} '+command+'\n').encode())
        end=time.monotonic()+3
        acknowledged=False
        while time.monotonic()<end:
            line=device.readline()
            if line.startswith(b'CMD'): acknowledged=line.strip()==f'CMD {request_id}'.encode()
            elif acknowledged and line.startswith(b'ERROR'): raise RuntimeError(line.decode().strip())
            elif acknowledged and line.startswith(b'{') and json.loads(line).get('request_id')==request_id: break
        else: raise RuntimeError('Command not acknowledged: '+command)
    device.reset_input_buffer(); device.write(b'screen\n')
    end=time.monotonic()+5
    while time.monotonic()<end:
        line=device.readline()
        if line.startswith(b'FRAME '): break
    else: raise RuntimeError('No framebuffer header')
    assert line.strip()==b'FRAME 320 170 RGB565BE',line
    device.timeout=10
    raw=device.read(320*170*2)
    assert len(raw)==320*170*2,f'Truncated frame: {len(raw)} bytes'
    pixels=[]
    for i in range(0,len(raw),2):
        color=int.from_bytes(raw[i:i+2],'big')
        pixels.append((((color>>11)&31)*255//31,((color>>5)&63)*255//63,(color&31)*255//31))
    image=Image.new('RGB',(320,170)); image.putdata(pixels)
    args.output.parent.mkdir(parents=True,exist_ok=True); image.save(args.output)
    print(args.output)
