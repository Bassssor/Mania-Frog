# Registered four-finger artwork

Canvas: 1189 × 669. Character-left hand is screen-right. Four visible fingers per hand; thumbs hidden.

`hands-8k.png` contains 32 frames in a 4-column atlas: 16 left-hand masks, then 16 right-hand masks.
Canonical left mask bits: ring=1, middle=2, index=4, pinky=8. Right: index=1, middle=2, ring=4, pinky=8.
LaneLayout.hpp maps lane order to these art bits. `hands-4k.png` and `hands-6k.png` retain the registered mode subsets.
Keyboards 4K–8K use the same fingertip contacts. Do not run the retired procedural finger-extension workflow.

The accepted source images and prompts are in `../regenerated-hands/target-v11/`. `tools/PrepareTargetHandArtwork.py` traces and independently poses the four fingertips while preserving continuous wrist/forearm joins. Inspect runtime-candidate and copy accepted hands-*.png here. `build.ps1` generates the three shared keyboards from `KeyboardSurface.hpp` and copies runtime PNGs into the native build.
