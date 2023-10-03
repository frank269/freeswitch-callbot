local channel_uuid = event:getHeader('Unique-ID') or '';
local call_at = event:getHeader('variable_CALL_AT') or '0';
--local pickup_at = event:getHeader('Caller-Channel-Answered-Time') or '0';
local pickup_at = event:getHeader('variable_PICKUP_AT') or '0';
local hangup_at = event:getHeader('Caller-Channel-Hangup-Time') or '0';
local conversation_id = event:getHeader('variable_CONVERSATION_ID') or '';
local bot_master_uri = event:getHeader('variable_CALLBOT_MASTER_URI') or '';
local phone_controller_uri = event:getHeader('variable_CALLBOT_CONTROLLER_URI') or '';
local is_bot_hangup = event:getHeader('variable_IS_BOT_HANGUP') or 'false';
local is_bot_transfer = event:getHeader('variable_IS_BOT_TRANSFERED') or 'false';
local is_bot_error = event:getHeader('variable_IS_BOT_ERROR') or 'false';
local is_voice_mail = event:getHeader('variable_IS_VOICE_MAIL') or 'false';
local sip_code = event:getHeader('variable_hangup_cause_q850') or '16';
local hangup_cause = event:getHeader('Hangup-Cause') or '';
local record_name = event:getHeader('variable_record_name') or '';
local audio_url = event:getHeader('variable_record_path') or '';

if conversation_id == '' then return; end

local status = 101;
if is_voice_mail == 'true' then
    status = 103;
    sip_code = '600';
    hangup_cause = 'VOICEMAIL_DETECTED'
elseif is_bot_transfer == 'true' then
    status = 102;
elseif is_bot_error == 'true' then
    status = 105;
elseif is_bot_hangup == 'true' then
    status = 100;
elseif pickup_at == '0' then
    status = 103;
end

local jsonParser = require "resources.functions.lunajson"
local event = {
    call_at = math.floor(tonumber(call_at)),
    pickup_at = math.floor(tonumber(pickup_at)),
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
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall request: " .. conversation_id .. " - " .. jsonRequest .. "\n")
local functionName = "phoneGatewayEndCall"
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)
local api = freeswitch.API();
local response = api:executeString("curl "..phone_controller_uri .. " json timeout 3 content-type 'application/xml' post '"..xmlPayload.."'") or '';
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall response: " .. conversation_id .. " - " .. response .. "\n")


jsonParser = null;
event = null;
jsonRequest = null;
functionName = null;
xmlPayload = null;
response = null;
status = null;
sip_code = null;
channel_uuid = null;
call_at = null;
pickup_at = null;
hangup_at = null;
conversation_id = null;
bot_master_uri = null;
phone_controller_uri = null;
is_bot_hangup = null;
is_bot_transfer = null;
is_bot_error = null;
is_voice_mail = null;
hangup_cause = null;
record_name = null;
audio_url = null;
