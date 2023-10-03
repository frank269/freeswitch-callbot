local filePath = event:getHeader('Playback-File-Path') or ''; -- ex: /be1704ef-7066-4ca5-8eb1-23db9de94642.wav
if filePath == '' or string.len(filePath) < 30 then return; end
local uuid = string.sub(filePath, 2, -5)
freeswitch.consoleLog("info", "bot_playback_stop: " .. uuid .. "\n")
--local session = freeswitch.Session(uuid)
--session:execute("broadcast","/Silent.wav aleg");
