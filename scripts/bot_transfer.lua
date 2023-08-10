local jsonString = event:getHeader('mod_call_bot::transfer_json') or '';
local sessionUuid = event:getHeader('mod_call_bot::session_id') or '';
freeswitch.consoleLog("info", "callbot event_transfer: " .. jsonString ..  ", sessionId: " .. sessionUuid .. "\n")
if jsonString == '' then return; end

-- {"customer_number":"0328552966","display_number":"842488898168","forward_type":2}
-- {"display_number":"842488898168","forward_type":1,"sip_url":"sip:999999@callbot.metechvn.com"}
local jsonParser = require "resources.functions.lunajson"
local transfer_data = jsonParser.decode(jsonString)

local session = freeswitch.Session(sessionUuid)
session:execute("stop_displace_session","/city.mp3")
--session:setAutoHangup(false)
if transfer_data.forward_type == 1 then
    if string.find(transfer_data.sip_url, "sip:") and string.find(transfer_data.sip_url, "@") then
        local extension, context = string.match(transfer_data.sip_url, "sip:(.-)@(.+)")
        session:transfer(extension, "XML", context)
    else
        session:transfer("999999", "XML", "callbot.metechvn.com")
    end
elseif transfer_data.forward_type == 2 then
    session:setVariable("origination_caller_id_number", transfer_data.display_number)
--    session:setVariable("effective_caller_id_number", transfer_data.display_number)
    ---session:execute("bridge", "sofia/internal/" .. transfer_data.customer_number .. "@103.63.117.76:5090")
    session:transfer(transfer_data.customer_number, "XML", "callbot.metechvn.com")
   -- new_session = freeswitch.Session("sofia/gateway/c11144a9-df60-41f0-951e-3047924ee1e4/" .. transfer_data.customer_number, session);
end
