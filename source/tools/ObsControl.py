"""Authenticated local OBS API helper. Never exports passwords or stream keys."""
import base64
import hashlib
import json
import os
from pathlib import Path
import sys
import uuid
import websocket

sys.stdin.reconfigure(encoding='utf-8-sig')
sys.stdout.reconfigure(encoding='utf-8')


class Obs:
    def __init__(self):
        path = Path(os.environ['APPDATA']) / 'obs-studio/plugin_config/obs-websocket/config.json'
        config = json.loads(path.read_text(encoding='utf-8-sig'))
        self.ws = websocket.create_connection(f"ws://127.0.0.1:{config['server_port']}", timeout=15)
        hello = json.loads(self.ws.recv())['d']
        identify = {'rpcVersion': 1, 'eventSubscriptions': 0}
        if 'authentication' in hello:
            auth = hello['authentication']
            secret = base64.b64encode(hashlib.sha256((config['server_password'] + auth['salt']).encode()).digest()).decode()
            identify['authentication'] = base64.b64encode(hashlib.sha256((secret + auth['challenge']).encode()).digest()).decode()
        self.ws.send(json.dumps({'op': 1, 'd': identify}))
        if json.loads(self.ws.recv())['op'] != 2:
            raise RuntimeError('OBS authentication failed')

    def call(self, request_type, data=None):
        request_id = str(uuid.uuid4())
        self.ws.send(json.dumps({'op': 6, 'd': {'requestType': request_type, 'requestId': request_id, 'requestData': data or {}}}))
        while True:
            message = json.loads(self.ws.recv())
            if message.get('op') == 7 and message['d']['requestId'] == request_id:
                response = message['d']
                if not response['requestStatus']['result']:
                    raise RuntimeError(f"{request_type}: {response['requestStatus']}")
                return response.get('responseData', {})

    def close(self):
        self.ws.close()


if __name__ == '__main__':
    client = Obs()
    try:
        for request in json.load(sys.stdin):
            print(json.dumps({'request': request['type'], 'data': client.call(request['type'], request.get('data'))}, ensure_ascii=False))
    finally:
        client.close()
