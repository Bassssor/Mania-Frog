"""Read-only checks of the regenerated, torso-anchored pressed body."""
from pathlib import Path
from PIL import Image,ImageChops
import json
import sys

sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[1]
directory=root/'dist/milk-frog-4k/verification'
idle=Image.open(root/'SFML/SFML/base.png').convert('RGBA')
pressed=Image.open(root/'SFML/SFML/body-pressed.png').convert('RGBA')
assert idle.size==pressed.size==(1189,669)
assert ImageChops.difference(idle.crop((0,291,1189,669)),pressed.crop((0,291,1189,669))).getbbox() is None, 'Original torso must remain exact below the neck blend'
original_scene=Image.open(root/'assets/milk-frog/desktop-preview.png').convert('RGBA')
scene_idle=Image.open(directory/'state-0.png').convert('RGBA')
assert ImageChops.difference(original_scene,scene_idle).getbbox() is None,'Idle art changed'
reference=Image.open(directory/'state-1.png').convert('RGBA')
head=(350,25,650,230)
for mask in range(1,16):
    scene=Image.open(directory/f'state-{mask}.png').convert('RGBA')
    assert ImageChops.difference(scene.crop(head),reference.crop(head)).getbbox() is None,'A held-key combination has a different expression'
def bounds(image):
    alpha=image.crop((350,0,650,200)).getchannel('A').point(lambda a:255 if a>=128 else 0)
    return alpha.getbbox()
before,after=bounds(scene_idle),bounds(reference)
assert abs(before[1]-after[1])<=8,'Head moved too much vertically'
assert abs((before[0]+before[2])-(after[0]+after[2]))<=16,'Head moved too far horizontally'
mouth=reference.crop((400,135,525,230))
dark=sum(a>200 and max(r,g,b)<110 for r,g,b,a in mouth.getdata())
assert dark>2500,'Laughing mouth is not wide open'
green=sum(a>200 and g>35 and g-r>10 and g>r*1.15 and g>b*1.1 for r,g,b,a in reference.crop((390,85,540,155)).getdata())
assert green==0,'Old green eyes remain visible during the closed-eye laugh'
report={'regeneratedHeadAndNeck':True,'idleUnchanged':True,'torsoUnchangedBelowWorldY':291,
    'heldCombinations':15,'headTopDisplacement':after[1]-before[1],
    'headCenterXDisplacement':((after[0]+after[2])-(before[0]+before[2]))/2,
    'openMouthDarkPixels':dark,'residualGreenEyePixels':green}
(directory/'laugh-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
