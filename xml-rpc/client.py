import xmlrpc.client
import json
s = xmlrpc.client.ServerProxy('http://14.232.240.205:9000')
# s = xmlrpc.client.ServerProxy('http://10.196.24.157:9000')
request = {
    "grpc_server" : "103.141.140.231:30067",
    "controller_url" : "http://10.196.24.157:9000",
    "action_type" : 1,
    "conversation_id" : "20230220111113-adb0d967-015e-45e5-bf2a-e9038fa2e8b9",
    "customer_number" : "0979019082",
    "display_number" : "123456789",
    "transfer_extension" : "sip:900009@callbot.metechvn.com"
}
# for i in [3]:
    # request["customer_number"] = "user/{0}@callbot.metechvn.com".format(100001+i%5)
for i in range(200):
    print(s.callControllerServiceRequest(json.dumps(request)))