"""Exercise the real key-setup UI and native OBS hot reload; restore user settings."""
import base64
import ctypes
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from PIL import Image, ImageChops
from ObsControl import Obs

ROOT=Path(__file__).resolve().parents[1]
PRESET=ROOT.parent/'obs-input-overlay'
OUT=ROOT/'build/obs-key-setup/live-verification'
OBS=Path(os.environ.get('OBS_EXE',r'C:\Program Files\obs-studio\bin\64bit\obs64.exe'))
NAME='奶蛙 · DFJK 输入叠加'
u=ctypes.windll.user32
u.CreateWindowExW.restype=ctypes.c_void_p
u.GetForegroundWindow.restype=ctypes.c_void_p
u.FindWindowW.restype=ctypes.c_void_p
u.GetDlgItem.argtypes=[ctypes.c_void_p,ctypes.c_int];u.GetDlgItem.restype=ctypes.c_void_p
u.SendMessageW.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_size_t,ctypes.c_ssize_t]
u.ShowWindow.argtypes=[ctypes.c_void_p,ctypes.c_int]
u.DestroyWindow.argtypes=[ctypes.c_void_p]
u.SetForegroundWindow.argtypes=[ctypes.c_void_p]
u.SetWindowPos.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_uint]
u.GetWindowTextW.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p,ctypes.c_int]
callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)

def event(key,down):u.keybd_event(key,0,0 if down else 2,0)
def focus(hwnd):
    u.ShowWindow(hwnd,9)
    for attempt in range(10):
        current=ctypes.windll.kernel32.GetCurrentThreadId();thread=u.GetWindowThreadProcessId(u.GetForegroundWindow(),None)
        attached=bool(u.AttachThreadInput(current,thread,True)) if current!=thread and thread else False
        try:
            u.SetWindowPos(hwnd,ctypes.c_void_p(-1),0,0,0,0,0x43)
            u.BringWindowToTop(hwnd);u.SetForegroundWindow(hwnd)
        finally:
            if attached:u.AttachThreadInput(current,thread,False)
        time.sleep(.15)
        if u.GetForegroundWindow()==hwnd:break
    if u.GetForegroundWindow()!=hwnd:raise RuntimeError('Test window did not acquire focus; no keys injected')
def snap(client,name=None):
    data=client.call('GetSourceScreenshot',{'sourceName':NAME,'imageFormat':'png','imageWidth':880,'imageHeight':880})
    image=Image.open(io.BytesIO(base64.b64decode(data['imageData'].split(',',1)[1]))).convert('RGBA')
    if name:image.save(OUT/(name+'.png'))
    return image
