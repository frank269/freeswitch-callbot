from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
import xmlrpc.client
import json
import time
from pbxConstant import *

class CallRequest():
    def __init__(self, json_str: str) -> None:
        request = json.loads(json_str)
        self.grpc_server = request['grpc_server']
        self.controller_url = request['controller_url']
        self.action_type = request['action_type']
        self.conversation_id = request['conversation_id']
        self.customer_number = request['customer_number']
        self.display_number = request['display_number']
        self.call_at = time.time() * 1000
        
    def __str__(self):
        return "{{CALLBOT_MASTER_URI={0},SESSION_ID={1},CALLBOT_CONTROLLER_URI={2}}}{3}".format(
            self.grpc_server, 
            self.conversation_id,
            self.controller_url,
            self.customer_number)


class CallResponse():
    def __init__(self, conversation_id: str, call_at: int, pickup_at: int, hangup_at: int, hangup_cause: str, status: int) -> None:
        self.conversation_id = conversation_id
        self.call_at = call_at
        self.pickup_at = pickup_at
        self.hangup_at = hangup_at
        self.status = status
        self.sip_code = PbxHangupCause[hangup_cause].value
        
    def __str__(self):
        return json.dumps({
            "conversation_id": self.conversation_id,
            "call_at": self.call_at,
            "pickup_at": self.pickup_at,
            "hangup_at": self.hangup_at,
            "status": self.status,
            "sip_code": self.sip_code,
        })

class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)

with SimpleXMLRPCServer(('0.0.0.0', 9000), requestHandler=RequestHandler) as server:
    server.register_introspection_functions()
    
    def sendToFreeswitchServer(server_uri: str, functionName: str, content: str):
        serv = xmlrpc.client.ServerProxy(server_uri)
        return serv.freeswitch.api(functionName, content)

    def sendEndCallToCallControllerServer(server_uri: str, content: str):
        serv = xmlrpc.client.ServerProxy(server_uri)
        return serv.phoneGatewayEndCall(content)

    @server.register_function
    def callControllerServiceRequest(json_request: str):
        call_request = CallRequest(json_request)
        print("callControllerServiceRequest request: {}".format(json_request))
        server_response = sendToFreeswitchServer("http://%s:%s@%s:%s" % (pbx_username, pbx_password, pbx_host, pbx_port),
                                   "originate",
                                   "{0} &start_call_with_bot".format(call_request))
        print("callControllerServiceRequest server_response: {}".format(server_response))
        response = {
                "status" : 0,
                "msg" : "success"
            }
        if "-ERR" in server_response:
            hangup_cause = server_response.strip().split("-ERR ")[1]
            call_response = CallResponse(call_request.conversation_id,call_request.call_at, 0, 0, hangup_cause, 103)
            print(sendEndCallToCallControllerServer(call_request.controller_url, call_response.__str__()))
            response = {
                "status" : -1,
                "msg" : server_response
            }

        return json.dumps(response)

    @server.register_function
    def phoneGatewayEndCall(json_response: str):
        print("phoneGatewayEndCall message: {}".format(json_response))
        return json.dumps({
            "status" : 0,
            "msg" : "success"
        })
    
    server.serve_forever()