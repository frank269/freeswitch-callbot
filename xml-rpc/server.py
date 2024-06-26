from xmlrpc.server import SimpleXMLRPCServer
from xmlrpc.server import SimpleXMLRPCRequestHandler
from socketserver import ThreadingMixIn
import xmlrpc.client
import json
import time
from pbxConstant import *
# from esl_thread import *
import logging
from uuid import uuid4
from multiprocessing import Process

logging.basicConfig(filename="logs/out.log",
                    filemode='a',
                    format='%(asctime)s,%(msecs)d %(name)s %(levelname)s %(message)s',
                    datefmt='%H:%M:%S',
                    level=logging.DEBUG)

logger = logging.getLogger(__name__)

USER_NO_RESPONSE_CAUSE = ["USER_BUSY", "NO_USER_RESPONSE", "NO_ANSWER", "CALL_REJECTED"]

class CallRequest():
    def __init__(self, json_str: str) -> None:
        request = json.loads(json_str)
        self.grpc_server = request['grpc_server']
        self.controller_url = request['controller_url']
        self.action_type = request['action_type']
        self.conversation_id = request['conversation_id']
        req_number = request['customer_number']
        self.customer_number = "user/{0}".format(req_number) if '@' in req_number else "sofia/internal/{0}@{1}".format(req_number,pbx_public_ip)
        self.record_name = "{0}.wav".format(uuid4())
        self.local_record_path = "{0}{1}".format(record_folder,self.record_name)
        self.record_path = "{0}{1}".format(record_prefix,self.record_name)
        self.call_at = time.time() * 1000
        self.outbound_number = request['display_number'] if "display_number" in request else outbound_number
        self.transfer = "TRANSFER_EXTENSION={0}".format(request["transfer_extension"]) if "transfer_extension" in request else ""

    def __str__(self):
        return "{{CALLBOT_MASTER_URI={0},CONVERSATION_ID={1},CALLBOT_CONTROLLER_URI={2},CALL_AT={3},execute_on_answer='lua::/opt/freeswitch/scripts/bot_answer.lua',execute_on_media='lua::/opt/freeswitch/scripts/bot_early_media.lua',record_name={6},local_record_path={5},record_path={7},origination_caller_id_number={8},detect_voicemail={10},batch_per_seconds={11},background_sound={12},{9}}}{4}".format(
            self.grpc_server,
            self.conversation_id,
            self.controller_url,
            self.call_at,
            self.customer_number,
            self.local_record_path,
            self.record_name,
            self.record_path,
            self.outbound_number,
            self.transfer,
            detect_voicemail,
            batch_per_seconds,
            background_sound)


class CallResponse():
    def __init__(self, conversation_id: str, call_at: int, pickup_at: int, hangup_at: int, hangup_cause: str) -> None:
        self.conversation_id = conversation_id
        self.call_at = call_at
        self.pickup_at = pickup_at
        self.hangup_at = hangup_at
        self.status = 105 if hangup_cause == "CRASH" else (103 if hangup_cause in USER_NO_RESPONSE_CAUSE else 104)
        self.sip_code = PbxHangupCause[hangup_cause].value if PbxHangupCause[hangup_cause] else 480

    def __str__(self):
        return json.dumps({
            "call_at": int(self.call_at),
            "pickup_at": int(self.pickup_at),
            "hangup_at": int(self.hangup_at),
            "conversation_id": self.conversation_id,
            "status": self.status,
            "sip_code": self.sip_code,
            "audio_url": None
        })

class RequestHandler(SimpleXMLRPCRequestHandler):
    rpc_paths = ('/RPC2',)

class SimpleThreadedXMLRPCServer(ThreadingMixIn, SimpleXMLRPCServer):
    pass

def sendToFreeswitchServer(server_uri: str, functionName: str, content: str):
    try:
        serv = xmlrpc.client.ServerProxy(server_uri)
        return serv.freeswitch.api(functionName, content)
    except:
        return "-ERR CRASH"

def sendEndCallToCallControllerServer(server_uri: str, content: str):
    serv = xmlrpc.client.ServerProxy(server_uri)
    return serv.phoneGatewayEndCall(content)

def startCall(json_request: str):
    call_request = CallRequest(json_request)
    logger.debug("startCall request: {}".format(json_request))
    logger.debug("startCall with url: {}".format(call_request))
    server_response = sendToFreeswitchServer("http://%s:%s@%s:%s" % (pbx_username, pbx_password, pbx_host, pbx_port),
                            "originate",
                            "{0} &start_call_with_bot".format(call_request))
    logger.debug("startCall server_response: {}".format(server_response))

    if "-ERR" in server_response:
        hangup_cause = server_response.strip().split("-ERR ")[1]
        call_response = CallResponse(call_request.conversation_id,call_request.call_at, 0, time.time() * 1000, hangup_cause)
        logger.debug(call_response.__str__())
        logger.debug(sendEndCallToCallControllerServer(call_request.controller_url, call_response.__str__()))
    # logger.debug("startCall done call!")

def run_server(host="0.0.0.0", port=9000):
    server_addr = (host, port)
    with SimpleThreadedXMLRPCServer(server_addr, requestHandler=RequestHandler) as server:
        server.register_introspection_functions()
        @server.register_function
        def callControllerServiceRequest(json_request: str):
            process = Process(target=startCall, args=[json_request])
            process.start()
            response = {
                    "status" : 0,
                    "msg" : "success"
                }

            return json.dumps(response)

        @server.register_function
        def phoneGatewayEndCall(json_response: str):
            logger.debug("phoneGatewayEndCall message: {}".format(json_response))
            response = json.loads(json_response)
            print("audio_url: {}, time: {}".format(response["audio_url"], (response["hangup_at"] - response["pickup_at"])/1000))
            return json.dumps({
                "status" : 0,
                "msg" : "success"
            })

        @server.register_function
        def voicemailDetectResult(json_str: str):
            logger.debug("voicemailDetectResult message: {}".format(json_str))
            request = json.loads(json_str)
            if request and request['is_voicemail'] is True:
                server_response = sendToFreeswitchServer("http://%s:%s@%s:%s" % (pbx_username, pbx_password, pbx_host, pbx_port),
                                "uuid_setvar",
                                "{0} IS_VOICE_MAIL true".format(request['conversion_id']))
                logger.debug("uuid_setvar {} response: {}".format(request['conversion_id'], server_response))
                server_response = sendToFreeswitchServer("http://%s:%s@%s:%s" % (pbx_username, pbx_password, pbx_host, pbx_port),
                                "uuid_kill",
                                "{0}".format(request['conversion_id']))
                logger.debug("uuid_kill {} response: {}".format(request['conversion_id'], server_response))
            return json.dumps({
                "status" : 0,
                "msg" : "success"
            })

        server.serve_forever()

if __name__ == '__main__':
    #eslThread = ESLThread(logger)
    #eslThread.start()
    run_server()
