freeswitch.consoleLog("debug", "Starting callbot init ... \n")
local destination_number = session:getVariable("destination_number");
--local caller_id_name = session:getVariable("caller_id_name");
local caller_id_number = session:getVariable("caller_id_number");
--local callbot_init_server = session:getVariable("callbot_init_server");
freeswitch.consoleLog("debug", "destination_number: " .. destination_number .. "\n")
--freeswitch.consoleLog("err", "caller_id_name:" .. caller_id_name .. "\n")
freeswitch.consoleLog("debug", "caller_id_number: " .. caller_id_number .. "\n")
--freeswitch.consoleLog("err", "callbot_init_server:" .. callbot_init_server .. "\n")

freeswitch.consoleLog("debug", "current time:" .. os.time() .. "\n")

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

local serverUrl = "http://172.16.90.35:30053/xmlrpc"
local record_folder = "/var/lib/freeswitch/recordings/callbot/"
local record_prefix = "https://172.16.88.13/file/callbot/"
local record_name = uuid() .. ".wav"
local local_record_path = record_folder .. record_name
local record_path = record_prefix .. record_name

local functionName = "init_callin"
local jsonRequest = string.format('{"callcenter_phone": "%s","customer_phone":"%s"}',destination_number,caller_id_number)
local xmlPayload = string.format('<?xml version="1.0"?>\n<methodCall>\n<methodName>%s</methodName>\n<params>\n<param>\n<value>\n<string>%s</string>\n</value>\n</param>\n</params>\n</methodCall>', functionName, jsonRequest)
local curlCommand = string.format('curl -X POST -H "Content-Type: application/xml" -d \'%s\' %s', xmlPayload, serverUrl)
-- Execute the cURL command
local handle = io.popen(curlCommand)
local response = handle:read("*a")
handle:close()
-- Process the XML-RPC response
freeswitch.consoleLog("debug", "callbot init response: " .. response .. "\n")
-- Extract the desired value from the XML response
local jsonString = extractValueFromXml(response, "string")
-- freeswitch.consoleLog("debug", "callbot init value: " .. jsonString .. "\n")

local jsonParser = require "resources.functions.lunajson"
local callbot_info = jsonParser.decode(jsonString)
freeswitch.consoleLog("debug", "conversation_id: " .. callbot_info.conversation_id .. "\n")
freeswitch.consoleLog("debug", "grpc_server: " .. callbot_info.grpc_server .. "\n")
freeswitch.consoleLog("debug", "controller_url: " .. callbot_info.controller_url .. "\n")
--freeswitch.consoleLog("err", "input_slots:" .. callbot_info.input_slots .. "\n")
freeswitch.consoleLog("debug", "record_name: " .. record_name .. "\n")
freeswitch.consoleLog("debug", "local_record_path: " .. local_record_path .. "\n")
freeswitch.consoleLog("debug", "record_path: " .. record_path .. "\n")

session:setVariable("CALLBOT_MASTER_URI", callbot_info.grpc_server)
session:setVariable("CONVERSATION_ID", callbot_info.conversation_id)
session:setVariable("CALLBOT_CONTROLLER_URI", callbot_info.controller_url)
session:setVariable("CALL_AT", "1684203625280")
session:setVariable("execute_on_answer", "record_session::" .. local_record_path)
session:setVariable("record_name", record_name)
session:setVariable("record_path", record_path)
session:setVariable("ignore_early_media", "true")

freeswitch.consoleLog("debug", "callbot init done! starting bot ...\n")