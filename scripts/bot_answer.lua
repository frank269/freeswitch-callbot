local uuid = session:getVariable('uuid') or '';


if uuid == '' then return; end

function file_exists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end 
end


local conversation_id = session:getVariable("CONVERSATION_ID") or '';
local server_url = os.getenv("VOICEMAIL_CHECK_URL") or "http://172.16.90.35:30008/voicemail/v1/submit_task"
local auth_token = os.getenv("VOICEMAIL_AUTH_TOKEN") or "iaKglGRsR8wlzx5hEcJJ3U2GlgaaMIqA"
local callback_url = os.getenv("VOICEMAIL_RESULT_CALLBACK") or "http://172.16.90.41:9000"
local record_path = "/var/lib/freeswitch/recordings/record_on_media/" .. uuid .. ".wav"
local default_host = os.getenv("DEFAULT_HOST") or 'callbot-audio.metechvn.com'
local public_ip = os.getenv("PUBLIC_IP") or '172.16.88.13'
local public_path = "https://" ..public_ip.. "/file/record_on_media/" .. uuid .. ".wav"

local detect_voicemail = session:getVariable("detect_voicemail") or '0';
if detect_voicemail == '1' then
  session:sleep(500);
  freeswitch.consoleLog("info", "action on answer phase, stop recording... !, record path: " .. record_path .. " \n")
  session:execute("stop_record_session", record_path)
  freeswitch.consoleLog("info", uuid .. " send api to detect voice mail!")
  local jsonRequest = string.format('{"token": "%s","conversion_id":"%s","callback_url":"%s","audio_url":"%s"}',auth_token,uuid,callback_url,public_path)
  local api = freeswitch.API();
  local response = api:executeString("curl ".. server_url .. " timeout 3 content-type 'application/json' post '"..jsonRequest.."'") or '';
  freeswitch.consoleLog("info", uuid .. " detect voice mail request: " .. jsonRequest .. "\n")
  jsonRequest = null;
  freeswitch.consoleLog("info", uuid .. " detect voice mail response: " .. response .. "\n")
  api = null;
  response = null;
end

freeswitch.consoleLog("info", uuid .. " start bot!")
local record_name = os.date('%Y/%m/%d/%H/') .. uuid .. '.wav'
local local_record_path = '/var/lib/freeswitch/recordings/callbot/' .. record_name
local record_path = 'https://'..public_ip..'/file/callbot/' .. record_name
session:execute("record_session", local_record_path)


local currentTimeSeconds = os.time()
local currentClockTimeSeconds = os.clock()
local milliseconds = (currentTimeSeconds * 1000) + ((currentClockTimeSeconds - math.floor(currentClockTimeSeconds)) * 1000)
session:setVariable("local_record_path", local_record_path)
session:setVariable("record_path", record_path)
session:setVariable("PICKUP_AT", milliseconds)
session:setVariable("START_BOT", "true")
local background_sound = session:getVariable("background_sound") or '';
if background_sound ~= '' and file_exists(background_sound) then
  freeswitch.consoleLog("info", uuid .. " play background sound: " .. background_sound);
  session:execute("displace_session", background_sound .. " mlf")
end


uuid = null;
conversation_id = null;
server_url = null;
auth_token = null;
callback_url = null;
record_path = null;
public_path = null;
detect_voicemail = null;
record_path = null;
