local function sendEndCall(session)
    local status = 102; -- bot transfer
    local sip_code = '480';
    local hangup_cause = "NORMAL_CLEARING"
    local conversation_id = session:getVariable('CONVERSATION_ID') or '';
    local bot_master_uri = session:getVariable('CALLBOT_MASTER_URI') or '';
    local phone_controller_uri = session:getVariable('CALLBOT_CONTROLLER_URI') or '';
    local record_name = session:getVariable('record_name') or '';
    local audio_url = session:getVariable('record_path') or '';
    local call_at = session:getVariable('CALL_AT') or '0';
    local pickup_at = session:getVariable('PICKUP_AT') or '0';

    local currentTimeSeconds = os.time()
    local currentClockTimeSeconds = os.clock()
    local milliseconds = (currentTimeSeconds * 1000) + ((currentClockTimeSeconds - math.floor(currentClockTimeSeconds)) * 1000)
    local hangup_at = '' .. milliseconds;

    local jsonParser = require "resources.functions.lunajson"
    local event = {
        call_at = math.floor(tonumber(call_at)),
        pickup_at = math.floor(tonumber(pickup_at)),
        hangup_at = math.floor(tonumber(hangup_at)),
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
    -- Send hangup callback
    local functionName = "phoneGatewayEndCall"
    local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>',>

    freeswitch.consoleLog("info", "callbot phoneGatewayEndCall request: " .. conversation_id .. " - " .. jsonRequest .. "\n")
    local api = freeswitch.API();
    local response = api:executeString("curl "..phone_controller_uri .. " timeout 3 content-type 'application/xml' post '"..xmlPayload.."'") or '';
    freeswitch.consoleLog("info", "callbot phoneGatewayEndCall response: " .. conversation_id .. " - " .. response .. "\n")
end
    

local jsonString = event:getHeader('mod_call_bot::transfer_json') or '';
local sessionUuid = event:getHeader('mod_call_bot::session_id') or '';
freeswitch.consoleLog("info", "callbot event_transfer: " .. jsonString ..  ", sessionId: " .. sessionUuid .. "\n")
if jsonString == '' then return; end

-- {"customer_number":"0328552966","display_number":"842488898168","forward_type":2}
-- {"display_number":"842488898168","forward_type":1,"sip_url":"sip:999999@callbot.metechvn.com"}
local jsonParser = require "resources.functions.lunajson"
local transfer_data = jsonParser.decode(jsonString)

local session = freeswitch.Session(sessionUuid)


-- local local_record_path = session:getVariable("local_record_path") or ''
-- if local_record_path ~= '' then
--    session:execute("stop_record_session", local_record_path)
-- end
sendEndCall(session)

-- session:execute("stop_displace_session","/city.mp3")
--session:setAutoHangup(false)
if transfer_data.forward_type == 1 then
    local extension = 999999;
    local context = "callbot.metechvn.com"
    if string.find(transfer_data.sip_url, "sip:") and string.find(transfer_data.sip_url, "@") then
        extension, context = string.match(transfer_data.sip_url, "sip:(.-)@(.+)")
    end
    session:transfer(extension,"xml",context);
elseif transfer_data.forward_type == 2 then
    session:setVariable("origination_caller_id_number", transfer_data.display_number)
--    session:setVariable("effective_caller_id_number", transfer_data.display_number)
    ---session:execute("bridge", "sofia/internal/" .. transfer_data.customer_number .. "@103.63.117.76:5090")
    session:transfer(transfer_data.customer_number, "XML", "callbot.metechvn.com")
   -- new_session = freeswitch.Session("sofia/gateway/c11144a9-df60-41f0-951e-3047924ee1e4/" .. transfer_data.customer_number, session);
end
