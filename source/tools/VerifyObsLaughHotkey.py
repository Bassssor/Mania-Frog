"""Verify native OBS combo hot reload without recording or modifying scenes."""
import ctypes,json,os,subprocess,sys,time
from pathlib import Path
from PIL import Image
from ObsControl import Obs
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent/'verification/laugh-hotkey'
OBS=Path(r'C:\Program Files\obs-studio\bin\64bit\obs64.exe')
NAME='奶蛙 · DFJK 输入叠加'
u=ctypes.windll.user32
u.CreateWindowExW.restype=ctypes.c_void_p
u.GetForegroundWindow.restype=ctypes.c_void_p
u.SetForegroundWindow.argtypes=[ctypes.c_void_p]
u.DestroyWindow.argtypes=[ctypes.c_void_p]
u.ShowWindow.argtypes=[ctypes.c_void_p,ctypes.c_int]
callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
def event(key,down):u.keybd_event(key,0,0 if down else 2,0)
def snap(client,name):
    path=OUT/(name+'.png')
    client.call('SaveSourceScreenshot',{'sourceName':NAME,'imageFormat':'png','imageFilePath':str(path),'imageWidth':880,'imageHeight':880})
    return Image.open(path).convert('RGBA').tobytes()
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    prefs=Path(os.environ['LOCALAPPDATA'])/'MilkFrog/config.txt';original_prefs=prefs.read_bytes()
    websocket=Path(os.environ['APPDATA'])/'obs-studio/plugin_config/obs-websocket/config.json'
    original_ws=websocket.read_bytes();settings=json.loads(original_ws.decode('utf-8-sig'));settings['server_enabled']=True
    websocket.write_text(json.dumps(settings,indent=4),encoding='utf-8')
    process=None;client=None;window=None;previous_window=u.GetForegroundWindow()
    def change(chord):
        lines=[line for line in original_prefs.decode('utf-8-sig').splitlines() if not line.startswith('laugh_keys=')]
        temporary=prefs.with_suffix('.txt.verify.tmp');temporary.write_text('\n'.join(lines)+f'\nlaugh_keys={chord}\n',encoding='utf-8');temporary.replace(prefs);time.sleep(.65)
    def cancel():event(ord('D'),True);time.sleep(.03);event(ord('D'),False);time.sleep(.65)
    try:
        process=subprocess.Popen([str(OBS),'--minimize-to-tray'],cwd=OBS.parent)
        for _ in range(120):
            try:
                if client is None:client=Obs()
                assert not client.call('GetStreamStatus')['outputActive']
                client.call('GetCurrentProgramScene');break
            except Exception:time.sleep(.2)
        else:raise RuntimeError('OBS frontend not ready')
        window=u.CreateWindowExW(0,'STATIC','MilkFrog Hotkey Verification',0x00CF0000,100,100,310,110,None,None,None,None)
        if not window:raise RuntimeError('Neutral test window unavailable')
        u.ShowWindow(window,9)
        current=ctypes.windll.kernel32.GetCurrentThreadId();foreground=u.GetWindowThreadProcessId(u.GetForegroundWindow(),None)
        attached=bool(u.AttachThreadInput(current,foreground,True)) if current!=foreground else False
        try:u.SetForegroundWindow(window)
        finally:
            if attached:u.AttachThreadInput(current,foreground,False)
        if u.GetForegroundWindow()!=window:raise RuntimeError('Test window not foreground; no test keys injected')
        change('17+81');idle=snap(client,'obs-idle')
        event(ord('Q'),True);event(ord('Q'),False);time.sleep(.12)
        assert snap(client,'obs-q-alone')==idle,'Q alone fired'
        event(0xA2,True);event(ord('Q'),True);time.sleep(.12)
        assert snap(client,'obs-ctrl-q')!=idle,'Ctrl+Q did not fire'
        event(ord('Q'),False);event(ord('Q'),True);time.sleep(.06)
        assert snap(client,'obs-ctrl-q-restart')!=idle,'Combo restart failed'
        event(ord('Q'),False);event(0xA2,False);cancel()
        assert snap(client,'obs-cancelled')==idle,'Gameplay cancellation failed'
        change('16+17+81')
        event(0xA3,True);event(ord('Q'),True);time.sleep(.12)
        assert snap(client,'obs-old-chord')==idle,'Old chord still fired after reload'
        event(ord('Q'),False);event(0xA3,False)
        event(0xA3,True);event(0xA1,True);event(ord('Q'),True);time.sleep(.12)
        assert snap(client,'obs-ctrl-shift-q')!=idle,'Three-key chord did not fire'
        for key in [ord('Q'),0xA3,0xA1]:event(key,False)
        cancel()
        assert snap(client,'obs-restored')==idle
        report={'native_source':'input-overlay','Ctrl+Q':'PASS','Q_alone_ignored':'PASS','restart':'PASS',
            'old_chord_ignored_after_reload':'PASS','Ctrl+Shift+Q_right_modifiers':'PASS','no_OBS_restart_for_settings':'PASS',
            'gameplay_cancellation_and_exact_restoration':'PASS'}
        (OUT/'obs-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report),flush=True)
    finally:
        for key in [ord('Q'),ord('D'),0xA2,0xA3,0xA0,0xA1]:event(key,False)
        prefs.write_bytes(original_prefs)
        if window:u.DestroyWindow(window)
        if previous_window:u.SetForegroundWindow(previous_window)
        if client:client.close()
        if process and process.poll() is None:
            def close(hwnd,param):
                pid=ctypes.c_ulong();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                if pid.value==process.pid:
                    title=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,title,1024)
                    if title.value.startswith('OBS '):u.PostMessageW(hwnd,0x0010,0,0)
                return True
            u.EnumWindows(callback(close),0);process.wait(timeout=30)
        websocket.write_bytes(original_ws)
if __name__=='__main__':main()
