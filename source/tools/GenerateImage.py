"""Generate MilkFrog artwork through the configured OpenAI-compatible relay."""
import argparse
import base64
import getpass
import json
import mimetypes
import os
from pathlib import Path
import sys
from datetime import datetime
from urllib.error import HTTPError, URLError
from urllib.parse import urlparse
from urllib.request import HTTPRedirectHandler, Request, build_opener, urlopen
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[1]
BASE_URL = 'https://ubuntu.0x40e.eu.org:2083/v1'
MODEL = 'image2.5sunburst'
KEY_ENV = 'MILKFROG_IMAGE_API_KEY'


class NoRedirect(HTTPRedirectHandler):
    # A relay redirect must not forward the API key to another host.
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def api_key():
    key = os.environ.get(KEY_ENV, '').strip()
    if not key and os.name == 'nt':
        # Also see a user environment variable set after Codex was launched.
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment') as registry:
                key = str(winreg.QueryValueEx(registry, KEY_ENV)[0]).strip()
        except OSError:
            pass
    if not key and sys.stdin.isatty():
        key = getpass.getpass('Relay API key (hidden, not saved): ').strip()
    if not key:
        raise ValueError(f'Set {KEY_ENV} to your relay API key first.')
    return key


def multipart(fields, images):
    boundary = 'MilkFrog' + uuid4().hex
    chunks = []
    for name, value in fields.items():
        chunks.append((f'--{boundary}\r\nContent-Disposition: form-data; '
                       f'name="{name}"\r\n\r\n{value}\r\n').encode('utf-8'))
    field = 'image' if len(images) == 1 else 'image[]'
    for index, path in enumerate(images):
        mime = mimetypes.guess_type(path.name)[0] or 'application/octet-stream'
        chunks.append((f'--{boundary}\r\nContent-Disposition: form-data; '
                       f'name="{field}"; filename="reference-{index}{path.suffix.lower()}"\r\n'
                       f'Content-Type: {mime}\r\n\r\n').encode('ascii'))
        chunks.extend([path.read_bytes(), b'\r\n'])
    chunks.append(f'--{boundary}--\r\n'.encode('ascii'))
    return b''.join(chunks), f'multipart/form-data; boundary={boundary}'


def image_suffix(content):
    if content.startswith(b'\x89PNG\r\n\x1a\n'):
        return '.png'
    if content.startswith(b'\xff\xd8\xff'):
        return '.jpg'
    if content.startswith(b'RIFF') and content[8:12] == b'WEBP':
        return '.webp'
    raise ValueError('The server did not return a PNG, JPEG or WebP image.')


