"""Verify silent installation and the real OBS source without opening any UI."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--obs-root', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    args = parser.parse_args()
    build = ROOT / 'build/obs-easy'
    out = build / 'verification'
    out.mkdir(parents=True, exist_ok=True)
    profile = out / ('profile-' + uuid.uuid4().hex[:8])
    app_data = profile / 'AppData'
    program_data = profile / 'ProgramData'
    config = app_data / 'obs-studio'
    plugins = program_data / 'obs-studio/plugins'
    scenes = config / 'basic/scenes'
    scenes.mkdir(parents=True)
    fixture = {
        'name': 'Verification',
        'sources': [
            {'name': 'Target', 'id': 'scene', 'settings': {'items': [
                {'name': 'Camera'}, {'name': 'Existing frog'}]}},
            {'name': 'Untouched', 'id': 'scene', 'settings': {'items': []}},
            {'name': 'Camera', 'id': 'dshow_input'},
            {'name': 'Existing frog', 'id': 'milk_frog_source'},
        ]}
    scene_file = scenes / 'fixture.json'
    scene_file.write_text(json.dumps(fixture), encoding='utf-8')
    before = scene_file.read_bytes()
    legacy = config / 'plugins/milk-frog-easy/data'
    legacy.mkdir(parents=True)
    custom_keys = b'{"version":1,"keys":[83,68,70,74,75],"revision":123}'
    (legacy / 'keybindings.json').write_bytes(custom_keys)
    # Mock process discovery and isolate both Windows data roots.
    wrapper = out / 'silent-test.ps1'
    wrapper.write_text(
        'param($Installer,$ArgumentsBase64)\n'
        'function Get-Process {param($Name,$ErrorAction)}\n'
        '$options=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($ArgumentsBase64)) | ConvertFrom-Json\n'
        '$parameters=@{};foreach($property in $options.PSObject.Properties){$parameters[$property.Name]=$property.Value}\n'
        '& $Installer @parameters\nexit $LASTEXITCODE\n',
        encoding='utf-8-sig')
    installer = ROOT.parent / 'obs-input-overlay/InstallObsEasy.ps1'

    def install(*flags, success=True):
        options = {'Quiet': True, 'Json': True}
        arguments = iter(flags)
        for flag in arguments:
            options[flag.lstrip('-')] = True if flag in ('-ListTargets', '-Status') else next(arguments)
        encoded = base64.b64encode(json.dumps(options).encode()).decode()
        command = ['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy',
                   'Bypass', '-File', str(wrapper), str(installer),
                   encoded]
        env = dict(os.environ, APPDATA=str(app_data), PROGRAMDATA=str(program_data))
        result = subprocess.run(command, capture_output=True, encoding='utf-8', errors='replace',
                                env=env, timeout=45)
        if (result.returncode == 0) != success:
            raise AssertionError(result.stdout + result.stderr)
        return json.loads(result.stdout.strip())

    targets = install('-ListTargets')
    assert len(targets['targets']) == 2
    assert install('-Scene', 'Target', success=False)['status'] == 'error'
    assert install('-Collection', 'Verification', '-Scene', 'Missing',
                   '-SourceName', 'Frog', success=False)['status'] == 'error'
    assert install('-Collection', 'Verification', '-Scene', 'Target',
                   '-SourceName', 'Camera', success=False)['status'] == 'error'
    assert install('-Collection', 'Verification', '-Scene', 'Target',
                   '-SourceName', 'Frog', '-Action', 'Update', success=False)['status'] == 'error'
    answer = install('-Collection', 'Verification', '-Scene', 'Target',
                     '-SourceName', 'Frog', '-WaitSeconds', '0')
    assert answer['status'] == 'pending'
    assert install('-Status', '-RequestId', answer['requestId'])['status'] == 'pending'
    assert Path(answer['configRoot']) == config
    portable = profile / 'Portable'
    portable_scenes = portable / 'config/obs-studio/basic/scenes'
    portable_scenes.mkdir(parents=True)
    (portable / 'portable_mode.txt').touch()
    shutil.copyfile(scene_file, portable_scenes / 'fixture.json')
    portable_install = install('-ObsRoot', str(portable), '-Collection', 'Verification',
                               '-Scene', 'Target', '-SourceName', 'Frog')
    assert Path(portable_install['pluginPath']) == portable / 'plugins/milk-frog-easy/milk-frog-easy.dll'
    assert Path(portable_install['pluginPath']).is_file()
    data = plugins / 'milk-frog-easy/data'
    bindings = data / 'keybindings.json'
    assert bindings.read_bytes() == custom_keys, 'The corrected install lost previous keys'
    preserved = custom_keys.replace(b'123', b'456')
    bindings.write_bytes(preserved)
    install('-Collection', 'Verification', '-Scene', 'Target',
            '-SourceName', 'Frog', '-WaitSeconds', '0')
    assert bindings.read_bytes() == preserved, 'Reinstallation overwrote custom keys'
    shutil.copyfile(installer.parent / 'keybindings.json', bindings)
    assert scene_file.read_bytes() == before, 'Installer edited an OBS scene file'
    for request in (data / 'requests').glob('*.json'):
        request.unlink()
    checks = ['silent target listing', 'explicit target required', 'unknown scene rejected',
              'foreign source name rejected', 'missing update rejected', 'pending result truthful',
              'status query', 'portable plugin root', 'legacy keys migrated',
              'custom keys preserved', 'scene config untouched']
    vswhere = Path(os.environ['ProgramFiles(x86)']) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
                                  'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                                  '-property', 'installationPath'], text=True).strip()
    commands = [
        '@echo off', 'chcp 65001 >nul',
        f'call "{vs}/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul',
        f'cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /LD /DFROG_FRONTEND_STUB /I"{args.sdk_root}" /Ibuild/obs-easy tools/VerifyObsEasy.cpp /Fobuild/obs-easy/stub.obj /link build/obs-easy/obs.lib /OUT:build/obs-easy/verification/obs-frontend-api.dll /IMPLIB:build/obs-easy/verification/stub.lib',
        'if errorlevel 1 exit /b %errorlevel%',
        f'cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /I"{args.sdk_root}" /Ibuild/obs-easy tools/VerifyObsEasy.cpp /Fobuild/obs-easy/verify.obj /Febuild/obs-easy/verification/VerifyObsEasy.exe /link build/obs-easy/obs.lib user32.lib',
        'exit /b %errorlevel%',
    ]
    command_file = build / 'build-test.cmd'
    command_file.write_bytes(('\r\n'.join(commands) + '\r\n').encode('utf-8'))
    subprocess.run(['cmd.exe', '/d', '/c', str(command_file)], cwd=ROOT, check=True)
    env = dict(os.environ)
    env['PATH'] = str(args.obs_root / 'bin/64bit') + os.pathsep + env['PATH']
    command = [str(out / 'VerifyObsEasy.exe'), str(plugins), str(data), str(args.obs_root), str(out)]
    with (out / 'native.log').open('w', encoding='utf-8') as log:
        process = subprocess.run(command, cwd=args.obs_root / 'bin/64bit', env=env,
                                 stdout=log, stderr=subprocess.STDOUT, timeout=60)
    if process.returncode:
        raise AssertionError((out / 'native.log').read_text(encoding='utf-8', errors='replace')[-6000:])
    assert 'Double destroy' not in (out / 'native.log').read_text(encoding='utf-8', errors='replace')
    pixels = (out / 'idle.rgba').read_bytes()
    image = Image.frombytes('RGBA', (880, 880), pixels)
    assert image.getchannel('A').getextrema() == (0, 255), 'Transparent render is empty'
    assert sum(image.getchannel('A').histogram()[1:]) > 100000
    # Convert premultiplied OBS pixels for ordinary PNG viewers.
    straight = bytearray(pixels)
    for i in range(0, len(straight), 4):
        alpha = straight[i + 3]
        if alpha:
            for c in range(3):
                straight[i + c] = min(255, (straight[i + c] * 255 + alpha // 2) // alpha)
    rendered = Image.frombytes('RGBA', (880, 880), bytes(straight))
    rendered.save(out / 'idle.png')
    reference = Image.open(installer.parent / 'milk-frog-4k-unlabeled.png').convert('RGBA').crop((1, 1, 881, 881))
    reference = Image.alpha_composite(reference, Image.open(installer.parent / 'key-labels.png').convert('RGBA'))
    difference = ImageChops.difference(rendered, reference).getextrema()
    assert difference[3][1] == 0 and max(channel[1] for channel in difference[:3]) <= 2, difference
    checks.append('transparent native GPU render')
    checks.append('reference render: identical alpha and RGB error <= 2/255')
    report = {'passed': True, 'installerChecks': checks,
              'native': json.loads((out / 'native-report.json').read_text()),
              'pluginSHA256': hashlib.sha256((installer.parent / 'milk-frog-easy.dll').read_bytes()).hexdigest()}
    (out / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
