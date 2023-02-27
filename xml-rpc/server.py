from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import xmlrpc.client
import json


pbx_host = '172.16.88.38'
pbx_username = 'freeswitch'
pbx_password = 'works'
pbx_port = '8080'

class CallRequest():
    
    def __init__(self, json_str: str) -> None:
        request = json.loads(json_str)
        self.grpc_server = request['grpc_server']
        self.controller_url = request['controller_url']
        self.action_type = request['action_type']
        self.conversation_id = request['conversation_id']
        self.customer_number = request['customer_number']
        self.display_number = request['display_number']
        
    def __str__(self):
        return "{{CALLBOT_MASTER_URI={0},SESSION_ID={1},CALLBOT_CONTROLLER_URI={2}}}{3}".format(
            self.grpc_server, 
            self.conversation_id,
            self.controller_url,
            self.customer_number)


class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)

with SimpleXMLRPCServer(('0.0.0.0', 9000), requestHandler=RequestHandler) as server:
    server.register_introspection_functions()
    @server.register_function
    def callControllerServiceRequest(json_request: str):
        serv = xmlrpc.client.ServerProxy("http://%s:%s@%s:%s" % (pbx_username, pbx_password, pbx_host, pbx_port))
        response = serv.freeswitch.api("originate", 
                               "{0} &start_call_with_bot".format(CallRequest(json_request)))
        print(response)
        return json.dumps({
            "status" : 0,
            "msg" : "success"
        })

    @server.register_function
    def phoneGatewayEndCall(json_response: str):
        print(json_response)
        return json.dumps({
            "status" : 0,
            "msg" : "success"
        })
    
    server.serve_forever()