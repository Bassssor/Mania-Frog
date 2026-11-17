"""Clean generated arm atlas alpha and register sprites on the native canvas."""
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from scipy.ndimage import binary_dilation, binary_erosion, distance_transform_edt, gaussian_filter, label, map_coordinates
from scipy.ndimage import gaussian_filter1d
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'assets/milk-frog/regenerated-hands'
W, H = 1189, 669
VIEWPORTS = [(500, 200, 900, 600), (280, 160, 680, 560)]

def clean(image):
    rgba = np.array(image.convert('RGBA'))
    r, g, b, alpha = [rgba[:, :, i].astype(float) for i in range(4)]
    # Reject the relay's saturated red/yellow matte fragments outside the art.
    noise = ((r > 150) & (g < r * .45)) | ((g > 180) & (b < 25) & (g > r * .94))
    alpha[noise] = 0
    components, _ = label(alpha > 128)
    sizes = np.bincount(components.ravel())
    sizes[0] = 0
    core = components == sizes.argmax()
    keep = binary_dilation(core, iterations=1)
    alpha[~keep] = 0
    alpha = np.clip((gaussian_filter(alpha / 255, .55) - .04) / .92, 0, 1)
    rgba[:, :, 3] = np.uint8(alpha * 255 + .5)
    rgba[rgba[:, :, 3] == 0] = 0
    return Image.fromarray(rgba)


def cells(mode):
    rows = 2 if mode == 4 else 4
    image = Image.open(ART / f'arms-{mode}k-short-round-v3.png').convert('RGBA')
    for i in range(rows * 4):
        x, y = i % 4, i // 4
        box = (round(x * image.width / 4), round(y * image.height / rows),
               round((x + 1) * image.width / 4), round((y + 1) * image.height / rows))
        yield clean(image.crop(box).resize((400, 400), Image.Resampling.LANCZOS))


def split(name, count):
    image = Image.open(ART / name).convert('RGBA')
    return [clean(image.crop((round(i * image.width / count), 0,
                              round((i + 1) * image.width / count), image.height)))
            .resize((400, 400), Image.Resampling.LANCZOS) for i in range(count)]


def yellow_bounds(image):
    a = np.array(image)
    r, g, b = [a[:, :, i].astype(float) for i in range(3)]
    mask = (a[:, :, 3] > 200) & (r > 180) & (g > 130) & (b < 150) & (r > g * 1.08)
    yy, xx = np.where(mask)
    return np.array([xx.min(), yy.min(), xx.max(), yy.max()], dtype=float)


def transform(image, forward, size):
    inverse = np.linalg.inv(forward)
    # Affine registration changes placement/scale only, never individual fingers.
    return image.convert('RGBa').transform(size, Image.Transform.AFFINE,
        tuple(inverse[:2].ravel()), Image.Resampling.BICUBIC).convert('RGBA')


def merge_shoulder(image, body, side):
    """Hide the isolated sprite's yellow cap where the original torso already exists.

    The generated arm is rendered over the body at runtime.  At the shoulder,
    the relay's cutout keeps a rounded terminal cap that becomes visible as a
    seam.  Removing only pixels whose yellow material matches the yellow torso
    lets the original body continue through the join while preserving the arm
    where it crosses the white belly or the transparent background.
    """
    arm = np.asarray(image.convert('RGBA')).astype(np.float32)
    receiver = np.asarray(body.convert('RGBA')).astype(np.float32)
    interior=binary_erosion(arm[:,:,3]>240,iterations=2)
    distance,nearest=distance_transform_edt(~interior,return_indices=True)
    edge=(arm[:,:,3]>0)&(distance<4)
    arm[:,:,:3][edge]=arm[nearest[0][edge],nearest[1][edge],:3]
    ar, ag, ab, aa = [arm[:, :, i] for i in range(4)]
    br, bg, bb, ba = [receiver[:, :, i] for i in range(4)]
    arm_yellow = np.clip((ag-ab-35)/40,0,1)
    body_yellow = np.clip((bg-bb-30)/40,0,1) * ba/255
    # Fade the cap into the existing torso over a broad upper-arm band.
    # The body's own alpha preserves the exterior silhouette of each arm.
    yy=np.arange(H)[:,None]
    start,end=(370,450) if side==0 else (300,385)
    progress=np.clip((yy-start)/(end-start),0,1)
    progress=progress*progress*(3-2*progress)
    body_core=(ba>250)&(bg-bb>70)
    distance,nearest=distance_transform_edt(~body_core,return_indices=True)
    color_weight=(1-progress)*np.clip(1-distance/20,0,1)*arm_yellow
    nearest_color=receiver[nearest[0],nearest[1],:3]
    arm[:,:,:3]=arm[:,:,:3]*(1-color_weight[:,:,None])+nearest_color*color_weight[:,:,None]
    # Cover the original body's antialiased edge with matching material so
    # that the former exterior contour cannot turn into an interior stripe.
    body_yellow=np.where(ba<250,0,body_yellow)
    arm[:, :, 3] *= 1-(1-progress)*body_yellow*arm_yellow
    arm[:, :, 3] = np.uint8(np.clip(arm[:, :, 3], 0, 255) + 0.5)
    arm[arm[:, :, 3] == 0] = 0
    return Image.fromarray(arm.astype(np.uint8), 'RGBA')


