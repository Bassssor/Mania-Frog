"""Remove transparent atlas margins without changing any visible hand pixel."""
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


def main():
    atlas = Image.open(ROOT/'assets/milk-frog/four-finger/hands-8k.png').convert('RGBA')
    width, height = 1189, 669
    assert atlas.size == (width*4, height*8)
    frames = [atlas.crop((i % 4*width, i // 4*height,
                          i % 4*width+width, i // 4*height+height)) for i in range(32)]
    boxes = [frame.getbbox() for frame in frames]
    left = max(0, min(box[0] for box in boxes)-2)
    top = max(0, min(box[1] for box in boxes)-2)
    right = min(width, max(box[2] for box in boxes)+2)
    bottom = min(height, max(box[3] for box in boxes)+2)
    w, h = right-left, bottom-top
    packed = Image.new('RGBA', (w*4, h*8))
    for i, frame in enumerate(frames):
        packed.paste(frame.crop((left, top, right, bottom)), (i % 4*w, i // 4*h))
    output = ROOT/'SFML/SFML'
    packed.save(output/'hands-runtime.png')
    (output/'hands-runtime.txt').write_text(f'{w} {h} {left} {top}\n', encoding='ascii')
    print(f'Packed 32 exact hand crops: {w}x{h} cells, original {width}x{height}.')


if __name__ == '__main__':
    main()
