"""Exercise the real native OBS source and global key hook, without browser code."""
import base64
import ctypes
import io
import json
from pathlib import Path
import time
from PIL import Image, ImageChops
from ObsControl import Obs

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/obs-native/verification'
NAME = '奶蛙 · DFJK 输入叠加'
FILTER = '奶蛙 DFJK · 原生动作与粒子'
KEYS = [68, 70, 74, 75]
PRESET = ROOT.parent / 'obs-input-overlay' if ROOT.name == 'source' else ROOT / 'dist/input-overlay'


def screenshot(client, size=880):
    data = client.call('GetSourceScreenshot', {'sourceName': NAME, 'imageFormat': 'png', 'imageWidth': size, 'imageHeight': size})
    return Image.open(io.BytesIO(base64.b64decode(data['imageData'].split(',', 1)[1]))).convert('RGBA')


def compare(actual, expected):
    alpha = ImageChops.difference(actual.getchannel('A'), expected.getchannel('A')).getextrema()[1]
    opaque_error = max((max(abs(a[k] - e[k]) for k in range(3))
        for a, e in zip(actual.getdata(), expected.getdata()) if e[3] == 255), default=0)
    if alpha > 2 or opaque_error > 4:
        raise AssertionError(f'Native image mismatch: alpha={alpha}, RGB={opaque_error}')
    return {'alphaError': alpha, 'opaqueRgbError': opaque_error}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    user = ctypes.windll.user32
    user.GetForegroundWindow.restype = ctypes.c_void_p
    handles = []
    def find(hwnd, ignored):
        title = ctypes.create_unicode_buffer(512)
        user.GetWindowTextW(hwnd, title, 512)
        if title.value.startswith('OBS '): handles.append(hwnd)
        return True
    callback = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)(find)
    user.EnumWindows(callback, 0)
    if not handles or user.GetForegroundWindow() != handles[0]:
        raise RuntimeError('OBS must have focus before test input; no input was sent')
    client = Obs()
    def particles(enabled):
        client.call('SetSourceFilterSettings', {'sourceName': NAME, 'filterName': FILTER,
                    'filterSettings': {'particles': enabled}, 'overlay': True})
    def release():
        for key in KEYS: user.keybd_event(key, 0, 2, 0)
    try:
        if client.call('GetInputSettings', {'inputName': NAME})['inputKind'] != 'input-overlay':
            raise AssertionError('Source is still a browser or another input kind')
        release(); particles(False); time.sleep(.1)
        report = {'nativeKind': 'input-overlay', 'combinations': [], 'scales': []}
        before = client.call('GetStats')
        for mask in range(16):
            release()
            for i, key in enumerate(KEYS):
                if mask & (1 << i): user.keybd_event(key, 0, 0, 0)
            time.sleep(.07)
            actual = screenshot(client)
            expected = Image.open(ROOT / f'dist/milk-frog-4k/verification/state-{mask}.png').convert('RGBA')
            actual.save(OUT / f'state-{mask}.png')
            report['combinations'].append({'mask': mask, **compare(actual, expected)})
            if mask in [0, 2, 4, 15]:
                for size in [220, 320, 440, 600, 800, 1200]:
                    scaled = screenshot(client, size)
                    ref = expected.resize((size, size), Image.Resampling.BILINEAR)
                    holes = sum(e[3] == 255 and a[3] < 252 for a, e in zip(scaled.getdata(), ref.getdata()))
                    if holes: raise AssertionError(f'Interior seam at mask={mask}, size={size}: {holes}')
                    report['scales'].append({'mask': mask, 'size': size, 'interiorHoles': holes})
        release(); time.sleep(.07)
        idle = Image.open(ROOT / 'dist/milk-frog-4k/verification/state-0.png').convert('RGBA')
        compare(screenshot(client), idle)
        # A missing config must safely show the official source's idle PNG;
        # restoring the config while rendering must reload without a deadlock.
        client.call('SetSourceFilterSettings', {'sourceName': NAME, 'filterName': FILTER,
            'filterSettings': {'config_file': str(ROOT / 'build/obs-native/nonexistent.json')}, 'overlay': True})
        compare(screenshot(client), idle)
        client.call('SetSourceFilterSettings', {'sourceName': NAME, 'filterName': FILTER,
            'filterSettings': {'config_file': str(PRESET / 'milk-frog.json')}, 'overlay': True})
        compare(screenshot(client), idle)
        report['configurationRecovery'] = 'missing config falls back to idle, live reload succeeds'
        # Unrelated keyboard input must not alter the pose or expression.
        user.keybd_event(65, 0, 0, 0); time.sleep(.05)
        compare(screenshot(client), idle); user.keybd_event(65, 0, 2, 0)
        particles(True)
        for key in KEYS: user.keybd_event(key, 0, 0, 0)
        time.sleep(.20)
        held = screenshot(client); held.save(OUT / 'fountain-held.png')
        expected = Image.open(ROOT / 'dist/milk-frog-4k/verification/state-15.png').convert('RGBA')
        changed = sum(max(d)>12 for d in ImageChops.difference(held, expected).getdata())
        if changed < 500: raise AssertionError('Native particle fountain missing')
        release(); time.sleep(.07)
        just_released = screenshot(client)
        compare(just_released.crop((390,40,620,230)), idle.crop((390,40,620,230)))
        remaining = sum(max(d)>12 for d in ImageChops.difference(just_released, idle).getdata())
        if remaining < 100: raise AssertionError('Fountain release animation vanished immediately')
        time.sleep(.45)
        compare(screenshot(client), idle)
        report['particles'] = {'changedPixels': changed, 'releaseTailPixels': remaining, 'clearedWithinSeconds': .52}
        report['releaseExpression'] = 'original idle face before particles finish fading'
        report['statsBefore'] = before; report['statsAfter'] = client.call('GetStats')
        report['streaming'] = client.call('GetStreamStatus')['outputActive']
        report['recording'] = client.call('GetRecordStatus')['outputActive']
        (OUT / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'passed': True, 'nativeKind': 'input-overlay', 'combinations': 16,
                          'scaleCases': len(report['scales']), 'particles': report['particles'],
                          'fps': report['statsAfter']['activeFps'],
                          'obsRenderMs': report['statsAfter']['averageFrameRenderTime']}, ensure_ascii=False))
    finally:
        release(); user.keybd_event(65, 0, 2, 0); particles(True); client.close()


if __name__ == '__main__': main()
