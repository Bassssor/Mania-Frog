"""Matte this black-background video without deleting black mouth/eye detail.

Run locally with NumPy/SciPy and FFmpeg. Existing OBS/desktop assets are never
written. First make a silhouette by filling enclosed dark regions; estimate
alpha only on its outer edge, and unpremultiply those pixels from black.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import cv2
import numpy as np
from scipy import ndimage as ndi

FFMPEG = os.environ.get('FFMPEG', 'ffmpeg')
FFPROBE = os.environ.get('FFPROBE', 'ffprobe')


def remove_watermark(rgb, seconds):
    result = rgb.copy()
    # The moving label occupies these two corners. The upper-left label is
    # entirely on black throughout this video, so restore that background.
    result[14:55, 14:145] = 0
    changed = np.count_nonzero(np.any(result != rgb, axis=2))
    if seconds < 4.25 or seconds >= 8:
        # Include the label's vertical drift. Repair a generous glyph mask
        # instead of a rectangle, so the desk edge stays continuous.
        x0, y0, x1, y1 = 580, 643, 719, 719
        grey = cv2.cvtColor(result, cv2.COLOR_RGB2GRAY)
        residual = cv2.subtract(grey, cv2.medianBlur(grey, 25))
        mask = np.zeros(result.shape[:2], np.uint8)
        mask[y0:y1, x0:x1] = (residual[y0:y1, x0:x1] > 4).astype(np.uint8) * 255
        mask = cv2.dilate(mask, np.ones((3, 3), np.uint8), iterations=4)
        result = cv2.inpaint(result, mask, 7, cv2.INPAINT_NS)
        changed += np.count_nonzero(mask)
    return result, changed


def matte(rgb):
    brightness = rgb.max(axis=2)
    # Preserve enclosed black eye/mouth regions. Unlike chromakey/lumakey,
    # this does not make every dark pixel transparent.
    silhouette = ndi.binary_fill_holes(brightness > 22)
    core = ndi.binary_erosion(silhouette, iterations=2, border_value=1)
    distance, indices = ndi.distance_transform_edt(~core, return_indices=True)
    nearest = rgb[indices[0], indices[1]].max(axis=2).astype(np.float32)
    edge = (~core) & (distance <= 4)
    alpha = silhouette.astype(np.float32)
    alpha[edge] = np.clip(brightness[edge] / np.maximum(nearest[edge], 30), 0, 1)
    alpha[(~silhouette) & (brightness < 8)] = 0
    color = rgb.astype(np.float32)
    unpremultiply = (alpha > .02) & (alpha < .999)
    color[unpremultiply] /= alpha[unpremultiply, None]
    color[alpha <= .02] = 0
    rgba = np.dstack([np.clip(color, 0, 255).astype(np.uint8), np.round(alpha * 255).astype(np.uint8)])
    return rgba, silhouette


def read_exact(pipe, size):
    blocks = []
    remaining = size
    while remaining:
        part = pipe.read(remaining)
        if not part:
            break
        blocks.append(part)
        remaining -= len(part)
    data = b''.join(blocks)
    if data and len(data) != size:
        raise RuntimeError('Incomplete decoded video frame')
    return data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--output-dir', type=Path, default=Path(__file__).parent)
    parser.add_argument('--duration', type=float, default=9)
    parser.add_argument('--audio', type=Path, default=Path(__file__).parent / 'laugh.wav')
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if not args.audio.is_file():
        raise FileNotFoundError('Replacement laugh.wav is required; source audio is never used')
    metadata = json.loads(subprocess.check_output([FFPROBE, '-v', 'error', '-select_streams', 'v:0',
        '-show_entries', 'stream=width,height,avg_frame_rate', '-of', 'json', str(args.source)]))
    stream = metadata['streams'][0]
    width, height, fps = stream['width'], stream['height'], stream['avg_frame_rate']
    rate_num, rate_den = map(int, fps.split('/'))
    rate = rate_num / rate_den
    duration = str(args.duration)
    decoder = subprocess.Popen([FFMPEG, '-hide_banner', '-loglevel', 'error', '-i', str(args.source),
        '-t', duration, '-an', '-f', 'rawvideo', '-pix_fmt', 'rgb24', 'pipe:1'], stdout=subprocess.PIPE)
    common = [FFMPEG, '-hide_banner', '-loglevel', 'error', '-y', '-f', 'rawvideo',
        '-pixel_format', 'rgba', '-video_size', f'{width}x{height}', '-framerate', fps, '-i', 'pipe:0']
    transparent = out / 'milk-frog-laugh-final-transparent.webm'
    encoder = subprocess.Popen(common + ['-i', str(args.audio), '-map', '0:v', '-map', '1:a:0',
        '-t', duration, '-c:v', 'libvpx-vp9', '-pix_fmt', 'yuva420p', '-crf', '22', '-b:v', '0',
        '-row-mt', '1', '-threads', '4', '-auto-alt-ref', '0', '-c:a', 'libopus', str(transparent)], stdin=subprocess.PIPE)
    preview = out / 'milk-frog-laugh-final-preview.mp4'
    review_encoder = subprocess.Popen([FFMPEG, '-hide_banner', '-loglevel', 'error', '-y',
        '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size', f'{width}x{height}', '-framerate', fps,
        '-i', 'pipe:0', '-i', str(args.audio), '-map', '0:v', '-map', '1:a:0', '-t', duration, '-c:v', 'libx264',
        '-crf', '18', '-preset', 'fast', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
        '-c:a', 'aac', str(preview)], stdin=subprocess.PIPE)
    yy, xx = np.indices((height, width))
    checker = ((xx // 24 + yy // 24) % 2)[..., None]
    backdrop = np.where(checker, np.array([45, 52, 60]), np.array([37, 43, 49])).astype(np.float32)
    frames, black_preserved, clear_pixels, intermediate_pixels = 0, 0, 0, 0
    watermark_repair_pixels = 0
    try:
        while data := read_exact(decoder.stdout, width * height * 3):
            rgb = np.frombuffer(data, np.uint8).reshape(height, width, 3)
            rgb, repaired = remove_watermark(rgb, frames / rate)
            watermark_repair_pixels += int(repaired)
            rgba, silhouette = matte(rgb)
            encoder.stdin.write(rgba.tobytes())
            alpha = rgba[..., 3:4].astype(np.float32) / 255
            review = np.round(rgba[..., :3] * alpha + backdrop * (1 - alpha)).astype(np.uint8)
            review_encoder.stdin.write(review.tobytes())
            black_preserved += int(((rgb.max(axis=2) < 20) & (rgba[..., 3] == 255) & silhouette).sum())
            clear_pixels += int((rgba[..., 3] == 0).sum())
            intermediate_pixels += int(((rgba[..., 3] > 0) & (rgba[..., 3] < 255)).sum())
            if frames in [0, 24, 48, 72, 120, 192, 215]:
                # QA raw pixels, not a delivered re-edited still asset.
                np.save(out / f'qa-frame-{frames:03}.npy', rgba)
            frames += 1
            if frames % 48 == 0:
                print(f'Matted {frames} frames', flush=True)
    finally:
        decoder.stdout.close()
        encoder.stdin.close()
        review_encoder.stdin.close()
    for process in [decoder, encoder, review_encoder]:
        if process.wait() != 0:
            raise RuntimeError('Video decode or encode failed')
    report = {'source': str(args.source), 'durationSeconds': args.duration, 'fps': fps,
        'frames': frames, 'dimensions': [width, height], 'transparentPixels': clear_pixels,
        'softAlphaPixels': intermediate_pixels, 'enclosedBlackPixelsPreserved': black_preserved,
        'transparentVideo': str(transparent), 'checkerboardPreview': str(preview),
        'watermarkRepairPixels': watermark_repair_pixels,
        'audioSource': str(args.audio), 'originalAudioIncluded': False,
        'trimReason': 'Original generated desk contour becomes a staircase in the final second',
        'retainedIntervalSeconds': [0, args.duration],
        'knownLimitations': ['Generated camera zoom remains', 'Endpoints do not yet match native idle',
            'Silhouette matting may retain enclosed background gaps']}
    (out / 'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
