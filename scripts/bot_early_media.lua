local uuid = session:getVariable('uuid') or '';

if uuid == '' then return; end

local detect_voicemail = session:getVariable("detect_voicemail") or '0';
if detect_voicemail == '1' then
  local record_path = "/var/lib/freeswitch/recordings/record_on_media/" .. uuid .. ".wav";
  freeswitch.consoleLog("info", uuid .. " action on early media phase, start recording to path " .. record_path .. " ... !\n")
  session:execute("record_session", record_path)

  uuid = null;
  record_path = null;
end
