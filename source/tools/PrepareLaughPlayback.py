"""Prepare the approved transparent clip for native WIC playback, losslessly."""
from pathlib import Path
import argparse, os, io, json, struct, subprocess, shutil, hashlib
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
MAGIC=b'MFLAUGH1';SHAPE=(720,720,24,216);FRAMES=SHAPE[3]
CODECS={'webp':b'MFLAUGH2'}


def read_pack(path):
    """Return (data, spans) of an MFLAUGH1 PNG pack; blank entries become None."""
    data=path.read_bytes()
    if data[:8]!=MAGIC:raise RuntimeError('Not an MFLAUGH1 frame pack: '+str(path))
    shape=struct.unpack_from('<4I',data,8)
    if shape!=SHAPE:raise RuntimeError(f'Unexpected frame shape {shape}, expected {SHAPE}')
    spans=[]
    for index in range(FRAMES):
        offset,size=struct.unpack_from('<QI',data,24+index*12)
        if not size:spans.append(None);continue
        if offset+size>len(data):raise RuntimeError(f'Frame {index} runs past the end of the pack')
        spans.append((offset,size))
    return data,spans


def encode_pack(source,target,codec,quality,report_path=None):
    """Re-encode an MFLAUGH1 PNG pack into another WIC-decodable container.

    `quality` is a 0.90..1.0 fraction, matching the encoder CLI. Pillow takes a
    0..100 integer, so it has to be scaled; passing the fraction straight
    through silently pins every frame to quality 0.
    Alpha is never quantised: every frame is decoded again and compared against
    the source alpha plane, and the worst error is reported.
    """
    if codec not in CODECS:raise RuntimeError('Unsupported codec: '+codec)
    if not .90<=quality<=1:raise RuntimeError(f'Quality {quality} is outside 0.90..1.0')
    pillow_quality=max(1,min(100,round(quality*100)))
    data,spans=read_pack(source)
    encoded=[];worst_alpha=0
    for index,span in enumerate(spans):
        if span is None:encoded.append(None);continue
        offset,size=span
        frame=Image.open(io.BytesIO(data[offset:offset+size])).convert('RGBA')
        if frame.size!=(SHAPE[0],SHAPE[1]):raise RuntimeError(f'Frame {index} has size {frame.size}')
        buffer=io.BytesIO()
        frame.save(buffer,format=codec.upper(),lossless=False,quality=pillow_quality,method=6)
        payload=buffer.getvalue()
        check=Image.open(io.BytesIO(payload)).convert('RGBA')
        error=max(abs(a-b) for a,b in zip(frame.getchannel('A').tobytes(),check.getchannel('A').tobytes()))
        worst_alpha=max(worst_alpha,error)
        encoded.append(payload)
        if index%48==0:print(f'  frame {index}/{FRAMES} ({len(payload):,} B)',flush=True)
    # Hard guarantee rather than a reported observation: the shipped pack must
    # reproduce every source alpha byte exactly, or the build stops here. Real
    # webp quantises alpha unless this is proven on every frame.
    if worst_alpha!=0:
        raise RuntimeError(f'{codec} encoded alpha is not lossless (worst error {worst_alpha}/255); refusing to write the pack')
    target.parent.mkdir(parents=True,exist_ok=True)
    temporary=target.with_suffix('.tmp')
    with temporary.open('wb') as output:
        output.write(CODECS[codec]+struct.pack('<4I',*SHAPE))
        output.write(bytes(FRAMES*12))
        table=[]
        for payload in encoded:
            if payload is None:table.append((0,0));continue
            table.append((output.tell(),len(payload)));output.write(payload)
        output.seek(24)
        for offset,size in table:output.write(struct.pack('<QI',offset,size))
    written=24+FRAMES*12+sum(len(p) for p in encoded if p is not None)
    if temporary.stat().st_size!=written:
        raise RuntimeError(f'Frame pack assembly lost data ({temporary.stat().st_size} != {written})')
    temporary.replace(target)
    report={'codec':codec,'quality':quality,'source_bytes':len(data),'encoded_bytes':target.stat().st_size,
            'frames':FRAMES,'bytes_per_frame':round(target.stat().st_size/FRAMES,1),
            'max_alpha_error':worst_alpha,'lossless_alpha':worst_alpha==0,
            'source_sha256':hashlib.sha256(data).hexdigest()}
    if report_path:Path(report_path).write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


