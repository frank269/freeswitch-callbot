import xmlrpc.client
import json
s = xmlrpc.client.ServerProxy('http://172.16.88.13:9000')
# s = xmlrpc.client.ServerProxy('http://10.196.24.157:9000')
request = {
    "grpc_server" : "172.16.90.35:30066",
    "controller_url" : "http://172.16.90.35:30053/xmlrpc",
    "action_type" : 1,
    "conversation_id" : "20230509033010-abc4d0ed-4a0b-4886-8fb7-f25eee81604e",
    # "conversation_id" : "20230512035752-322af1fa-712d-41b9-9d12-3a83c91b6e14",
    "customer_number" : "1000@voice.metechvn.com",
    # "customer_number" : "0979019082",
    # "display_number" : "123456789",
    "display_number" : "842488898268",
    "transfer_extension" : "sip:900009@callbot.metechvn.com"
}
# for i in [3]:
    # request["customer_number"] = "user/{0}@callbot.metechvn.com".format(100001+i%5)
for i in range(1):
    print(s.callControllerServiceRequest(json.dumps(request)))