import xmlrpc.client
import json
import time
# s = xmlrpc.client.ServerProxy('https://a25c-1-55-211-83.ngrok-free.app')
# s = xmlrpc.client.ServerProxy('http://172.16.90.41:9000')
s = xmlrpc.client.ServerProxy('http://10.196.24.157:9000')
request = {
    "grpc_server" : "172.16.90.35:30066",
    "controller_url" : "http://10.196.24.157:9000/RPC2",
    "action_type" : 1,
    # "conversation_id" : "20230518035143-8a0a2fd5-cd86-44e1-8663-d964373292ac",
    "conversation_id" : "20230810150214-c5206bb9-12c1-4291-a5a6-69d71fba4b06",
    # "customer_number" : "1000@voice.metechvn.com",
    "customer_number" : "0979019082",
    # "customer_number" : "0373944950",
    "display_number" : "123456789",
    # "display_number" : "842488898468",
    # "transfer_extension" : "sip:1068@210.245.26.17:5060"
}
# for i in [3]:
    # request["customer_number"] = "user/{0}@callbot.metechvn.com".format(100001+i%5)
while(True):
    for i in range(1):
        print(s.callControllerServiceRequest(json.dumps(request)))
    time.sleep(60)