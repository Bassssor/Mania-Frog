"""Verify the image CLI against a local fake relay without paid API calls."""
import base64
from email.parser import BytesParser
from email.policy import default
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
from threading import Thread
import unittest

SCRIPT = Path(__file__).with_name('GenerateImage.py')
PNG = base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aFZkAAAAASUVORK5CYII=')
KEY = 'milkfrog-local-test-key'


class Relay(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        self.server.requests.append((self.path, self.headers, body))
        self.send_response(self.server.status)
        if self.server.status == 302:
            self.send_header('Location', self.server.base + '/redirect-target')
        self.end_headers()
        self.wfile.write(json.dumps(self.server.payload).encode('utf-8'))

    def do_GET(self):
        self.server.requests.append((self.path, self.headers, b''))
        self.send_response(200)
        self.end_headers()
        self.wfile.write(PNG)


class ImageGenerationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), Relay)
        cls.server.base = f'http://127.0.0.1:{cls.server.server_port}'
        cls.worker = Thread(target=cls.server.serve_forever, daemon=True)
        cls.worker.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.worker.join()

    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.output = self.directory / '奶蛙候选.png'
        self.server.status = 200
        self.server.requests = []
        self.server.payload = {'data': [{'b64_json': base64.b64encode(PNG).decode()}]}

    def run_cli(self, *options, prompt=True, success=True):
        command = [sys.executable, str(SCRIPT), '--base-url', self.server.base,
                   '--output', str(self.output), '--timeout', '5']
        if prompt:
            command += ['--prompt', '奶白色小青蛙，桌宠素材']
        result = subprocess.run(command + list(options), capture_output=True,
                                encoding='utf-8', timeout=15,
                                env=dict(os.environ, MILKFROG_IMAGE_API_KEY=KEY,
                                         PYTHONIOENCODING='utf-8'))
        self.assertEqual(result.returncode, 0 if success else 1, result.stdout + result.stderr)
        self.assertNotIn(KEY, result.stdout + result.stderr)
        return result

    def test_text_generation_and_prompt_record(self):
        self.run_cli('--background', 'transparent')
        path, headers, body = self.server.requests[0]
        self.assertEqual(path, '/v1/images/generations')
        self.assertEqual(headers['Authorization'], 'Bearer ' + KEY)
        payload = json.loads(body)
        self.assertEqual(payload, {'model': 'image2.5sunburst', 'prompt': '奶白色小青蛙，桌宠素材',
                                  'n': 1, 'size': '1024x1024', 'background': 'transparent'})
        self.assertEqual(self.output.read_bytes(), PNG)
        text = self.output.with_suffix('.png.json').read_text(encoding='utf-8')
        self.assertNotIn(KEY, text)
        self.assertEqual(json.loads(text)['prompt'], payload['prompt'])
        self.assertEqual(len(self.server.requests), 1)

    def test_prompt_file_with_utf8_bom_and_v1_url(self):
        prompt = self.directory / '提示词.txt'
        prompt.write_text('参考奶蛙造型\n生成待机姿势', encoding='utf-8-sig')
        self.run_cli('--prompt-file', str(prompt), '--base-url', self.server.base + '/v1/', prompt=False)
        path, _, body = self.server.requests[0]
        self.assertEqual(path, '/v1/images/generations')
        self.assertEqual(json.loads(body)['prompt'], '参考奶蛙造型\n生成待机姿势')

    def test_reference_images_multipart(self):
        for count in (1, 2):
            with self.subTest(count=count):
                self.output = self.directory / f'edit-{count}.png'
                options = []
                for index in range(count):
                    reference = self.directory / f'参考图{index}.png'
                    reference.write_bytes(PNG)
                    options += ['--image', str(reference)]
                self.run_cli(*options)
                path, headers, body = self.server.requests[-1]
                self.assertEqual(path, '/v1/images/edits')
                message = BytesParser(policy=default).parsebytes(
                    ('Content-Type: ' + headers['Content-Type'] + '\r\n\r\n').encode() + body)
                parts = list(message.iter_parts())
                images = [part for part in parts if part.get_filename()]
                self.assertEqual(len(images), count)
                for part in images:
                    self.assertEqual(part.get_param('name', header='content-disposition'),
                                     'image' if count == 1 else 'image[]')
                    self.assertEqual(part.get_payload(decode=True), PNG)

    def test_download_url_does_not_receive_api_key(self):
        self.server.payload = {'data': [{'url': self.server.base + '/result.png'}]}
        self.run_cli()
        self.assertEqual(self.output.read_bytes(), PNG)
        self.assertEqual(len(self.server.requests), 2)
        self.assertIsNone(self.server.requests[1][1]['Authorization'])

    def test_http_error_is_redacted_and_not_retried(self):
        self.server.status = 401
        self.server.payload = {'error': {'message': 'Invalid key: ' + KEY}}
        result = self.run_cli(success=False)
        self.assertIn('HTTP 401', result.stderr)
        self.assertIn('[REDACTED]', result.stderr)
        self.assertEqual(len(self.server.requests), 1)
        self.assertFalse(self.output.exists())

    def test_authenticated_redirect_is_not_followed(self):
        self.server.status = 302
        self.run_cli(success=False)
        self.assertEqual(len(self.server.requests), 1)

    def test_existing_artwork_is_preserved_without_a_request(self):
        self.output.write_bytes(b'approved artwork')
        self.run_cli(success=False)
        self.assertEqual(self.server.requests, [])
        self.assertEqual(self.output.read_bytes(), b'approved artwork')

    def test_missing_reference_fails_before_a_request(self):
        self.run_cli('--image', str(self.directory / 'missing.png'), success=False)
        self.assertEqual(self.server.requests, [])

    def test_non_image_response_is_not_saved(self):
        self.server.payload = {'data': [{'b64_json': base64.b64encode(b'<html>error</html>').decode()}]}
        self.run_cli(success=False)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
