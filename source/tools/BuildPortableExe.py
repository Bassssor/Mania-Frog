"""Build a compact standalone EXE without reducing resolution, fps, or alpha quality."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def imported_dlls(path):
    """Read the normal PE import directory (both Win32 and Win64)."""
    data=path.read_bytes()
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[pe:pe+4]!=b'PE\0\0':raise ValueError('Invalid PE: '+path.name)
    sections=struct.unpack_from('<H',data,pe+6)[0]
    optional_size=struct.unpack_from('<H',data,pe+20)[0]
    optional=pe+24
    directory=optional+(96 if struct.unpack_from('<H',data,optional)[0]==0x10b else 112)
    tables=optional+optional_size
    def offset(rva):
        for i in range(sections):
            virtual_size,virtual_address,raw_size,raw=struct.unpack_from('<4I',data,tables+i*40+8)
            if virtual_address<=rva<virtual_address+max(virtual_size,raw_size):return raw+rva-virtual_address
        raise ValueError('Invalid PE RVA: '+path.name)
    rva=struct.unpack_from('<I',data,directory+8)[0]
    if not rva:return set()
    pos=offset(rva);result=set()
    while any(data[pos:pos+20]):
        name=offset(struct.unpack_from('<I',data,pos+12)[0]);end=data.index(0,name)
        result.add(data[name:end].decode('ascii').lower());pos+=20
    # Delay imports can be resolved later in the program; keep those too.
    delay=struct.unpack_from('<I',data,directory+13*8)[0]
    if delay:
        pos=offset(delay)
        while any(data[pos:pos+32]):
            if struct.unpack_from('<I',data,pos)[0]&1:
                name=offset(struct.unpack_from('<I',data,pos+4)[0]);result.add(data[name:data.index(0,name)].decode('ascii').lower())
            else:raise ValueError('Unsupported VA-based delay import: '+path.name)
            pos+=32
    return result


def compress_losslessly(data):
    lib=ctypes.WinDLL('cabinet',use_last_error=True)
    lib.CreateCompressor.argtypes=[ctypes.c_uint,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    lib.Compress.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    lib.CloseCompressor.argtypes=[ctypes.c_void_p]
    handle=ctypes.c_void_p()
    if not lib.CreateCompressor(5,None,ctypes.byref(handle)):raise ctypes.WinError(ctypes.get_last_error())
    try:
        size=ctypes.c_size_t()
        lib.Compress(handle,data,len(data),None,0,ctypes.byref(size))
        if not size.value:raise ctypes.WinError(ctypes.get_last_error())
        output=ctypes.create_string_buffer(size.value)
        if not lib.Compress(handle,data,len(data),output,len(output),ctypes.byref(size)):raise ctypes.WinError(ctypes.get_last_error())
        return output.raw[:size.value]
    finally:lib.CloseCompressor(handle)


def main():
    parser=argparse.ArgumentParser()
    default=ROOT.parent/'MilkFrog.exe' if ROOT.name=='source' else ROOT/'dist/MilkFrog.exe'
    parser.add_argument('--output', type=Path, default=default)
    parser.add_argument('--frame-codec',choices=['webp','jxr'],default='webp',help='Container for the laughter frame pack.')
    parser.add_argument('--image-quality',type=float,default=.95,help='Colour quality of the frame codec; 1 keeps the original lossless PNG frames. Alpha is always lossless.')
    args=parser.parse_args()
    if not .90<=args.image_quality<=1:parser.error('Image quality must be between 0.90 and 1.0')
    folder=ROOT/'dist/milk-frog-4k'
    build=ROOT/'build/portable';build.mkdir(parents=True,exist_ok=True)
    payload=build/'payload';payload.mkdir(exist_ok=True)
    vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    installation=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    if not installation:raise RuntimeError('Visual Studio C++ Build Tools are required')
    names=['MilkFrog.exe','config.txt','license.md','base.png','body-pressed.png','exit-pose.png',
        'tabletop.png','keyboard-corner.png','hands-runtime.png','hands-runtime.txt','keyboard-4k.png','keyboard-6k.png','keyboard-8k.png','laugh-frames.mfa','laugh.wav']
    available={p.name.lower():p.name for p in folder.glob('*.dll')}
    pending=[folder/'MilkFrog.exe'];required=set()
    while pending:
        for name in imported_dlls(pending.pop()):
            if name in available and name not in required:
                required.add(name);pending.append(folder/available[name])
    names+=sorted(available[name] for name in required)
    original_frame_pack=folder/'laugh-frames.mfa'
    optimized_pack=original_frame_pack
    optimization=None
    if args.image_quality<1:
        source_hash=hashlib.sha256(original_frame_pack.read_bytes()).hexdigest()
        optimized_pack=build/('laugh-'+args.frame_codec+'-'+str(args.image_quality)+'.mfa')
        metadata=optimized_pack.with_suffix('.json')
        encoder_hash=hashlib.sha256((ROOT/'tools/PrepareLaughPlayback.py').read_bytes()).hexdigest()
        expected={'source_sha256':source_hash,'codec':args.frame_codec,'quality':args.image_quality,'encoder_sha256':encoder_hash}
        cached=json.loads(metadata.read_text()) if metadata.exists() else {}
        if not optimized_pack.exists() or any(cached.get(k)!=v for k,v in expected.items()):
            temporary=optimized_pack.with_suffix('.tmp')
            subprocess.run([sys.executable,str(ROOT/'tools/PrepareLaughPlayback.py'),'encode-pack',
                '--input',str(original_frame_pack),'--output',str(temporary),
                '--codec',args.frame_codec,'--quality',str(args.image_quality),
                '--report',str(metadata)],check=True)
            optimization=json.loads(metadata.read_text())
            if optimization['max_alpha_error']!=0:raise RuntimeError('Alpha changed during optimization')
            temporary.replace(optimized_pack)
            metadata.write_text(json.dumps({**expected,'verification':optimization},indent=2))
        else:optimization=cached['verification']
    resources=['#include <windows.h>','101 ICON "SFML/SFML/milk-frog.ico"']
    rows=[];details=[];digest=hashlib.sha256()
    for index,name in enumerate(names,1000):
        original=(optimized_pack if name=='laugh-frames.mfa' else folder/name).read_bytes()
        digest.update(name.encode());digest.update(original)
        # PNG/JPEG XR is already compressed; avoid spending startup time on
        # an ineffective second pass. Executables, DLLs and PCM compress well.
        compressed=compress_losslessly(original) if name.lower().endswith(('.dll','.exe','.wav')) else original
        method=5 if len(compressed)<len(original)*.95 else 0
        packed=compressed if method else original
        asset=payload/(str(index)+'.bin');asset.write_bytes(packed)
        resources.append(f'{index} RCDATA "build/portable/payload/{index}.bin"')
        rows.append(f'    {{{index},L"{name}",{str(name=="config.txt").lower()},{method},{len(original)}}},')
        details.append({'name':name,'runtimeBytes':len(original),'embeddedBytes':len(packed),'compression':method})
    version=digest.hexdigest()[:24]
    manifest=['#pragma once','struct PayloadFile { unsigned resource; const wchar_t* name; bool editable; unsigned compression; unsigned unpackedSize; };',
              f'constexpr const wchar_t* PayloadVersion=L"{version}";', 'constexpr PayloadFile PayloadFiles[] = {',*rows,'};']
    (build/'payload.rc').write_text('\n'.join(resources),encoding='ascii')
    (build/'payload_manifest.h').write_text('\n'.join(manifest),encoding='ascii')
    script=f'''@echo off
call "{installation}\\Common7\\Tools\\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
rc /nologo /fobuild\\portable\\payload.res build\\portable\\payload.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /Ibuild\\portable SFML\\SFML\\PortableLauncher.cpp build\\portable\\payload.res /Fobuild\\portable\\PortableLauncher.obj /Febuild\\portable\\MilkFrogPortable.exe /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib ole32.lib cabinet.lib
exit /b %errorlevel%
'''
    (build/'build.cmd').write_text(script,encoding='utf-8')
    subprocess.run([os.environ['ComSpec'],'/d','/c',r'build\portable\build.cmd'],cwd=ROOT,check=True)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((build/'MilkFrogPortable.exe').read_bytes())
    report={'payloadVersion':version,'files':names,'fileCount':len(names),'resources':details,
            'imageOptimization':optimization,'removedUnusedDlls':sorted(set(available)-required),
            'outputBytes':args.output.stat().st_size,'cache':str(Path(os.environ['LOCALAPPDATA'])/'MilkFrog/Runtime'/version)}
    (build/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))


if __name__=='__main__':main()
