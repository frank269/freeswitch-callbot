import xmlrpc.client
import json

s = xmlrpc.client.ServerProxy('http://172.16.88.13:9000')
request = {
    "grpc_server" : "103.141.140.231:30067",
    "controller_url" : "http://10.196.24.16:9000",
    "action_type" : 1,
    "conversation_id" : "20230220111113-adb0d967-015e-45e5-bf2a-e9038fa2e8b9",
    "customer_number" : "user/10006@voice.metechvn.com",
    "display_number" : ""
}
print(s.callControllerServiceRequest(json.dumps(request)))