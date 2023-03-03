from datetime import datetime
from esl_call_center import EslCallCenter
import threading
import time
import logging
import xmlrpc.client
import json
class ESLThread(threading.Thread):
    def __init__(self, logger):
        super().__init__()
        self.logger = logger
        self.freeswitch = EslCallCenter()
        self.__run_flag = True
        self.connect()

    def connect(self):
        self.con = self.freeswitch.connect()
        if not self.con.connected():
            self.logger.error("ESLThread ERROR: connection failed!")
            return

        # listen events
        self.con.events("PLAIN", "CUSTOM mod_call_bot::bot_hangup")
        # self.con.events("PLAIN", "DTMF")
        # self.con.events("PLAIN", "CHANNEL_CREATE")
        # self.con.events("PLAIN", "CHANNEL_PROGRESS_MEDIA")
        # self.con.events("PLAIN", "CHANNEL_ANSWER")
        # self.con.events("PLAIN", "CHANNEL_HANGUP")
        # self.con.events('PLAIN', 'CUSTOM sofia::register')
        # self.con.events('PLAIN', 'CUSTOM sofia::unregister')

    def run(self):
        self.logger.info("ESLThread is running...")
        while self.__run_flag:
            try:
                if self.con is not None and self.con.connected():
                    e = self.con.recvEventTimed(1000)
                    if e:
                        event = e.serialize(ser_format="custom")
                        self.process_event(event['mod_call_bot::hangup_json'])
                        # if event['Event-Name'] == 'CHANNEL_CREATE':
                        #     self.handler.CallCreateHandle(event)
                        # elif event['Event-Name'] == 'CHANNEL_PROGRESS_MEDIA':
                        #     self.handler.CallProcessMediaHandle(event)
                        # elif event['Event-Name'] == 'CHANNEL_ANSWER':
                        #     self.handler.CallAnsweredHandle(event)
                        # elif event['Event-Name'] == 'CHANNEL_HANGUP':
                        #     self.handler.CallHangUpHandle(event)
                        # elif event['Event-Name'] == 'CUSTOM' and 'Event-Subclass' in event and event['Event-Subclass'] == 'sofia::register':
                        #     self.handler.OnUserRegisted(event)
                        # elif event['Event-Name'] == 'CUSTOM' and 'Event-Subclass' in event and event['Event-Subclass'] == 'sofia::unregister':
                        #     self.handler.OnUserUnregisted(event)
                        # elif event['Event-Name'] == 'CUSTOM':
                        #     self.handler.CallCustomHandle(event)
                        # elif event['Event-Name'] == 'DTMF':
                        #     self.handler.CallDTMFHandle(event)
                else:
                    time.sleep(1)
                    self.connect()
            except Exception as e:
                self.logger.error("ESL Socket error ... {0}".format(e))
                time.sleep(1)
                self.connect()

    def stop(self):
        self.__run_flag = False
        self.con.disconnect()
        self.logger.info("esl thread is stopped!")

    def process_event(self, json_string):
        self.logger.debug("received event: {0}".format(json_string))
        event = json.loads(json_string)
        server = xmlrpc.client.ServerProxy(event["phone_controller_uri"])
        request = {
            "call_at" : event["call_at"],
            "pickup_at" : event["pickup_at"],
            "hangup_at" :event["hangup_at"],
            "conversation_id" : event["conversation_id"],
            "status" : 102 if event["is_bot_transfer"] else (100 if event["is_bot_hangup"] else 101),
            "sip_code" : event["sip_code"],
        }
        self.logger.debug("call phoneGatewayEndCall response: {0}".format(server.phoneGatewayEndCall(json.dumps(request))))