def arm_alignment(image, master):
    source, target = yellow_bounds(image), yellow_bounds(master)
    scale = (target[2:] - target[:2]) / (source[2:] - source[:2])
    offset = target[:2] - source[:2] * scale
    matrix = np.array([[scale[0], 0, offset[0]], [0, scale[1], offset[1]], [0, 0, 1]])
    return matrix


def align_arm(image, master):
    return transform(image,arm_alignment(image,master),(400,400))


def register_four_pose(image, master, old_master, side):
    # Keep the upper arm registered to the existing three-finger poses while
    # fitting the four generated pads to their four keycaps farther down.
    arm_matrix=registration(old_master,6,side)@arm_alignment(master,old_master)
    hand_matrix=registration(master,8,side)
    yy,xx=np.mgrid[:H,:W]
    start,end=(370,510) if side==0 else (330,460)
    t=np.clip((yy-start)/(end-start),0,1);t=t*t*(3-2*t)
    a,b=np.linalg.inv(arm_matrix),np.linalg.inv(hand_matrix)
    sx=(a[0,0]*xx+a[0,1]*yy+a[0,2])*(1-t)+(b[0,0]*xx+b[0,1]*yy+b[0,2])*t
    sy=(a[1,0]*xx+a[1,1]*yy+a[1,2])*(1-t)+(b[1,0]*xx+b[1,1]*yy+b[1,2])*t
    source=np.asarray(image.convert('RGBa'))
    result=np.stack([map_coordinates(source[:,:,c],[sy,sx],order=1,mode='constant') for c in range(4)],axis=-1)
    return Image.fromarray(result,'RGBa').convert('RGBA')


def lift_released_ring(image):
    # Two generated poses leave the curled ring pad grazing its key.  A small
    # continuous bend lifts that existing finger without adding or cutting art.
    yy,xx=np.mgrid[:H,:W]
    lift=10*np.exp(-((xx-370)/25)**4)*np.clip((yy-420)/30,0,1)
    source=np.asarray(image.convert('RGBa'))
    result=np.stack([map_coordinates(source[:,:,c],[yy+lift,xx],order=1,mode='constant') for c in range(4)],axis=-1)
    return Image.fromarray(result,'RGBa').convert('RGBA')


def down_tips(image, count):
    a = np.array(image)
    mask = (a[:, :, 3] > 180) & (a[:, :, 0] < 175) & (a[:, :, 1] < 165)
    bottom = np.zeros(400)
    for x in range(400):
        yy = np.where(mask[:, x])[0]
        if len(yy):
            bottom[x] = yy[-1]
    peaks, _ = find_peaks(gaussian_filter1d(bottom, 2), prominence=4, distance=25)
    peaks = sorted(sorted(peaks, key=lambda x: bottom[x], reverse=True)[:count])
    if len(peaks) != count:
        raise ValueError('Cannot identify the master pose fingertips')
    return np.array([(x, bottom[x]) for x in peaks], dtype=float)


