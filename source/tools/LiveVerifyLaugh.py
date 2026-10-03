"""Local OBS integration verification; restore settings and scene afterwards."""
import ctypes,json,os,subprocess,sys,time,hashlib
from pathlib import Path
from PIL import Image,ImageChops
from ObsControl import Obs
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent/'verification/laugh-playback'
OBS=Path(r'C:\Program Files\obs-studio\bin\64bit\obs64.exe')
NAME='奶蛙 · DFJK 输入叠加'
AUDIO='奶蛙 · F5 笑声'
u=ctypes.windll.user32
callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
def press(key):u.keybd_event(key,0,0,0)
def release(key):u.keybd_event(key,0,2,0)
def tap(key):press(key);time.sleep(.02);release(key)
def snapshot(client,name):
    path=OUT/(name+'.png')
    client.call('SaveSourceScreenshot',{'sourceName':NAME,'imageFormat':'png','imageFilePath':str(path),'imageWidth':880,'imageHeight':880})
    return Image.open(path).convert('RGBA')
def same(a,b):return a.tobytes()==b.tobytes()
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    config=Path(os.environ['APPDATA'])/'obs-studio/plugin_config/obs-websocket/config.json'
    original=config.read_bytes();settings=json.loads(original.decode('utf-8-sig'));settings['server_enabled']=True
    config.write_text(json.dumps(settings,indent=4),encoding='utf-8')
    process=None;client=None;muted=[];scene=None;temporary=None;recording=False;record_directory=None
    report={}
    try:
        process=subprocess.Popen([str(OBS),'--minimize-to-tray'],cwd=OBS.parent)
        for _ in range(100):
            try:client=Obs();break
            except Exception:time.sleep(.2)
        if client is None:raise RuntimeError('OBS API did not initialize')
        for _ in range(150):
            try:
                streaming=client.call('GetStreamStatus')['outputActive']
                recording_status=client.call('GetRecordStatus')['outputActive'];break
            except Exception:time.sleep(.2)
        else:raise RuntimeError('OBS frontend did not initialize')
        assert not streaming and not recording_status
        scene=client.call('GetCurrentProgramScene')['currentProgramSceneName']
        # Keep injected test keys away from the user's other applications.
        windows=[]
        def visit(hwnd,param):
            pid=ctypes.c_ulong();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
            if pid.value==process.pid and u.IsWindowVisible(hwnd):windows.append(hwnd)
            return True
        u.EnumWindows(callback(visit),0)
        if not windows:
            def visit_any(hwnd,param):
                pid=ctypes.c_ulong();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                if pid.value==process.pid:
                    title=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,title,1024)
                    if title.value.startswith('OBS '):windows.append(hwnd)
                return True
            u.EnumWindows(callback(visit_any),0)
        if not windows:raise RuntimeError('OBS main window unavailable')
        hwnd=windows[0];u.ShowWindow(hwnd,9)
        current=ctypes.windll.kernel32.GetCurrentThreadId()
        foreground=u.GetWindowThreadProcessId(u.GetForegroundWindow(),None)
        attached=bool(u.AttachThreadInput(current,foreground,True)) if foreground!=current else False
        try:u.BringWindowToTop(hwnd);u.SetForegroundWindow(hwnd)
        finally:
            if attached:u.AttachThreadInput(current,foreground,False)
        time.sleep(.3)
        pid=ctypes.c_ulong();u.GetWindowThreadProcessId(u.GetForegroundWindow(),ctypes.byref(pid))
        if pid.value!=process.pid:raise RuntimeError('OBS foreground was not established; no keys injected')
        idle=snapshot(client,'obs-idle')
        tap(0x74);time.sleep(.12);first=snapshot(client,'obs-video-start')
        assert not same(idle,first),'F5 did not start video'
        time.sleep(1.0);progress=snapshot(client,'obs-video-progress')
        assert not same(first,progress),'Video did not advance'
        tap(0x74);time.sleep(.05);restarted=snapshot(client,'obs-video-restarted')
        assert not same(progress,restarted),'F5 did not restart'
        for key in [ord('D'),ord('F'),ord('J'),ord('K')]:
            tap(0x74);time.sleep(.12);press(key);time.sleep(.05)
            snapshot(client,'obs-cancel-'+chr(key));release(key);time.sleep(.60)
            restored=snapshot(client,'obs-restored-'+chr(key))
            assert same(idle,restored),f'{chr(key)} cancellation did not return to exact idle'
        print('PASS: live OBS F5 starts/restarts; D/F/J/K cancel and return to exact idle.',flush=True)
        inputs=client.call('GetInputList')['inputs']
        for item in inputs:
            if item['inputName']==AUDIO:continue
            try:
                value=client.call('GetInputMute',{'inputName':item['inputName']})['inputMuted']
                muted.append((item['inputName'],value))
                client.call('SetInputMute',{'inputName':item['inputName'],'inputMuted':True})
            except Exception:pass
        temporary='MilkFrog Playback Verification'
        client.call('CreateScene',{'sceneName':temporary})
        for source in [NAME,AUDIO]:client.call('CreateSceneItem',{'sceneName':temporary,'sourceName':source,'sceneItemEnabled':True})
        client.call('SetCurrentProgramScene',{'sceneName':temporary})
        record_directory=client.call('GetRecordDirectory')['recordDirectory']
        client.call('SetRecordDirectory',{'recordDirectory':str(OUT)})
        client.call('StartRecord');recording=True;time.sleep(.3)
        tap(0x74);time.sleep(9.25)
        ended=snapshot(client,'obs-ended')
        assert same(idle,ended),'Video did not complete/restore automatically'
        result=client.call('StopRecord');recording=False
        output=Path(result['outputPath']);target=OUT/('obs-laugh-recording'+output.suffix)
        # StopRecord responds before the muxer releases its file on Windows.
        for _ in range(100):
            try:output.replace(target);break
            except PermissionError:time.sleep(.1)
        else:target=output
        report={'live_video':'PASS','restart':'PASS','DFJK_cancellation':'PASS','exact_idle_restoration':'PASS',
            'completion':'PASS','native_source_kind':client.call('GetInputSettings',{'inputName':NAME})['inputKind'],
            'audio_monitor':client.call('GetInputAudioMonitorType',{'inputName':AUDIO})['monitorType'],
            'recording':str(target)}
        (OUT/'obs-live.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False),flush=True)
    finally:
        release(0x74)
        for key in 'DFJK':release(ord(key))
        if client:
            if recording:
                try:client.call('StopRecord')
                except Exception:pass
            if scene:
                try:client.call('SetCurrentProgramScene',{'sceneName':scene})
                except Exception:pass
            if record_directory:
                try:client.call('SetRecordDirectory',{'recordDirectory':record_directory})
                except Exception:pass
            if temporary:
                try:client.call('RemoveScene',{'sceneName':temporary})
                except Exception:pass
            for name,value in muted:
                try:client.call('SetInputMute',{'inputName':name,'inputMuted':value})
                except Exception:pass
            client.close()
        if process and process.poll() is None:
            def close(hwnd,param):
                pid=ctypes.c_ulong();u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                if pid.value==process.pid:
                    title=ctypes.create_unicode_buffer(1024);u.GetWindowTextW(hwnd,title,1024)
                    if title.value.startswith('OBS '):u.PostMessageW(hwnd,0x0010,0,0)
                return True
            u.EnumWindows(callback(close),0)
            try:process.wait(timeout=30)
            except subprocess.TimeoutExpired:raise RuntimeError('OBS did not close normally; websocket configuration needs restoration')
        config.write_bytes(original)
        # Leave the user's normal native scene available, without the test API.
        subprocess.Popen([str(OBS),'--minimize-to-tray'],cwd=OBS.parent)
if __name__=='__main__':main()
