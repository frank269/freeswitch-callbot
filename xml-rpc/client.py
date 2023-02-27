import xmlrpc.client
import json

s = xmlrpc.client.ServerProxy('http://localhost:9000')
request = {
    "grpc_server" : "103.141.140.231:30067",
    "controller_url" : "localhost:9000",
    "action_type" : 1,
    "conversation_id" : "20230220111113-adb0d967-015e-45e5-bf2a-e9038fa2e8b9",
    "customer_number" : "user/100019@uat1.metechvn.com",
    "display_number" : ""
}
print(s.callControllerServiceRequest(json.dumps(request)))