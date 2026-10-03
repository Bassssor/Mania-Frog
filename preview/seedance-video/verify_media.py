"""Check the actual encoded deliverables, including replacement audio."""
import json
import os
from pathlib import Path
import subprocess
import numpy as np

ROOT = Path(__file__).parent
FFMPEG = os.environ.get('FFMPEG', 'ffmpeg')
FFPROBE = os.environ.get('FFPROBE', 'ffprobe')


def audio(path):
    raw = subprocess.check_output([FFMPEG, '-v', 'error', '-i', str(path), '-map', '0:a:0',
        '-t', '9', '-ac', '1', '-ar', '48000', '-f', 'f32le', 'pipe:1'])
    return np.frombuffer(raw, '<f4')


def main():
    reference = audio(ROOT / 'laugh.wav')
    result = {}
    for suffix in ['preview.mp4', 'transparent.webm']:
        path = ROOT / f'milk-frog-laugh-final-{suffix}'
        metadata = json.loads(subprocess.check_output([FFPROBE, '-v', 'error', '-count_frames',
            '-show_entries', 'stream=codec_name,width,height,avg_frame_rate,nb_read_frames:stream_tags=alpha_mode:format=duration',
            '-of', 'json', str(path)]))
        assert len(metadata['streams']) == 2, 'Exactly one video and one replacement audio stream required'
        video = metadata['streams'][0]
        assert (video['width'], video['height'], int(video['nb_read_frames'])) == (720, 720, 216)
        decoded = audio(path)
        # The real laugh is between about 2.75 and 7.5 seconds. Compare its
        # samples against the supplied WAV after resampling and encoding.
        begin, end = 144000, min(len(decoded), len(reference), 336000)
        correlation = float(np.corrcoef(reference[begin:end], decoded[begin:end])[0, 1])
        assert correlation > .98, 'Encoded audio does not match the replacement WAV'
        early_rms = float(np.sqrt(np.mean(decoded[:120000] ** 2)))
        assert early_rms < .001, 'Original audio may still be present during the WAV silence'
        result[suffix] = {'metadata': metadata, 'replacementAudioCorrelation': correlation,
            'earlySilenceRms': early_rms, 'sourceAudioIncluded': False}
        if suffix.endswith('webm'):
            tags = {key.lower(): value for key, value in video.get('tags', {}).items()}
            assert tags.get('alpha_mode') == '1'
            raw = subprocess.check_output([FFMPEG, '-v', 'error', '-c:v', 'libvpx-vp9', '-ss', '3',
                '-i', str(path), '-frames:v', '1', '-vf', 'alphaextract', '-f', 'rawvideo', '-pix_fmt', 'gray', 'pipe:1'])
            alpha = np.frombuffer(raw, np.uint8).reshape(720, 720)
            assert alpha[0, 0] == 0 and alpha[235, 352] == 255
            result[suffix]['alpha'] = {'transparentPixels': int((alpha == 0).sum()),
                'opaquePixels': int((alpha == 255).sum()), 'mouthAlpha': int(alpha[235, 352]),
                'cornerAlpha': int(alpha[0, 0])}
    report = json.loads((ROOT / 'verification.json').read_text(encoding='utf-8'))
    report.update({'encodedMediaVerification': result, 'audioSource': str(ROOT / 'laugh.wav'),
        'originalAudioIncluded': False, 'transparentVideo': str(ROOT / 'milk-frog-laugh-final-transparent.webm'),
        'checkerboardPreview': str(ROOT / 'milk-frog-laugh-final-preview.mp4')})
    (ROOT / 'verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
