import xmlrpc.client
import json
# s = xmlrpc.client.ServerProxy('http://172.16.88.13:9000')
s = xmlrpc.client.ServerProxy('http://172.16.90.45:9001')
# s = xmlrpc.client.ServerProxy('http://10.196.24.157:9000')
request = {
    "is_voicemail" : True,
    "conversion_id" : "16a54a05-c0c2-4041-ad80-4997f9db0a0c",
}

print(s.voicemailDetectResult(json.dumps(request)))