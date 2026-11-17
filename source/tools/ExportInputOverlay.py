"""Export compact per-hand OBS atlases without duplicating every full-scene state."""
import json
import os
import subprocess
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT.parent/'obs-input-overlay'
FRAMES=ROOT/'build/obs-key-setup/frames'
SIZE=880


def pack(part,count):
    images=[Image.open(FRAMES/'parts'/f'{part}-{i}.png').convert('RGBA') for i in range(count)]
    boxes=[im.getbbox() for im in images if im.getbbox()]
    box=(max(0,min(b[0] for b in boxes)-2),max(0,min(b[1] for b in boxes)-2),
         min(SIZE,max(b[2] for b in boxes)+2),min(SIZE,max(b[3] for b in boxes)+2))
    x,y,r,b=box;w,h=r-x,b-y;columns=min(4,count);rows=(count+columns-1)//columns
    atlas=Image.new('RGBA',((w+3)*columns+2,(h+3)*rows+2))
    for i,im in enumerate(images):atlas.paste(im.crop(box),(1+i%columns*(w+3),1+i//columns*(h+3)))
    path=OUT/f'{part}-parts.png';atlas.save(path)
    with Image.open(path) as check:
        for i,im in enumerate(images):
            px,py=1+i%columns*(w+3),1+i//columns*(h+3)
            assert check.crop((px,py,px+w,py+h)).tobytes()==im.crop(box).tobytes()
    return dict(file=path.name,x=x,y=y,width=w,height=h,columns=columns,count=count)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    renderer=ROOT/'dist/milk-frog-4k/MilkFrog.exe'
    def export(flag,directory,count):
        subprocess.run([str(renderer),flag,os.path.relpath(directory,renderer.parent),'--mode',str(count)],cwd=renderer.parent,check=True)
    export('--export-obs-parts',FRAMES/'parts',4)
    parts={name:pack(name,count) for name,count in [('body',2),('desk',3),('hand',32),('shadow',32)]}
    for count in range(4,9):
        frames=FRAMES/str(count)
        export('--export-obs-assets',frames,count)
        idle=Image.new('RGBA',(882,882));idle.paste(Image.open(frames/'state-0.png').convert('RGBA'),(1,1))
        idle.save(OUT/f'milk-frog-{count}k-unlabeled.png')
        if count==4:
            idle.save(OUT/'milk-frog-unlabeled.png')
            # Retain the public 4K atlas aliases for existing integrations.
            for layer in ('background','foreground'):
                atlas=Image.new('RGBA',(3534,3534))
                for mask in range(16):atlas.paste(Image.open(frames/f'{layer}-{mask}.png'),(1+mask%4*883,1+mask//4*883))
                atlas.save(OUT/f'{layer}-atlas.png')
    config={'overlay_width':SIZE,'overlay_height':SIZE,'flags':0,
            'elements':[{'id':'milk-frog-idle','type':0,'z_level':0,'pos':[0,0],'mapping':[1,1,SIZE,SIZE]}],
            'milk_frog':{'version':2,'tile_size':SIZE,'stride':883,'parts':parts,
                         'supported_lanes':[4,5,6,7,8],'dynamic_key_labels':True,'keybindings_file':'keybindings.json'}}
    (OUT/'milk-frog.json').write_text(json.dumps(config,indent=2),encoding='utf-8')
    bindings=OUT/'keybindings.json'
    keys=json.loads(bindings.read_text(encoding='utf-8')).get('keys',[68,70,74,75]) if bindings.exists() else [68,70,74,75]
    subprocess.run([str(OUT/'MilkFrogKeySetup.exe'),'--set',*map(str,keys)],check=True)
    print('PASS: 4K–8K, 496 combinations exported; 69 cropped part tiles preserved pixel-for-pixel; three shared keyboards.')


if __name__=='__main__':main()
