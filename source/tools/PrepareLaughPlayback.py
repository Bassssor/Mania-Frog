"""Prepare the approved transparent clip for native WIC playback, losslessly."""
from pathlib import Path
import argparse, os, io, json, struct, subprocess, shutil, hashlib
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--ffmpeg',default=os.environ.get('FFMPEG', 'ffmpeg'))
    args=parser.parse_args()
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
        process=subprocess.Popen([args.ffmpeg,'-v','error','-c:v','libvpx-vp9','-i',str(video),'-an','-frames:v','216','-f','rawvideo','-pix_fmt','rgba','pipe:1'],stdout=subprocess.PIPE)
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
if __name__=='__main__':main()