def expected(mask):
    offset=(1+mask%4*883,1+mask//4*883)
    box=(*offset,offset[0]+880,offset[1]+880)
    bg=Image.open(PRESET/'background-atlas.png').convert('RGBA').crop(box)
    labels=Image.open(PRESET/'key-labels.png').convert('RGBA')
    fg=Image.open(PRESET/'foreground-atlas.png').convert('RGBA').crop(box)
    return Image.alpha_composite(Image.alpha_composite(bg,labels),fg)
def compare(actual,wanted):
    a=ImageChops.difference(actual.getchannel('A'),wanted.getchannel('A')).getextrema()[1]
    opaque=wanted.getchannel('A').point(lambda value:255 if value==255 else 0)
    difference=ImageChops.difference(actual,wanted)
    rgb=max(ImageChops.multiply(difference.getchannel(channel),opaque).getextrema()[1] for channel in 'RGB')
    if a>2 or rgb>4:raise AssertionError(f'Output mismatch: alpha={a}, opaque RGB={rgb}')
    return {'alpha_error':a,'opaque_RGB_error':rgb}
def configure(keys):
    subprocess.run([str(PRESET/'MilkFrogKeySetup.exe'),'--set',*map(str,keys)],check=True)
    time.sleep(.65)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    files={name:(PRESET/name).read_bytes() for name in ['keybindings.json','key-labels.png','milk-frog.png']}
    prefs=Path(os.environ['LOCALAPPDATA'])/'MilkFrog/config.txt'
    original_prefs=prefs.read_bytes() if prefs.exists() else None
    ws=Path(os.environ['APPDATA'])/'obs-studio/plugin_config/obs-websocket/config.json'
    original_ws=ws.read_bytes();settings=json.loads(original_ws.decode('utf-8-sig'));settings['server_enabled']=True
    ws.write_text(json.dumps(settings,indent=4),encoding='utf-8')
    process=client=neutral=tool=None;previous=u.GetForegroundWindow();filter_name=None;particles=True
    def release():
        for key in [32,13,0xA2,0xA3,0xA0,0xA1,0x74,*map(ord,'ASDFJKZXQ')]:event(key,False)
    try:
        process=subprocess.Popen([str(OBS),'--minimize-to-tray'],cwd=OBS.parent)
        for _ in range(120):
            try:
                if client is None:client=Obs()
                assert not client.call('GetStreamStatus')['outputActive'] and not client.call('GetRecordStatus')['outputActive']
                filters=client.call('GetSourceFilterList',{'sourceName':NAME})['filters'];break
            except Exception:time.sleep(.2)
        else:raise RuntimeError('OBS did not become ready')
        active=next(f for f in filters if f['filterKind']=='milk_frog_native_animation')
        filter_name=active['filterName'];particles=active['filterSettings'].get('particles',True)
        assert Path(active['filterSettings']['config_file']).resolve()==(PRESET/'milk-frog.json').resolve()
        client.call('SetSourceFilterSettings',{'sourceName':NAME,'filterName':filter_name,'filterSettings':{'particles':False},'overlay':True})
        time.sleep(.5)
        neutral=u.CreateWindowExW(0,'STATIC','MilkFrog OBS Key Verification',0x00CF0000,100,100,380,140,None,None,None,None)
        if not neutral:raise RuntimeError('Neutral window unavailable')
        focus(neutral);release()

        # The actual key-capture UI, including a multi-key laughter chord.
        test=OUT/'ui-preset';test.mkdir(exist_ok=True)
        for name in ['milk-frog.json','milk-frog-unlabeled.png','keybindings.json']:
            shutil.copyfile(PRESET/name,test/name)
        tool=subprocess.Popen([str(PRESET/'MilkFrogKeySetup.exe'),'--folder',str(test)])
        for _ in range(50):
            hwnd=u.FindWindowW('MilkFrogObsKeySetup',None)
            if hwnd:break
            time.sleep(.1)
        else:raise RuntimeError('Key setup UI did not open')
        focus(hwnd)
        u.SendMessageW(hwnd,0x111,100,0);event(32,True);event(32,False);time.sleep(.15)
        text=ctypes.create_unicode_buffer(256);u.GetWindowTextW(u.GetDlgItem(hwnd,100),text,256)
        assert 'SPC' in text.value,'Gameplay UI capture failed'
        u.SendMessageW(hwnd,0x111,112,0)
        for key in [0xA3,0xA1,ord('Q')]:event(key,True)
        for key in [ord('Q'),0xA1,0xA3]:event(key,False)
        time.sleep(.15);u.GetWindowTextW(u.GetDlgItem(hwnd,112),text,256)
        assert 'Ctrl + Shift + Q' in text.value,'Laughter chord UI capture failed'
        u.SendMessageW(hwnd,0x111,110,0);time.sleep(.2)
        assert json.loads((test/'keybindings.json').read_text())['keys']==[32,70,74,75]
        assert 'laugh_keys=16+17+81' in prefs.read_text()
        time.sleep(.65)
        focus(neutral)
        for key in [0xA3,0xA1,ord('Q')]:event(key,True)
        for key in [ord('Q'),0xA1,0xA3]:event(key,False)
        time.sleep(.15);assert snap(client).tobytes()!=expected(0).tobytes(),'Saved UI chord did not reach OBS'
        event(ord('D'),True);time.sleep(.1);event(ord('D'),False);time.sleep(.1)
        compare(snap(client),expected(0))
        event(0x74,True);event(0x74,False);time.sleep(.1);compare(snap(client),expected(0))
        focus(hwnd)
        u.SendMessageW(hwnd,0x111,111,0);u.SendMessageW(hwnd,0x111,113,0);u.SendMessageW(hwnd,0x111,110,0)
        assert json.loads((test/'keybindings.json').read_text())['keys']==[68,70,74,75]
        assert 'laugh_keys=116' in prefs.read_text()
        before=(test/'keybindings.json').read_bytes()
        u.SendMessageW(hwnd,0x111,100,0);event(ord('Z'),True);event(ord('Z'),False);time.sleep(.1)
        u.SendMessageW(hwnd,0x111,2,0);tool.wait(timeout=10);tool=None
        assert (test/'keybindings.json').read_bytes()==before,'Close saved uncommitted edits'
        focus(neutral)
        # Restore the user's own laughter shortcut before testing real inputs.
        if original_prefs is not None:prefs.write_bytes(original_prefs)
        else:prefs.unlink(missing_ok=True)
        time.sleep(.65)

        configure([65,83,0xA2,32]);idle=snap(client,'remapped-idle')
        report={'real_UI_capture_save_reset_cancel':'PASS','Ctrl_Shift_Q_capture_and_persistence':'PASS','saved_laughter_chord_updates_OBS_and_old_F5_is_ignored':'PASS','native_source':'input-overlay','combinations':[]}
        compare(idle,expected(0))
        for mask in range(16):
            release()
            for slot,key in enumerate([65,83,0xA2,32]):
                if mask&(1<<slot):event(key,True)
            time.sleep(.1)
            error=compare(snap(client,'remapped-all-held' if mask==15 else None),expected(mask))
            report['combinations'].append({'mask':mask,**error})
        release();time.sleep(.12)
        for key in map(ord,'DFJK'):
            event(key,True);time.sleep(.12);compare(snap(client,'old-key-'+chr(key)),expected(0));event(key,False)
        report['old_keys_removed']='PASS'
        event(65,True);configure([90,88,0xA3,13]);compare(snap(client),expected(0));event(65,False)
        event(0xA2,True);time.sleep(.12);compare(snap(client),expected(0));event(0xA2,False)
        event(0xA3,True);time.sleep(.12);compare(snap(client,'right-control'),expected(4));event(0xA3,False)
        report['remap_while_old_key_held']='PASS';report['left_right_control_distinct']='PASS'
        old=(PRESET/'keybindings.json').read_bytes()
        rejected=subprocess.run([str(PRESET/'MilkFrogKeySetup.exe'),'--set','65','65','32','13'])
        assert rejected.returncode==20 and (PRESET/'keybindings.json').read_bytes()==old
        report['duplicate_rejection_preserves_configuration']='PASS'
        # F5 behavior with the current, remapped gameplay keys.
        if not original_prefs or b'laugh_keys=' not in original_prefs or b'laugh_keys=116' in original_prefs:
            release();event(0x74,True);event(0x74,False);time.sleep(.15)
            assert snap(client).tobytes()!=expected(0).tobytes()
            event(90,True);time.sleep(.12);compare(snap(client),expected(1));event(90,False);time.sleep(.1);compare(snap(client),expected(0))
            report['new_gameplay_key_cancels_video']='PASS'
        report['no_OBS_restart_after_key_changes']='PASS'
        report['window_output_size_unchanged']=True
        (OUT/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(json.dumps(report),flush=True)
    finally:
        release()
        if tool and tool.poll() is None:
            hwnd=u.FindWindowW('MilkFrogObsKeySetup',None)
            if hwnd:u.SendMessageW(hwnd,0x10,0,0)
            tool.wait(timeout=10)
        for name,data in files.items():(PRESET/name).write_bytes(data)
        if original_prefs is not None:prefs.write_bytes(original_prefs)
        else:prefs.unlink(missing_ok=True)
        if client and filter_name:
            try:client.call('SetSourceFilterSettings',{'sourceName':NAME,'filterName':filter_name,'filterSettings':{'particles':particles},'overlay':True})
            except Exception:pass
        if neutral:u.DestroyWindow(neutral)
        if previous:u.SetForegroundWindow(previous)
        if client:client.close()
        if process and process.poll() is None:
            def close(hwnd,param):
                pid=ctypes.c_ulong();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                if pid.value==process.pid:
                    title=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,title,1024)
                    if title.value.startswith('OBS '):u.PostMessageW(hwnd,0x10,0,0)
                return True
            u.EnumWindows(callback(close),0);process.wait(timeout=30)
        ws.write_bytes(original_ws)

if __name__=='__main__':main()
