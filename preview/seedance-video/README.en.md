# Seedance belly-laugh video

- `milk-frog-laugh-final-preview.mp4`: checkerboard review copy with only the supplied laugh.wav; original audio removed.
- `milk-frog-laugh-final-transparent.webm`: actual VP9 alpha / Opus asset, 720×720, 24 fps, 216 video frames, exactly 9 seconds of video.
- `laugh.wav`: supplied replacement audio with its original volume and leading silence.
- `make_transparent.py`: local processing using NumPy, SciPy, OpenCV and FFmpeg.
- `verify_media.py`: checks frame count, decoded alpha and correlation with the replacement WAV.
- `verification.json`: specifications and alpha checks after decoding the encoded file.

Silhouette matting preserves dark mouth/eye detail. Soft alpha and black unpremultiplication reduce fringes. The moving generation watermark has been repaired. The last second contains a staircase-shaped desk contour in the original generation, so the requested fallback trim retains 0–9 seconds. Replacement audio is trimmed to the same duration; the natural silent tail remains.

The supplied generated video provides the desired continuous 3D expression and body motion. HyperFrames can compose/render media, but does not itself generate these character movements.

The clip is integrated into both the desktop companion and the native OBS plugin. Default F5 starts or restarts playback; a gameplay key interrupts it immediately. Native frame data is frozen in `source/assets/milk-frog/laugh/` and `obs-input-overlay/`. Both players restore the existing idle artwork when playback ends.

Processing scripts use `ffmpeg` / `ffprobe` from PATH, or the `FFMPEG` / `FFPROBE` environment variables.
