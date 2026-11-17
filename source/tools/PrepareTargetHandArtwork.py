"""Extract the target's painted arms and pose their existing short fingertips."""
from pathlib import Path
import json
import numpy as np
from PIL import Image
import aggdraw
from scipy.ndimage import gaussian_filter, map_coordinates, distance_transform_edt, binary_fill_holes, binary_closing, binary_erosion
from ImportGeneratedLaneArtwork import transform
from TargetFingerPoses import draw_outline, FingerPoses

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'assets/milk-frog/regenerated-hands/target-v11'
OUT = ART / 'runtime-candidate'
W, H = 1189, 669
SCALE, OFFSET = .7, 128
# Left mask bits: ring, middle, index, pinky. Right: index, middle, ring, pinky.
FINGERS = [[(790,824),(740,817),(690,805),(840,835)],
           [(515,779),(465,773),(420,765),(370,755)]]


def extract(name, side, idle=False):
    source = Image.open(ART / name).convert('RGBA')
    a = np.asarray(source).astype(float)
    r, g, b = [a[:, :, i] for i in range(3)]
    skin = (r-b > 65) | ((r < 155) & (b < 100) & (g-b > 8))
    if not idle:
        yy = np.arange(source.height)[:,None]
        skin &= (yy >= (650 if side == 0 else 600)) | (g-b > 70)
    mask = Image.new('L', source.size)
    path = aggdraw.Path()
    if idle and side == 0:
        path.moveto(730,420)
        for c in [(760,389,800,379,838,390),(918,403,992,445,1000,500),
                  (1014,550,970,584,927,571),(875,561,843,525,818,491),
                  (808,503,795,504,789,489),(777,493,767,490,765,477),
                  (751,483,743,474,741,463),(730,465,719,452,726,434),
                  (728,430,729,425,730,420)]:path.curveto(*c)
    elif idle:
        path.moveto(461,390)
        # The lower inner edge meets the body's left silhouette at (458, 380).
        for c in [(501,365,547,363,574,389),(590,405,601,424,598,440),
                  (596,451,582,458,573,451),(571,466,553,474,542,465),
                  (541,479,525,484,517,475),(514,489,493,495,485,481),
                  (476,500,460,522,451,543),(420,541,400,520,398,487),
                  (396,442,420,413,461,390)]:path.curveto(*c)
    if idle:
        path.close()
        painter=aggdraw.Draw(mask);painter.path(path,aggdraw.Brush(255));painter.flush()
    else:
        mask=draw_outline(source.size,side)
    solid = binary_fill_holes(binary_closing(skin & (np.asarray(mask)>128),iterations=1))
    if not idle:
        # The finger outline also excludes the brown keycap bevels in the
        # source. Colour segmentation cannot distinguish those from skin.
        fingertips = np.arange(source.height)[:,None] >= (730 if side==0 else 680)
        solid = np.where(fingertips,np.asarray(mask)>128,solid)
        depth = distance_transform_edt(solid)
        clean_core = solid & (depth>6) & skin
        _,nearest = distance_transform_edt(~clean_core,return_indices=True)
        weight = np.clip((6-depth)/4,0,1)*fingertips
        sample = a[nearest[0],nearest[1],:3]
        a[:,:,:3] = a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
    if idle:
        # The traced silhouette includes the complete fingertips and their valleys.
        # A colour threshold would cut holes in the dark crease pixels.
        solid = np.asarray(mask)>128
        yy,xx = np.mgrid[:source.height,:source.width]
        depth = distance_transform_edt(solid)
        dark_core = solid & (depth>4) & (a[:,:,0]<145)
        distance,nearest = distance_transform_edt(~dark_core,return_indices=True)
        tip_band = (yy>435)&(xx<830) if side==0 else (yy>435)&(xx>490)
        weight = np.clip((4-depth)/2,0,1)*np.clip((8-distance)/3,0,1)*tip_band
        sample = a[nearest[0],nearest[1],:3]
        a[:,:,:3] = a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
        if side == 0:
            _,nearest = distance_transform_edt(depth<12,return_indices=True)
            weight = np.clip((12-depth)/8,0,1)*np.clip((xx-835)/25,0,1)*np.clip((yy-445)/25,0,1)
            sample = a[nearest[0],nearest[1],:3]
            a[:,:,:3] = a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
        else:
            # The far elbow has a baked white backdrop inside its opaque
            # outline. Continue the adjacent yellow material across that rim.
            yellow_core = solid & (depth>8) & (a[:,:,1]-a[:,:,2]>70)
            _,nearest = distance_transform_edt(~yellow_core,return_indices=True)
            weight = np.clip((9-depth)/5,0,1)*np.clip((yy-450)/20,0,1)*np.clip((485-xx)/15,0,1)
            sample = a[nearest[0],nearest[1],:3]
            a[:,:,:3] = a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
    if not idle and side == 1:
        yy,xx = np.mgrid[:source.height,:source.width]
        patch=(np.asarray(mask)>128)&(xx<395)&(yy>637)&(yy<698)
        weight=np.clip((395-xx)/24,0,1)*np.clip((yy-637)/12,0,1)*np.clip((698-yy)/12,0,1)*patch
        sample=a[yy,np.maximum(xx,395),:3]
        a[:,:,:3]=a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
        solid|=patch
    if not idle and side == 0:
        yy,xx = np.mgrid[:source.height,:source.width]
        # Continue the forearm's clean paint across the original elbow bevel.
        weight=np.exp(-((xx-880)/30)**4-((yy-559)/35)**4)
        sample=a[yy,np.minimum(np.maximum(xx,918),source.width-1),:3]
        a[:,:,:3]=a[:,:,:3]*(1-weight[:,:,None])+sample*weight[:,:,None]
    if idle:
        alpha = gaussian_filter(np.asarray(mask)/255, .35)
    else:
        alpha = gaussian_filter(solid.astype(float), .6)
        alpha = np.where(fingertips,gaussian_filter(np.asarray(mask)/255,.35),alpha)
    # Keep antialias pixels coloured by the arm, not the pale image backdrop.
    distance, nearest = distance_transform_edt(alpha < .97, return_indices=True)
    edge = (alpha > 0) & (alpha < .97) & (distance < 4)
    a[:, :, :3][edge] = a[nearest[0][edge], nearest[1][edge], :3]
    a[:, :, 3] = alpha*255
    return Image.fromarray(np.uint8(a))


