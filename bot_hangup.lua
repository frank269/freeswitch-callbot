local channel_uuid = event:getHeader('Unique-ID') or '';
local call_at = event:getHeader('variable_CALL_AT') or '0';
local pickup_at = event:getHeader('Caller-Channel-Answered-Time') or '0';
local hangup_at = event:getHeader('Caller-Channel-Hangup-Time') or '0';
local conversation_id = event:getHeader('variable_CONVERSATION_ID') or '';
local bot_master_uri = event:getHeader('variable_CALLBOT_MASTER_URI') or '';
local phone_controller_uri = event:getHeader('variable_CALLBOT_CONTROLLER_URI') or '';
local is_bot_hangup = event:getHeader('variable_IS_BOT_HANGUP') or 'false';
local is_bot_transfer = event:getHeader('variable_IS_BOT_TRANSFERED') or 'false';
local is_bot_error = event:getHeader('variable_IS_BOT_ERROR') or 'false';
local sip_code = event:getHeader('variable_hangup_cause_q850') or '16';
local hangup_cause = event:getHeader('Hangup-Cause') or '';
local record_name = event:getHeader('variable_record_name') or '';
local audio_url = event:getHeader('variable_record_path') or '';

if conversation_id == '' then return; end

local status = 101;
if is_bot_transfer == 'true' then
    status = 102;
elseif is_bot_error == 'true' then
    status = 105;
elseif is_bot_hangup == 'true' then
    status = 100;
end

local jsonParser = require "resources.functions.lunajson"
local event = {
    call_at = math.floor(tonumber(call_at)),
    pickup_at = math.floor(tonumber(pickup_at)/1000),
    hangup_at = math.floor(tonumber(hangup_at)/1000),
    conversation_id = conversation_id,
    bot_master_uri = bot_master_uri,
    phone_controller_uri = phone_controller_uri,
    sip_code = tonumber(sip_code),
    hangup_cause = hangup_cause,
    record_name = record_name,
    audio_url = audio_url,
    status = status
}

local jsonRequest = jsonParser.encode(event);
freeswitch.consoleLog("info", "callbot hangup request: " .. jsonRequest  .. "\n")
-- Send hangup callback
local functionName = "phoneGatewayEndCall"
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)
local curlCommand = string.format('curl -X POST -H "Content-Type: application/xml" -d \'%s\' %s', xmlPayload, phone_controller_uri)
-- Execute the cURL command
local handle = io.popen(curlCommand)
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall request: " .. curlCommand .. "\n")
local response = handle:read("*a")
handle:close()
-- Process the XML-RPC response
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall response: " .. response .. "\n")
