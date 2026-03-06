import urllib.request
import json
req = urllib.request.Request('http://localhost:8000/api/v1/model/generate', data=b'{"code":"", "model_type":"girder", "args":{}}', headers={'Content-Type': 'application/json'})
try:
    urllib.request.urlopen(req)
    print("Success")
except Exception as e:
    print(e.read().decode('utf-8'))
