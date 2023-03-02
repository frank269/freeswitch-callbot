from datetime import datetime
from esl_call_center import EslCallCenter
import threading
import time
import logging


class ESLThread(threading.Thread):
    def __init__(self):
        super().__init__()
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)
        self.freeswitch = EslCallCenter()
        self.__run_flag = True
        self.connect()

    def connect(self):
        self.con = self.freeswitch.connect()
        if not self.con.connected():
            self.logger.error("ESLThread ERROR: connection failed!")
            return

        # listen events
        self.con.events("PLAIN", "custom")
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
                        print(event)
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


# class CallHandler():
#     def __init__(self, kafka_thread: KafkaThread):
#         self.kafka_thread: KafkaThread = kafka_thread
#         self.logger = logging.getLogger(__name__)
#         self.logger.setLevel(logging.INFO)

#     def CallCreateHandle(self, event):
#         if 'variable_sip_h_X-Call-Id' in event and 'Channel-Name' in event and 'sofia/external' in event['Channel-Name']:
#             active_call: AutoCallCdrRequest = AutoCallCdrRequest(
#                 phoneState=PhoneState.CREATE.value,
#                 call_id=event['variable_sip_h_X-Call-Id'],
#                 answer_state=event['Answer-State'],
#                 channel_call_state=event['Channel-Call-State'],
#                 called=event['Caller-Destination-Number'],
#                 caller=event['Caller-ANI'],
#                 caller_context=event['Caller-Context'],
#                 created_time=int(event['Caller-Channel-Created-Time'])/1000000,
#                 created_date=int(time.time()),
#                 direction=event['Caller-Direction'],
#             )
#             if 'variable_sip_h_X-Campaign-Id' in event:
#                 active_call.campaign_uuid = event['variable_sip_h_X-Campaign-Id']
#             if 'variable_sip_h_X-Campaign-Info' in event:
#                 call_info = str(
#                     event['variable_sip_h_X-Campaign-Info']).split('|')
#                 if len(call_info) > 4:
#                     active_call.stt = call_info[0]
#                     active_call.customer_id = call_info[1]
#                     active_call.group_id = call_info[2]
#                     active_call.group_name = call_info[3]
#                     active_call.caller_name = call_info[4]
#             self.kafka_thread.addToQueue(active_call)

#     def CallProcessMediaHandle(self, event):
#         if 'variable_sip_h_X-Call-Id' in event and 'Channel-Name' in event and 'sofia/external' in event['Channel-Name']:
#             active_call: AutoCallCdrRequest = AutoCallCdrRequest(
#                 call_id=event['variable_sip_h_X-Call-Id'],
#                 answer_state=event['Answer-State'],
#                 phoneState=PhoneState.PROGRESS_MEDIA.value,
#                 direction=event['Call-Direction'],
#                 progress_media_time=int(
#                     event['Caller-Channel-Progress-Media-Time'])/1000000
#             )
#             self.kafka_thread.addToQueue(active_call)

#     def CallAnsweredHandle(self, event):
#         if 'variable_sip_h_X-Call-Id' in event and 'Channel-Name' in event and 'sofia/external' in event['Channel-Name']:
#             active_call: AutoCallCdrRequest = AutoCallCdrRequest(
#                 call_id=event['variable_sip_h_X-Call-Id'],
#                 phoneState=PhoneState.ANSWERED.value,
#                 answer_state=event['Answer-State'],
#                 answered_time=int(
#                     event['Caller-Channel-Answered-Time'])/1000000,
#                 bridged_time=int(
#                     event['Caller-Channel-Bridged-Time'])/1000000
#             )
#             self.kafka_thread.addToQueue(active_call)

#     def CallHangUpHandle(self, event):
#         # and 'Channel-Name' in event and 'sofia/external' in event['Channel-Name']:
#         if 'variable_sip_h_X-Call-Id' in event:
#             active_call: AutoCallCdrRequest = AutoCallCdrRequest(
#                 call_id=event['variable_sip_h_X-Call-Id'],
#                 phoneState=PhoneState.HANGUP.value,
#                 answer_state=event['Answer-State'],
#                 created_time=int(event['Caller-Channel-Created-Time'])/1000000,
#                 answered_time=int(
#                     event['Caller-Channel-Answered-Time'])/1000000,
#                 progress_media_time=int(
#                     event['Caller-Channel-Progress-Media-Time'])/1000000,
#                 hangup_cause=event['Hangup-Cause'],
#                 hangup_time=int(time.time())
#             )
#             self.kafka_thread.addToQueue(active_call)

#     def CallCustomHandle(self, event):
#         active_call: AutoCallCdrRequest = AutoCallCdrRequest(
#             call_id=event['X-Call-Id'],
#             phoneState=PhoneState.DTMF.value,
#             dtmf=event['X-Valid-DTMF'],
#             dtmf_time=int(time.time())
#         )
#         self.kafka_thread.addToQueue(active_call)

#     def CallDTMFHandle(self, event):
#         self.logger.info(event)

#     def OnUserRegisted(self, event):
#         self.logger.info(event)

#     def OnUserUnregisted(self, event):
#         self.logger.info(event)


eslThread = ESLThread()
eslThread.start()