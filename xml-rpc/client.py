import xmlrpc.client
import json
# s = xmlrpc.client.ServerProxy('http://172.16.88.13:9000')
s = xmlrpc.client.ServerProxy('http://172.16.90.41:9000')
# s = xmlrpc.client.ServerProxy('http://10.196.24.157:9000')
request = {
    "grpc_server" : "172.16.90.35:30066",
    "controller_url" : "http://172.16.90.35:30053/xmlrpc",
    "action_type" : 1,
    # "conversation_id" : "20230518035143-8a0a2fd5-cd86-44e1-8663-d964373292ac",
    "conversation_id" : "20230524070310-ccd3632c-0778-47d4-bbcd-15ce0682903d",
    "customer_number" : "1000@callbot.metechvn.com",
    # "customer_number" : "0979019082",
    "display_number" : "123456789",
    # "display_number" : "842488898168",
    # "transfer_extension" : "sip:1068@210.245.26.17:5060"
}
# for i in [3]:
    # request["customer_number"] = "user/{0}@callbot.metechvn.com".format(100001+i%5)
for i in range(1):
    print(s.callControllerServiceRequest(json.dumps(request)))