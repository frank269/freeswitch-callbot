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
local is_voice_mail = event:getHeader('variable_IS_VOICE_MAIL') or 'false';
-- local sip_code = event:getHeader('variable_hangup_cause_q850') or '16';
local hangup_cause = event:getHeader('Hangup-Cause') or '';
local record_name = event:getHeader('variable_record_name') or '';
local audio_url = event:getHeader('variable_record_path') or '';

if conversation_id == '' then return; end

local function hangupCauseToSipcode(hangup_cause)
    if hangup_cause == 'UNALLOCATED_NUMBER' or hangup_cause == 'NO_ROUTE_TRANSIT_NET' or hangup_cause == 'NO_ROUTE_DESTINATION' or hangup_cause == 'NUMBER_CHANGED' then
        return 404
    elseif hangup_cause == 'USER_BUSY' then
        return 486
    elseif hangup_cause == 'NO_USER_RESPONSE' then
        return 408
    elseif hangup_cause == 'NORMAL_UNSPECIFIED' or hangup_cause == 'NO_ANSWER' or hangup_cause == 'SUBSCRIBER_ABSENT' then
        return 480
    elseif hangup_cause == 'CALL_REJECTED' or hangup_cause == 'DECLINE' then
        return 603
    elseif hangup_cause == 'REDIRECTION_TO_NEW_DESTINATION' then
        return 410
    elseif hangup_cause == 'DESTINATION_OUT_OF_ORDER' or hangup_cause == 'INVALID_PROFILE' then
        return 502
    elseif hangup_cause == 'INVALID_NUMBER_FORMAT' or hangup_cause == 'INVALID_URL' or hangup_cause == 'INVALID_GATEWAY' then
        return 484
    elseif hangup_cause == 'FACILITY_REJECTED' or hangup_cause == 'FACILITY_NOT_IMPLEMENTED' or hangup_cause == 'SERVICE_NOT_IMPLEMENTED' then
        return 501
    elseif hangup_cause == 'REQUESTED_CHAN_UNAVAIL' or hangup_cause == 'NORMAL_CIRCUIT_CONGESTION' or hangup_cause == 'NETWORK_OUT_OF_ORDER' or hangup_cause == 'NORMAL_TEMPORARY_FAILURE' or hangup_cause == 'SWITCH_CONGESTION' or hangup_cause == 'GATEWAY_DOWN' or hangup_cause == 'BEARERCAPABILITY_NOTAVAIL' then
        return 503
    elseif hangup_cause == 'OUTGOING_CALL_BARRED' or hangup_cause == 'INCOMING_CALL_BARRED' or hangup_cause == 'BEARERCAPABILITY_NOTAUTH' then
        return 403
    elseif hangup_cause == 'BEARERCAPABILITY_NOTIMPL' or hangup_cause == 'INCOMPATIBLE_DESTINATION' then
        return 488
    elseif hangup_cause == 'RECOVERY_ON_TIMER_EXPIRE' then
        return 504
    elseif hangup_cause == 'CAUSE_ORIGINATOR_CANCEL' then
        return 487
    elseif hangup_cause == 'EXCHANGE_ROUTING_ERROR' then
        return 483
    elseif hangup_cause == 'BUSY_EVERYWHERE' then
        return 600
    elseif hangup_cause == 'DOES_NOT_EXIST_ANYWHERE' then
        return 604
    elseif hangup_cause == 'NOT_ACCEPTABLE' then
        return 606
    elseif hangup_cause == 'UNWANTED' then
        return 607
    elseif hangup_cause == 'NO_IDENTITY' then
        return 428
    elseif hangup_cause == 'BAD_IDENTITY_INFO' then
        return 429
    elseif hangup_cause == 'UNSUPPORTED_CERTIFICATE' then
        return 437
    elseif hangup_cause == 'INVALID_IDENTITY' then
        return 438
    elseif hangup_cause == 'STALE_DATE' then
        return 439
    end
    return 480
end

local sip_code = hangupCauseToSipcode(hangup_cause)

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
-- freeswitch.consoleLog("info", "callbot hangup request: " .. jsonRequest  .. "\n")
-- Send hangup callback
local functionName = "phoneGatewayEndCall"
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)

local api = freeswitch.API();
local response = api:executeString("curl ".. phone_controller_uri .. " timeout 3 content-type 'application/xml' post '"..xmlPayload.."'") or '';

-- local curlCommand = string.format('curl -X POST -H "Content-Type: application/xml" -d \'%s\' %s', xmlPayload, phone_controller_uri)
-- Execute the cURL command
-- local handle = io.popen(curlCommand)
-- freeswitch.consoleLog("info", "callbot phoneGatewayEndCall request: " .. conversation_id .. " - " .. jsonRequest .. "\n")
-- local response = handle:read("*a")
-- handle:close()
-- Process the XML-RPC response
freeswitch.consoleLog("info", "callbot phoneGatewayEndCall response: " .. conversation_id .. " - " .. response .. "\n")

jsonParser = null;
event = null;
jsonRequest = null;
functionName = null;
xmlPayload = null;
api = null;
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
-- local sip_code = null;
hangup_cause = null;
record_name = null;
audio_url = null;