def registration(master, mode, side):
    count = 4 if mode==8 else 2 if mode == 4 else 3
    tips = down_tips(master, count)
    targets = np.array(([(594, 526), (694, 543)] if side == 0 else [(408, 495), (498, 510)])
        if mode == 4 else ([(594, 526), (658, 537), (722, 548)] if side == 0
                           else [(370, 488), (434, 499), (498, 510)]), dtype=float)
    if mode==8:
        targets=np.array([(594,526),(658,537),(722,548),(786,559)] if side==0
                         else [(306,477),(370,488),(434,499),(498,510)],dtype=float)
    top = yellow_bounds(master)[1]
    sy = (targets[:, 1].mean() - (265 if side == 0 else 244)) / (tips[:, 1].mean() - top)
    x_shear = 0 if side == 0 else .05
    basis = np.column_stack((tips[:, 0], np.ones(count)))
    sx, tx = np.linalg.lstsq(basis, targets[:, 0] - x_shear * tips[:, 1], rcond=None)[0]
    y_shear, ty = np.linalg.lstsq(basis, targets[:, 1] - sy * tips[:, 1], rcond=None)[0]
    return np.array([[sx, x_shear, tx], [y_shear, sy, ty], [0, 0, 1]])


def generated_poses(mode, side):
    if mode == 4:
        extended = generated_poses(6, side)
        return [extended[i] for i in ((0, 2, 4, 6) if side == 0 else (0, 1, 2, 3))]
    name = 'left' if side == 0 else 'right'
    if mode==8:
        frames=generated_poses(6,side)
        master=split(f'{name}-four-pressed-clean.png',1)[0]
        poses={15:master}
        for pair in ((8,9),(10,11),(12,13),(14,15)):
            poses.update(zip(pair,split(f'{name}-poses-{pair[0]}-{pair[1]}-v6.png',2)))
        old_master=split(f'{name}-three-pressed-clean.png',1)[0]
        body=Image.open(ROOT/'SFML/SFML/base.png').convert('RGBA')
        for mask in range(8,16):
            frame=register_four_pose(align_arm(poses[mask],master),master,old_master,side)
            if side==1 and mask in (10,11):frame=lift_released_ring(frame)
            frames.append(merge_shoulder(frame,body,side))
        return frames
    master = split(f'{name}-three-pressed-clean.png', 1)[0]
    poses = {7: master}
    for pair in ((1, 6), (2, 5), (4, 3)):
        images = split(f'{name}-poses-{pair[0]}-{pair[1]}-v5.png', 2)
        poses.update(zip(pair, images))
    matrix = registration(master, mode, side)
    idle = list(cells(4))[side * 4]
    body = Image.open(ROOT / 'SFML/SFML/base.png').convert('RGBA')
    result = Image.new('RGBA', (W, H))
    result.alpha_composite(idle, VIEWPORTS[side][:2])
    frames = [merge_shoulder(result, body, side)]
    for mask in range(1, 4 if mode == 4 else 8):
        aligned = align_arm(poses[mask], master)
        frames.append(merge_shoulder(transform(aligned, matrix, (W, H)), body, side))
    return frames


def main():
    output = ART / 'runtime-candidate'
    output.mkdir(exist_ok=True)
    for mode in (4, 6, 8):
        frames = generated_poses(mode, 0) + generated_poses(mode, 1)
        hand_count=4 if mode==4 else 8 if mode==6 else 16
        rows=hand_count//2
        sheet = Image.new('RGBA', (W * 4, H * rows))
        preview = Image.new('RGB', (1600, 400 * rows), '#ebe8e3')
        draw = ImageDraw.Draw(preview)
        for i, frame in enumerate(frames):
            sheet.paste(frame, (i % 4 * W, i // 4 * H))
            side = i // hand_count
            crop = frame.crop(VIEWPORTS[side])
            preview.paste(crop, (i % 4 * 400, i // 4 * 400), crop)
            draw.text((i % 4 * 400 + 10, i // 4 * 400 + 10),
                      ('left' if side == 0 else 'right') + ' ' + str(i % hand_count), fill='#333333')
            frame.save(output / f'{mode}k-{i}.png')
        sheet.save(output / f'hands-{mode}k.png')
        preview.save(ART / f'arms-{mode}k-final-preview.jpg', quality=95)
    clean(Image.open(ART / 'idle-short-round-v2.png')).save(ART / 'idle-short-round-clean.png')
    print('Registered generated arms in runtime-candidate; current app assets are unchanged.')


if __name__ == '__main__':
    main()
