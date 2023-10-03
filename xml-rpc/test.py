import xmlrpc.client
import time

GATEWAY_ID = "d57ce3c7-f8ac-4f66-affc-f22ca0b442d5" # to pbx
# GATEWAY_ID = "24339c15-c3c2-4fa6-b3ef-e705a5f896ae" # to kamailio
DESTINATION = "11111"
MAX_CCU = 350
SLEEP_IN_SEC = 1
PHONE_NUMBER = 979000000

RPC_HOST = "172.16.88.12"
RPC_PORT = "8080"
RPC_USERNAME = "freeswitch"
RPC_PASSWORD = "works"
SERVER_URL = "http://%s:%s@%s:%s" % (RPC_USERNAME, RPC_PASSWORD, RPC_HOST, RPC_PORT)

def sendToFreeswitchServer( functionName: str, content: str):
    try:
        serv = xmlrpc.client.ServerProxy(SERVER_URL)
        return serv.freeswitch.api(functionName, content)
    except:
        return "-ERR CRASH"

def getTotalCall():
    return int(sendToFreeswitchServer("show","calls count").split(" ")[0])

def makeCalls(num_calls: int, start_phone):
    err = 0
    for i in range(num_calls):
        response = sendToFreeswitchServer("originate",
                                "{{origination_caller_id_number=0{0}}}sofia/gateway/{1}/{2} &playback('local_stream://foo')".format(start_phone + i, GATEWAY_ID, DESTINATION))
        if "-ERR" in response:
            # print(response)
            err += 1
    return err

if __name__ == '__main__':
    start_phone = PHONE_NUMBER
    current_count = 0
    while(True):
        current_count = getTotalCall()
        print("current active calls: %d" % current_count)
        if current_count < MAX_CCU:
            call_errors = makeCalls(MAX_CCU - current_count, start_phone)
            start_phone += MAX_CCU - current_count
            if call_errors > 0:
                print("make call error: %d" % call_errors)
        time.sleep(SLEEP_IN_SEC)