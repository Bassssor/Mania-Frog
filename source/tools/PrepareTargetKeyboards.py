"""Export the keyboard geometry using the same camera as the native tabletop."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    renderer=ROOT/'dist/milk-frog-4k/MilkFrog.exe'
    output=ROOT/'assets/milk-frog/four-finger'
    subprocess.run([str(renderer),'--export-keyboards',str(output)],check=True)
    print('Exported the three shared keyboards from KeyboardSurface.hpp.')


if __name__ == '__main__':
    main()
