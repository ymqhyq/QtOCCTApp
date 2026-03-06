import urllib.request
import json

req = urllib.request.Request(
    'http://127.0.0.1:8000/api/v1/model/generate',
    data=b'{"code":"", "model_type":"bed_stone", "args":{}}',
    headers={'Content-Type': 'application/json'}
)

try:
    with urllib.request.urlopen(req) as resp:
        content = resp.read()
        print('SUCCESS:', len(content), 'bytes generated')
except urllib.error.HTTPError as e:
    print('HTTP ERROR:', e.code, e.reason)
    print(e.read().decode('utf-8', errors='ignore'))
except Exception as e:
    print('OTHER ERROR:', e)