def generate(args, prompt, images, key):
    base = args.base_url.rstrip('/')
    parsed = urlparse(base)
    if parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ValueError('Use a base URL without credentials, query or fragment.')
    local = parsed.hostname in ('127.0.0.1', 'localhost', '::1')
    if parsed.scheme != 'https' and not (parsed.scheme == 'http' and local):
        raise ValueError('The relay base URL must use HTTPS.')
    if not parsed.netloc:
        raise ValueError('The relay base URL needs a hostname.')
    if not base.endswith('/v1'):
        base += '/v1'
    fields = {'model': args.model, 'prompt': prompt, 'n': 1, 'size': args.size}
    for name in ('background', 'quality'):
        if getattr(args, name) is not None:
            fields[name] = getattr(args, name)
    if images:
        body, content_type = multipart(fields, images)
        endpoint = '/images/edits'
    else:
        body = json.dumps(fields, ensure_ascii=False).encode('utf-8')
        content_type = 'application/json'
        endpoint = '/images/generations'
    request = Request(base + endpoint, data=body, headers={
        'Authorization': 'Bearer ' + key, 'Content-Type': content_type,
        'Accept': 'application/json', 'User-Agent': 'MilkFrog-Artwork/1.0',
    })
    print(f'Requesting {args.model} via {endpoint} ...', flush=True)
    # Do not retry a paid generation automatically after a timeout or error.
    with build_opener(NoRedirect()).open(request, timeout=args.timeout) as response:
        result = json.load(response)
    if not isinstance(result, dict) or not result.get('data'):
        raise ValueError('Expected an Images API response with a non-empty data array.')
    item = result['data'][0]
    if item.get('b64_json'):
        content = base64.b64decode(item['b64_json'], validate=True)
    elif item.get('url'):
        url = item['url']
        if urlparse(url).scheme not in ('https', 'http'):
            raise ValueError('The image download URL must use HTTP or HTTPS.')
        # The download request deliberately carries no relay Authorization header.
        with urlopen(url, timeout=args.timeout) as response:
            content = response.read()
    else:
        raise ValueError('Expected data[0].b64_json or data[0].url in the response.')
    suffix = image_suffix(content)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    output = (Path(args.output) if args.output else
              ROOT / 'assets/milk-frog/generated' / (stamp + suffix)).resolve()
    if output.suffix.lower() not in ({'.jpg', '.jpeg'} if suffix == '.jpg' else {suffix}):
        raise ValueError(f'The returned image is {suffix}; use that output extension.')
    metadata = output.with_suffix(output.suffix + '.json')
    if output.exists() or metadata.exists():
        raise ValueError(f'Output already exists; choose another name: {output}')
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as handle:
        handle.write(content)
    record = dict(fields, base_url=base, created_at=datetime.now().isoformat(),
                  references=[str(path.resolve()) for path in images],
                  revised_prompt=item.get('revised_prompt'), output=str(output))
    with metadata.open('x', encoding='utf-8') as handle:
        json.dump(record, handle, ensure_ascii=False, indent=2)
        handle.write('\n')
    print(f'Saved image: {output}\nSaved prompt: {metadata}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--prompt', help='Text describing the desired artwork')
    source.add_argument('--prompt-file', type=Path, help='UTF-8 text file containing the prompt')
    parser.add_argument('--image', action='append', type=Path, default=[],
                        help='Reference PNG/JPEG/WebP; repeat for multiple references (Images edits API)')
    parser.add_argument('--output', help='New output filename; default: assets/milk-frog/generated/<timestamp>')
    parser.add_argument('--size', default='1024x1024', help='Image dimensions (default: 1024x1024)')
    parser.add_argument('--background', choices=['transparent', 'opaque', 'auto'])
    parser.add_argument('--quality', choices=['low', 'medium', 'high', 'auto'])
    parser.add_argument('--base-url', default=BASE_URL, help='Relay URL, with or without /v1')
    parser.add_argument('--model', default=MODEL)
    parser.add_argument('--timeout', type=float, default=300, help='Network timeout in seconds')
    args = parser.parse_args()
    key = ''
    try:
        prompt = (args.prompt_file.read_text(encoding='utf-8-sig')
                  if args.prompt_file else args.prompt).strip()
        if not prompt:
            raise ValueError('The prompt must not be empty.')
        if args.timeout <= 0:
            raise ValueError('The timeout must be positive.')
        for image in args.image:
            if not image.is_file() or image.suffix.lower() not in ('.png', '.jpg', '.jpeg', '.webp'):
                raise ValueError(f'Reference must be an existing PNG/JPEG/WebP: {image}')
        if args.output:
            output = Path(args.output)
            if output.exists() or output.with_suffix(output.suffix + '.json').exists():
                raise ValueError(f'Output already exists; choose another name: {output}')
        key = api_key()
        generate(args, prompt, args.image, key)
        return 0
    except HTTPError as error:
        detail = error.read(4096).decode('utf-8', errors='replace')
        message = f'HTTP {error.code}: {detail}'
    except (OSError, ValueError, KeyError, TypeError, URLError) as error:
        message = str(error)
    except KeyboardInterrupt:
        message = 'Cancelled. If the request was already sent, check relay usage before retrying.'
    if key:
        message = message.replace(key, '[REDACTED]')
    print('Image generation failed: ' + message, file=sys.stderr)
    return 1


if __name__ == '__main__':
    raise SystemExit(main())
