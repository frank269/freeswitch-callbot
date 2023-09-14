local uuid = session:getVariable('uuid') or '';

if uuid == '' then return; end

local conversation_id = session:getVariable("CONVERSATION_ID") or '';
local server_url = "http://172.16.90.35:30008/voicemail/v1/submit_task"
local auth_token = "iaKglGRsR8wlzx5hEcJJ3U2GlgaaMIqA"
local callback_url = "http://172.16.90.41:9000"
local record_path = "/var/lib/freeswitch/recordings/record_on_media/" .. uuid .. ".wav";
local public_path = "https://callbot-audio.metechvn.com/file/record_on_media/" .. uuid .. ".wav";

local detect_voicemail = session:getVariable("detect_voicemail") or '';
if detect_voicemail == '1' then
    session:sleep(500);
    freeswitch.consoleLog("info", "action on answer phase, stop recording... !, record path: " .. record_path .. " \n")
    session:execute("stop_record_session", record_path)
    freeswitch.consoleLog("info", uuid .. " send api to detect voice mail!")
    local jsonRequest = string.format('{"token": "%s","conversion_id":"%s","callback_url":"%s","audio_url":"%s"}',auth_token,uuid,callback_url,public_path)
    
    local api = freeswitch.API();
    local response = api:executeString("curl ".. server_url .. " timeout 3 content-type 'application/json' post '"..jsonRequest.."'") or '';

    freeswitch.consoleLog("info", uuid .. " detect voice mail response: " .. response .. "\n")
end

local currentTimeSeconds = os.time()
local currentClockTimeSeconds = os.clock()
local milliseconds = (currentTimeSeconds * 1000) + ((currentClockTimeSeconds - math.floor(currentClockTimeSeconds)) * 1000)


freeswitch.consoleLog("info", uuid .. " start bot!")
local record_name = os.date('%Y/%m/%d/%H/') .. uuid .. '.wav'
local local_record_path = '/var/lib/freeswitch/recordings/callbot/' .. record_name
local record_path = 'https://172.16.88.13/file/callbot/' .. record_name

session:execute("record_session", record_path)

session:setVariable("local_record_path", local_record_path)
session:setVariable("record_path", record_path)
session:setVariable("PICKUP_AT", milliseconds)
session:setVariable("START_BOT", "true")
--session:execute("displace_session","/city.mp3 mlfw")