def build_assets(ffmpeg):
    media=ROOT.parent/'preview/seedance-video'
    assets=ROOT/'assets/milk-frog/laugh'
    assets.mkdir(parents=True,exist_ok=True)
    pack=assets/'laugh-frames.mfa'
    video=media/'milk-frog-laugh-final-transparent.webm'
    manifest=assets/'manifest.json'
    if video.exists():identity=hashlib.sha256(video.read_bytes()).hexdigest()
    elif pack.exists() and manifest.exists():identity=json.loads(manifest.read_text())['video_sha256']
    else:raise RuntimeError('Approved clip or frozen native frames are required')
    if not pack.exists() or not manifest.exists() or json.loads(manifest.read_text()).get('video_sha256')!=identity:
        process=subprocess.Popen([ffmpeg,'-v','error','-c:v','libvpx-vp9','-i',str(video),'-an','-frames:v','216','-f','rawvideo','-pix_fmt','rgba','pipe:1'],stdout=subprocess.PIPE)
        index=[]
        with pack.open('wb') as output:
            output.write(b'MFLAUGH1'+struct.pack('<4I',720,720,24,216))
            output.write(bytes(216*12))
            for frame in range(216):
                pixels=process.stdout.read(720*720*4)
                if len(pixels)!=720*720*4:raise RuntimeError(f'Frame {frame} incomplete')
                buffer=io.BytesIO()
                Image.frombytes('RGBA',(720,720),pixels).save(buffer,format='PNG',compress_level=3)
                encoded=buffer.getvalue();index.append((output.tell(),len(encoded)));output.write(encoded)
            output.seek(24)
            for offset,size in index:output.write(struct.pack('<QI',offset,size))
        if process.wait()!=0:raise RuntimeError('Transparent video decode failed')
        manifest.write_text(json.dumps({'video_sha256':identity,'frames':216,'fps':24,'seconds':9,'width':720,'height':720,'pack_bytes':pack.stat().st_size},indent=2))
    if (media/'laugh.wav').exists():shutil.copyfile(media/'laugh.wav',assets/'laugh.wav')
    for destination in [ROOT/'dist/milk-frog-4k',ROOT.parent/'obs-input-overlay']:
        destination.mkdir(parents=True,exist_ok=True)
        for name in ['laugh-frames.mfa','laugh.wav']:shutil.copyfile(assets/name,destination/name)
    print(manifest.read_text())


def main():
    parser=argparse.ArgumentParser()
    sub=parser.add_subparsers(dest='command')
    default=sub.add_parser('build',help='rebuild the PNG frame pack from the approved clip')
    default.add_argument('--ffmpeg',default=os.environ.get('FFMPEG','ffmpeg'))
    encode=sub.add_parser('encode-pack',help='re-encode an existing pack into another container')
    encode.add_argument('--input',type=Path,required=True)
    encode.add_argument('--output',type=Path,required=True)
    encode.add_argument('--codec',choices=sorted(CODECS),default='webp')
    encode.add_argument('--quality',type=float,default=.95)
    encode.add_argument('--report',type=Path)
    args=parser.parse_args()
    if args.command=='encode-pack':
        report=encode_pack(args.input,args.output,args.codec,args.quality,args.report)
        print(json.dumps(report,indent=2))
    else:
        build_assets(getattr(args,'ffmpeg',os.environ.get('FFMPEG','ffmpeg')))


if __name__=='__main__':main()