def register(image, body, side, idle):
    matrix = np.array([[SCALE,0,OFFSET+(14 if idle and side==1 else 0)],[0,SCALE,0],[0,0,1]])
    result = transform(image, matrix, (W,H))
    if idle and side == 1:
        # The far hand is smaller; taper the correction out before the belly join.
        pixels = np.asarray(result.convert('RGBa'))
        y, x = np.mgrid[:H,:W]
        blend = np.clip((y-335)/45,0,1)
        scale = .88 + .12*blend*blend*(3-2*blend)
        coordinates = [305+(y-305)/scale,514+(x-514)/scale]
        pixels = np.stack([map_coordinates(pixels[:,:,c],coordinates,order=1,
                                          mode='constant') for c in range(4)],axis=-1)
        result = Image.fromarray(pixels,'RGBa').convert('RGBA')
    a = np.asarray(result).astype(float)
    b = np.asarray(body).astype(float)
    yy = np.arange(H)[:,None]
    progress = np.clip((yy-240)/120,0,1)
    progress = progress*progress*(3-2*progress)
    yellow = np.clip((a[:,:,1]-a[:,:,2]-60)/45,0,1)
    body_core = (b[:,:,3]>250) & (b[:,:,1]-b[:,:,2]>70)
    inside = distance_transform_edt(body_core)
    color_core = binary_erosion(body_core,iterations=6)
    distance,nearest = distance_transform_edt(~color_core,return_indices=True)
    weight = (1-progress)*yellow*np.clip(1-distance/45,0,1)
    a[:,:,:3] = a[:,:,:3]*(1-weight[:,:,None])+b[nearest[0],nearest[1],:3]*weight[:,:,None]
    a[:,:,3] *= 1-(1-progress)*yellow*np.clip((inside-3)/12,0,1)
    return Image.fromarray(np.uint8(a))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    body = Image.open(ROOT/'SFML/SFML/base.png').convert('RGBA')
    frames = []
    for side in range(2):
        master = extract('08-all-held-v2.png', side)
        master.save(OUT/f'arm-source-{side}.png')
        poses = FingerPoses(master,side,FINGERS[side])
        idle = extract('00-raised.png', side, idle=True)
        for mask in range(16):
            frame = register(idle if mask == 0 else poses.pose(mask), body, side, mask==0)
            frame.save(OUT/f'hand-{side}-{mask}.png')
            frames.append(frame)
    for mode, indices in [(8,list(range(32))),
                          (6,list(range(8))+list(range(16,24))),
                          (4,[0,2,4,6,16,17,18,19])]:
        atlas = Image.new('RGBA',(W*4,H*(len(indices)//4)))
        for i,index in enumerate(indices):
            atlas.paste(frames[index],(i%4*W,i//4*H))
        atlas.save(OUT/f'hands-{mode}k.png')
    contacts = [[(round(x*SCALE+OFFSET),round(y*SCALE)) for x,y in hand]
                for hand in FINGERS]
    (OUT/'contacts.json').write_text(json.dumps(contacts,indent=2)+'\n')
    table = Image.open(ROOT/'SFML/SFML/tabletop.png').convert('RGBA').crop((0,0,W,H))
    for name,lm,rm in [('raised',0,0),('4k',6,3),('6k',7,7),('8k',15,15)]:
        scene=(body if name=='raised' else Image.open(ROOT/'SFML/SFML/body-pressed.png').convert('RGBA')).copy();scene.alpha_composite(table)
        scene.alpha_composite(frames[lm]);scene.alpha_composite(frames[16+rm])
        bg=Image.new('RGBA',(W,H),'#ece9e3');bg.alpha_composite(scene)
        bg.convert('RGB').save(OUT/f'scene-{name}.jpg',quality=95)
    print('Staged image-derived arms, 32 poses and contact coordinates:',OUT)


if __name__ == '__main__':
    main()
