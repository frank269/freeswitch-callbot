local jsonString = event:getHeader('mod_call_bot::hangup_json') or '';
freeswitch.consoleLog("info", "callbot event_hangup:" .. jsonString .. "\n")
if jsonString == '' then return; end

local jsonParser = require "resources.functions.lunajson"
local hangup_event = jsonParser.decode(jsonString)


local sessionUuid = hangup_event.session_id;

freeswitch.consoleLog("err", "sessionId: " .. sessionUuid .. "\n")

local session = freeswitch.Session(sessionUuid)

if (session == nil) then
  freeswitch.consoleLog("err", "sessionId is gone\n")
  return
end

--local hangup_time = session:getVariable("hangup_cause")
--freeswitch.consoleLog("err", "hangup_time: " .. hangup_time .. "\n")


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
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall response: " .. response .. "\n")
