"""Package the existing native PNG layers for OBS's real input-overlay source."""
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT.parent / 'obs-input-overlay' if ROOT.name == 'source' else ROOT / 'dist/input-overlay'
FRAMES = ROOT / 'dist/milk-frog-4k/verification'
SIZE, STRIDE = 880, 883


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Stock input-overlay draws the idle frame; the native animation filter
    # supplies the registered combinations and the two particle depth passes.
    idle = Image.new('RGBA', (882, 882))
    idle.paste(Image.open(FRAMES / 'state-0.png').convert('RGBA'), (1, 1))
    idle.save(OUT / 'milk-frog.png')
    for layer in ['background', 'foreground']:
        atlas = Image.new('RGBA', (STRIDE * 4 + 2, STRIDE * 4 + 2))
        for mask in range(16):
            frame = Image.open(FRAMES / f'{layer}-{mask}.png').convert('RGBA')
            if frame.size != (SIZE, SIZE):
                raise ValueError('Unregistered native layer')
            x, y = 1 + mask % 4 * STRIDE, 1 + mask // 4 * STRIDE
            atlas.paste(frame, (x, y))
        atlas.save(OUT / f'{layer}-atlas.png')
    config = {
        'overlay_width': SIZE, 'overlay_height': SIZE, 'flags': 0,
        'elements': [{'id': 'milk-frog-idle', 'type': 0, 'z_level': 0,
                      'pos': [0, 0], 'mapping': [1, 1, SIZE, SIZE]}],
        'milk_frog': {'version': 1, 'tile_size': SIZE, 'stride': STRIDE,
                      'background_atlas': 'background-atlas.png',
                      'foreground_atlas': 'foreground-atlas.png'},
    }
    (OUT / 'milk-frog.json').write_text(json.dumps(config, indent=2), encoding='utf-8')
    # Check atlas packing with exact RGBA comparisons, including transparent edges.
    for layer in ['background', 'foreground']:
        with Image.open(OUT / f'{layer}-atlas.png') as atlas:
            for mask in range(16):
                x, y = 1 + mask % 4 * STRIDE, 1 + mask // 4 * STRIDE
                expected = Image.open(FRAMES / f'{layer}-{mask}.png').convert('RGBA')
                assert atlas.crop((x, y, x + SIZE, y + SIZE)).tobytes() == expected.tobytes()
    print('PASS: native input-overlay PNG/JSON and 32 atlas frames exported exactly.')


if __name__ == '__main__':
    main()
