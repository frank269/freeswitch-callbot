from ESL import *
import os


class EslCallCenter():

    def __init__(self, logger):
        self.log = logger
        self.host = os.getenv('ESL_HOST', "172.16.88.38")
        self.port = os.getenv('ESL_PORT', "8021")
        self.password = os.getenv('ESL_PASSWORD', "ClueCon")

    def connect(self):
        try:
            return ESLconnection(self.host, self.port, self.password)
        except Exception as error:
            self.log.error(error)
            return None

    def makeSureConnected(self):
        if self.conn is None or self.conn.connected() == 0:
            self.conn = self.connect()

        if self.conn is None:
            self.log.error('EslCallCenter connect to ESL failed!')
            return False
        else:
            return self.conn.connected() == 1

    def sendCommand(self, command: str):
        # if not self.makeSureConnected():
        #     return ''
        try:
            conn = self.connect()
            body = conn.api(command).getBody()
            conn.disconnect()
            return body
        except Exception as error:
            self.log.error(error)
            return ''

    def sendLuaCommand(self, command: str):
        # if not self.makeSureConnected():
        #     return ''
        try:
            conn = self.connect()
            xml = conn.bgapi(command).serialize('xml')
            conn.disconnect()
            return xml
        except Exception as error:
            self.log.error(error)
            return ''
