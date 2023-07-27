local uuid = session:getVariable('uuid') or '';

if uuid == '' then return; end

local conversation_id = session:getVariable("CONVERSATION_ID") or '';
local server_url = "http://172.16.90.35:30008/voicemail/v1/submit_task"
local auth_token = "iaKglGRsR8wlzx5hEcJJ3U2GlgaaMIqA"
local callback_url = "http://172.16.90.41:9000"
local record_path = "/var/lib/freeswitch/recordings/record_on_media/" .. uuid .. ".wav";
local public_path = "https://callbot-audio.metechvn.com/file/record_on_media/" .. uuid .. ".wav";

session:sleep(500);
freeswitch.consoleLog("info", "action on answer phase, stop recording... !, record path: " .. record_path .. " \n")
session:execute("stop_record_session", record_path)

freeswitch.consoleLog("info", uuid .. " send api to detect voice mail!")

local jsonRequest = string.format('{"token": "%s","conversion_id":"%s","callback_url":"%s","audio_url":"%s"}',auth_token,uuid,callback_url,public_path)
local curlCommand = string.format('curl -X POST -H "Content-Type: application/json" -d \'%s\' %s', jsonRequest, server_url)
freeswitch.consoleLog("info", uuid .. " detect voice mail response: " .. jsonRequest .. "\n")
-- Execute the cURL command
local handle = io.popen(curlCommand)
local response = handle:read("*a")
handle:close()
-- Process the XML-RPC response
freeswitch.consoleLog("info", uuid .. " detect voice mail response: " .. response .. "\n")

freeswitch.consoleLog("info", uuid .. " start bot!")
local record_path = session:getVariable('local_record_path') or '';
session:execute("record_session", record_path)
session:setVariable("START_BOT", "true")
