"""Build a single directly runnable EXE from the verified native release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    default=ROOT.parent/'MilkFrog.exe' if ROOT.name=='source' else ROOT/'dist/MilkFrog.exe'
    parser.add_argument('--output', type=Path, default=default)
    args=parser.parse_args()
    folder=ROOT/'dist/milk-frog-4k'
    names=['MilkFrog.exe','config.txt','license.md','base.png','base_0001.png',
        'base_0010.png','base_0011.png','base_0100.png','base_1000.png',
        'base_1100.png','base_left.png','base_right.png','body-pressed.png',
        'tabletop.png','keyboard-corner.png','laugh-frames.mfa','laugh.wav']
    names+=sorted(p.name for p in folder.glob('*.dll'))
    digest=hashlib.sha256()
    for name in names:
        digest.update(name.encode());digest.update((folder/name).read_bytes())
    version=digest.hexdigest()[:24]
    build=ROOT/'build/portable'
    build.mkdir(parents=True,exist_ok=True)
    resources=['#include <windows.h>','101 ICON "SFML/SFML/milk-frog.ico"']
    manifest=['#pragma once','struct PayloadFile { unsigned resource; const wchar_t* name; bool editable; };',
              f'constexpr const wchar_t* PayloadVersion=L"{version}";', 'constexpr PayloadFile PayloadFiles[] = {']
    for index,name in enumerate(names,1000):
        resources.append(f'{index} RCDATA "dist/milk-frog-4k/{name}"')
        manifest.append(f'    {{{index},L"{name}",{str(name=="config.txt").lower()}}},')
    manifest.append('};')
    (build/'payload.rc').write_text('\n'.join(resources),encoding='ascii')
    (build/'payload_manifest.h').write_text('\n'.join(manifest),encoding='ascii')
    vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    installation=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    if not installation:raise RuntimeError('Visual Studio C++ Build Tools are required')
    script=f'''@echo off
call "{installation}\\Common7\\Tools\\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
rc /nologo /fobuild\\portable\\payload.res build\\portable\\payload.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /Ibuild\\portable SFML\\SFML\\PortableLauncher.cpp build\\portable\\payload.res /Fobuild\\portable\\PortableLauncher.obj /Febuild\\portable\\MilkFrogPortable.exe /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ole32.lib
exit /b %errorlevel%
'''
    (build/'build.cmd').write_text(script,encoding='utf-8')
    subprocess.run([os.environ['ComSpec'],'/d','/c',r'build\portable\build.cmd'],cwd=ROOT,check=True)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((build/'MilkFrogPortable.exe').read_bytes())
    report={'payloadVersion':version,'files':names,'fileCount':len(names),
            'outputBytes':args.output.stat().st_size,'cache':str(Path(os.environ['LOCALAPPDATA'])/'MilkFrog/Runtime'/version)}
    (build/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))


if __name__=='__main__':main()
