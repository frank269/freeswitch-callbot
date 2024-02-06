if (session == nil) then
  return
end

freeswitch.consoleLog("info", "Starting callbot init ... \n")
local destination_number = session:getVariable("destination_number");
--local caller_id_name = session:getVariable("caller_id_name");
local caller_id_number = session:getVariable("caller_id_number");
--local callbot_init_server = session:getVariable("callbot_init_server");
freeswitch.consoleLog("info", "destination_number: " .. destination_number .. "\n")
--freeswitch.consoleLog("err", "caller_id_name:" .. caller_id_name .. "\n")
freeswitch.consoleLog("info", "caller_id_number: " .. caller_id_number .. "\n")
--freeswitch.consoleLog("err", "callbot_init_server:" .. callbot_init_server .. "\n")

-- Get the current time in seconds
local currentTimeSeconds = os.time()
-- Get the current clock time in seconds
local currentClockTimeSeconds = os.clock()
-- Calculate the milliseconds by subtracting the integer part of the clock time from the current time
local milliseconds = (currentTimeSeconds * 1000) + ((currentClockTimeSeconds - math.floor(currentClockTimeSeconds)) * 1000)
freeswitch.consoleLog("info", "current time:" .. milliseconds  .. "\n")

local random = math.random
local function uuid()
    local template ='xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'
    return string.gsub(template, '[xy]', function (c)
        local v = (c == 'x') and random(0, 0xf) or random(8, 0xb)
        return string.format('%x', v)
    end)
end

-- Function to extract value from XML
local function extractValueFromXml(xmlString, elementName)
    local startTag = "<" .. elementName .. ">"
    local endTag = "</" .. elementName .. ">"
    local startIndex, endIndex = xmlString:find(startTag .. "(.-)" .. endTag)
    if startIndex and endIndex then
        local value = xmlString:sub(startIndex + #startTag, endIndex - #endTag)
        return value
    end
    return nil
end

function file_exists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end 
end

local serverUrl =  os.getenv("BOT_INIT_URL") or "http://172.16.90.35:30053/xmlrpc"
local public_ip = os.getenv("PUBLIC_IP") or '172.16.88.13'
local record_folder = "/var/lib/freeswitch/recordings/callbot/"
local record_prefix = "https://"..public_ip.."/file/callbot/"
local record_name = os.date('%Y/%m/%d/%H/') .. uuid() .. ".wav"
local local_record_path = record_folder .. record_name
local record_path = record_prefix .. record_name

local is_bot_transfered = session:getVariable("IS_BOT_TRANSFERED") or ''
if is_bot_transfered == 'true' then
 session:execute("record_session", local_record_path)
end

local functionName = "init_callin"
local jsonRequest = string.format('{"callcenter_phone": "%s","customer_phone":"%s"}',destination_number,caller_id_number)
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)

local api = freeswitch.API();
local response = api:executeString("curl ".. serverUrl .. " timeout 3 content-type 'application/xml' post '"..xmlPayload.."'");

-- Process the XML-RPC response
freeswitch.consoleLog("err", "callbot init response: " .. response .. "\n")
-- Extract the desired value from the XML response
local jsonString = extractValueFromXml(response, "string")
-- freeswitch.consoleLog("info", "callbot init value: " .. jsonString .. "\n")

local jsonParser = require "resources.functions.lunajson"
local callbot_info = jsonParser.decode(jsonString)
freeswitch.consoleLog("info", "conversation_id: " .. callbot_info.conversation_id .. "\n")
freeswitch.consoleLog("info", "grpc_server: " .. callbot_info.grpc_server .. "\n")
freeswitch.consoleLog("info", "controller_url: " .. callbot_info.controller_url .. "\n")
--freeswitch.consoleLog("err", "input_slots:" .. callbot_info.input_slots .. "\n")
freeswitch.consoleLog("info", "record_name: " .. record_name .. "\n")
freeswitch.consoleLog("info", "local_record_path: " .. local_record_path .. "\n")
freeswitch.consoleLog("info", "record_path: " .. record_path .. "\n")

session:setVariable("CALLBOT_MASTER_URI", callbot_info.grpc_server)
session:setVariable("CONVERSATION_ID", callbot_info.conversation_id)
session:setVariable("CALLBOT_CONTROLLER_URI", callbot_info.controller_url)
session:setVariable("CALL_AT", milliseconds)
session:setVariable("PICKUP_AT", milliseconds)

session:setVariable("execute_on_answer", "record_session::" .. local_record_path)

session:setVariable("record_name", record_name)
session:setVariable("record_path", record_path)
session:setVariable("ignore_early_media", "true")

session:setVariable("local_record_path", local_record_path)
session:setVariable("IS_BOT_HANGUP", "")
session:setVariable("IS_BOT_TRANSFERED", "")
session:setVariable("IS_BOT_ERROR", "")
session:setVariable("IS_PLAYING", "")

freeswitch.consoleLog("info", "callbot init done! starting bot ...\n")

session:answer()
session:setVariable("START_BOT", "true")

local background_sound = session:getVariable("background_sound") or '';
if background_sound ~= '' and file_exists(background_sound) then
  freeswitch.consoleLog("info", session:get_uuid() .. " play background sound: " .. background_sound);
  session:execute("displace_session",background_sound .. " mlf")
end
