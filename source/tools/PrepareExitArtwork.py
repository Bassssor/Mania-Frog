"""Clean the generated full-body cutout and place it in the desktop scene."""
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import binary_dilation

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'assets/milk-frog/exit'

pixels = np.array(Image.open(ART / 'sad-full-body-generated.png').convert('RGBA'))
# Preserve the generated contour and its antialiasing, removing only faint
# background specks beyond the two-pixel feather around the character.
contour = binary_dilation(pixels[:, :, 3] >= 32, iterations=2)
pixels[~contour] = 0
character = Image.fromarray(pixels)
character = character.crop(character.getbbox())
character.save(ART / 'exit-character.png')

# The native view is x=120..1000, y=1..881. The entire pose fits inside it.
sprite = character.copy()
sprite.thumbnail((710, 760), Image.Resampling.LANCZOS)
scene = Image.new('RGBA', (1189, 880))
scene.alpha_composite(sprite, (560 - sprite.width // 2, 820 - sprite.height))
scene.save(ROOT / 'SFML/SFML/exit-pose.png')
assert scene.getbbox()[0] > 120 and scene.getbbox()[2] < 1000
assert scene.getbbox()[1] > 1 and scene.getbbox()[3] < 880
print('Prepared complete exit pose:', scene.getbbox())
