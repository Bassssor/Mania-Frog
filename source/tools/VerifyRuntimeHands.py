"""Check that the compact runtime atlas preserves every hand and its placement."""
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
atlas = Image.open(ROOT/'assets/milk-frog/four-finger/hands-8k.png').convert('RGBA')
packed = Image.open(ROOT/'SFML/SFML/hands-runtime.png').convert('RGBA')
w, h, left, top = map(int, (ROOT/'SFML/SFML/hands-runtime.txt').read_text().split())
assert packed.size == (w*4, h*8)
for i in range(32):
    source = np.asarray(atlas.crop((i % 4*1189, i // 4*669,
                                   i % 4*1189+1189, i // 4*669+669)))
    frame = Image.new('RGBA', (1189, 669))
    frame.paste(packed.crop((i % 4*w, i // 4*h, i % 4*w+w, i // 4*h+h)), (left, top))
    actual = np.asarray(frame)
    assert np.array_equal(source[:, :, 3], actual[:, :, 3]), f'alpha or placement changed: {i}'
    visible = source[:, :, 3] > 0
    assert np.array_equal(source[visible], actual[visible]), f'visible colour changed: {i}'
print('PASS: all 32 runtime hand poses preserve exact alpha, visible RGB and placement.')
