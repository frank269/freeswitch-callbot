local uuid = session:getVariable('uuid') or '';

if uuid == '' then return; end
local api = freeswitch.API();
require "resources.functions.file_exists";
local vm_detect_time = session:getVariable('vm_detect_time') or '50';

local detect_voicemail = session:getVariable("detect_voicemail") or '0';
if detect_voicemail == '1' then
  session:sleep(5000);
  freeswitch.consoleLog("info", "schedule to start detect voicemail ... !\n")
  session:execute("avmd_start","debug=0,detectors_n=18")
  local stop_vm_file = (scripts_dir or '') .. '/app/vm_detect/stop.lua';
  if file_exists(stop_vm_file) then
    local sched_result = api:executeString('sched_api +' .. vm_detect_time .. ' vm_detect lua ' .. stop_vm_file .. ' ' .. uuid) or '-ERR';
    if sched_result:sub(1, 4) ~= '-ERR' then
      api:executeString('uuid_setvar ' .. uuid .. ' vm_sched_id ' .. sched_result:sub(11));
    end
  else
    local sched_result = api:executeString('sched_api +' .. vm_detect_time .. ' vm_detect avmd ' .. uuid .. ' stop') or '-ERR';
    if sched_result:sub(1, 4) ~= '-ERR' then
      api:executeString('uuid_setvar ' .. uuid .. ' vm_sched_id ' .. sched_result:sub(11));
    end
  end
end
