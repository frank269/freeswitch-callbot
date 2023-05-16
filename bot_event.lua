local jsonString = event:getHeader('mod_call_bot::hangup_json') or '';
freeswitch.consoleLog("err", "event_hangup:" .. jsonString .. "\n")
if jsonString == '' then return; end

local jsonParser = require "resources.functions.lunajson"
local hangup_event = jsonParser.decode(jsonString)

--{"pickup_at":1684211935451,"call_at":1684203625280,"hangup_at":1684211936411,"conversation_id":"20230516043855-f57f1615-919f-44db-839b-3659273e29a6","bot_master_uri":"172.16.90.35:30066","phone_controller_uri":"http://172.16.90.35:30053/xmlrpc","is_bot_hangup":false,"is_bot_transfer":false,"sip_code":16,"hangup_cause":"NORMAL_CLEARING","record_path":"https://172.16.88.13/file/callbot/72d5bf64-1342-4428-b6fa-412f5b9bba98.wav","record_name":"72d5bf64-1342-4428-b6fa-412f5b9bba98.wav"}
-- Send hangup callback
local serverUrl = hangup_event.phone_controller_uri
local functionName = "phoneGatewayEndCall"
local jsonRequest = jsonString
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)
local curlCommand = string.format('curl -X POST -H "Content-Type: application/xml" -d \'%s\' %s', xmlPayload, serverUrl)
-- Execute the cURL command
local handle = io.popen(curlCommand)
local response = handle:read("*a")
handle:close()
-- Process the XML-RPC response
freeswitch.consoleLog("err", "phoneGatewayEndCall response: " .. response .. "\n")