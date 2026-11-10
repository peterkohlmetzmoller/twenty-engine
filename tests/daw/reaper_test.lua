-- reaper_test.lua: plays the Twenty Engine plugins in REAPER, headless on a CI machine, and renders the results.
-- Started by REAPER at launch (copied to <resource path>/Scripts/__startup.lua). TE_OUT names the output folder.
-- Writes reaper_log.txt (what happened) and one WAV per case; analyze.py checks them.
local OUT = os.getenv("TE_OUT") or reaper.GetResourcePath()
local logf = io.open(OUT .. "/reaper_log.txt", "w")
local function L(...)
  local t = {} for i = 1, select("#", ...) do t[#t + 1] = tostring(select(i, ...)) end
  logf:write(table.concat(t, " ") .. "\n") logf:flush()
end

local NOTES = {  -- {start s, length s, key, velocity}: a chord, then a short line, then a held low note
  {0.5, 2.0, 48, 100}, {0.5, 2.0, 55, 90}, {0.5, 2.0, 60, 90}, {0.5, 2.0, 64, 80},
  {3.0, 0.4, 67, 100}, {3.5, 0.4, 69, 100}, {4.0, 0.4, 72, 110}, {4.5, 0.4, 71, 100},
  {5.0, 3.5, 36, 110}, {5.0, 3.5, 60, 90},
}
local LEN = 10.0

local function clear()
  for i = reaper.CountTracks(0) - 1, 0, -1 do reaper.DeleteTrack(reaper.GetTrack(0, i)) end
end

local function newTrack(fxname)
  local n = reaper.CountTracks(0)
  reaper.InsertTrackAtIndex(n, true)
  local tr = reaper.GetTrack(0, n)
  local fx = reaper.TrackFX_AddByName(tr, fxname, false, -1)
  local _, got = reaper.TrackFX_GetFXName(tr, math.max(fx, 0), "")
  L("ADD", fxname, "->", fx, got)
  return tr, fx
end

local function midiItem(tr, ccNum)
  local item = reaper.CreateNewMIDIItemInProj(tr, 0, LEN, false)
  local take = reaper.GetActiveTake(item)
  for _, n in ipairs(NOTES) do
    local s = reaper.MIDI_GetPPQPosFromProjTime(take, n[1])
    local e = reaper.MIDI_GetPPQPosFromProjTime(take, n[1] + n[2])
    reaper.MIDI_InsertNote(take, false, false, s, e, 0, n[3], n[4], true)
  end
  if ccNum then   -- a sweep from closed to open over the whole part
    for k = 0, 64 do
      local t = 0.4 + k * (8.0 / 64)
      reaper.MIDI_InsertCC(take, false, false, reaper.MIDI_GetPPQPosFromProjTime(take, t), 0xB0, 0, ccNum, math.min(127, k * 2))
    end
  end
  reaper.MIDI_Sort(take)
  return item
end

-- Automation's stream (engine/paramstream.h): the LPF cutoff (setting 29 -> controller 31) from closed to open,
-- 14 bits: channel 15 low byte, channel 16 high byte - as the plugin's own port records it into a region
local function streamItem(tr)
  local item = midiItem(tr, nil)
  local take = reaper.GetActiveTake(item)
  for k = 0, 64 do
    local t = 0.4 + k * (8.0 / 64)
    local v = math.floor(k * 16383 / 64)
    local ppq = reaper.MIDI_GetPPQPosFromProjTime(take, t)
    reaper.MIDI_InsertCC(take, false, false, ppq, 0xB0, 14, 31, v % 128)
    reaper.MIDI_InsertCC(take, false, false, ppq, 0xB0, 15, 31, math.floor(v / 128))
  end
  reaper.MIDI_Sort(take)
  return item
end

local function findParam(tr, fx, wanted)
  for p = 0, reaper.TrackFX_GetNumParams(tr, fx) - 1 do
    local _, name = reaper.TrackFX_GetParamName(tr, fx, p, "")
    if name == wanted then return p end
  end
  return -1
end

local function render(name)
  reaper.GetSetProjectInfo_String(0, "RENDER_FILE", OUT, true)
  reaper.GetSetProjectInfo_String(0, "RENDER_PATTERN", name, true)
  reaper.GetSetProjectInfo_String(0, "RENDER_FORMAT", "evaw", true)
  reaper.GetSetProjectInfo(0, "RENDER_SETTINGS", 0, true)
  reaper.GetSetProjectInfo(0, "RENDER_BOUNDSFLAG", 0, true)
  reaper.GetSetProjectInfo(0, "RENDER_STARTPOS", 0, true)
  reaper.GetSetProjectInfo(0, "RENDER_ENDPOS", LEN, true)
  reaper.GetSetProjectInfo(0, "RENDER_SRATE", 48000, true)
  reaper.GetSetProjectInfo(0, "RENDER_CHANNELS", 2, true)
  reaper.GetSetProjectInfo(0, "RENDER_TAILFLAG", 0, true)
  local t0 = reaper.time_precise()
  reaper.Main_OnCommand(42230, 0)   -- File: Render project, using the most recent render settings, auto-close render dialog
  L("RENDER", name, string.format("%.1fs", reaper.time_precise() - t0))
end

local function run()
  L("REAPER", reaper.GetAppVersion(), "OS", reaper.GetOS())
  local plugs = {}
  local i = 0
  while true do
    local ok, name, ident = reaper.EnumInstalledFX(i)
    if not ok then break end
    if name:find("Twenty Engine") then
      L("FOUND", name, "|", ident)
      local kind = name:match("^(%w+)i?:") or "?"
      plugs[#plugs + 1] = {name = name, studio = name:find("Automation") ~= nil, kind = name:match("^AU") and "AU" or "VST3"}
    end
    i = i + 1
  end
  L("PLUGINS", #plugs)

  for _, P in ipairs(plugs) do
    local tag = (P.studio and "studio" or "inst") .. "_" .. P.kind:lower()
    -- 1. notes only
    clear()
    local tr, fx = newTrack(P.name)
    if fx < 0 then L("FAIL", tag, "could not be added") goto continue end
    L("PARAMS", tag, reaper.TrackFX_GetNumParams(tr, fx))
    if P.studio then
      local names = {}
      for p = 0, math.min(reaper.TrackFX_GetNumParams(tr, fx), 60) - 1 do
        local _, n = reaper.TrackFX_GetParamName(tr, fx, p, "") names[#names + 1] = n
      end
      L("PARAMNAMES", tag, table.concat(names, ", "))
    end
    midiItem(tr, nil)
    render(tag .. "_notes")

    -- 2. a filter sweep: MIDI CC 74 on the track (how a plain MIDI instrument gets knob moves)
    clear()
    tr, fx = newTrack(P.name)
    midiItem(tr, 74)
    render(tag .. "_cc74")

    -- 3. the same sweep as DAW automation of "LPF Cutoff"
    clear()
    tr, fx = newTrack(P.name)
    midiItem(tr, nil)
    local pc = findParam(tr, fx, "LPF Cutoff")
    L("PARAM", tag, "LPF Cutoff =", pc)
    if pc >= 0 then
      local env = reaper.GetFXEnvelope(tr, fx, pc, true)
      local _, lo, hi = reaper.TrackFX_GetParam(tr, fx, pc)   -- envelopes use the parameter's own range (AU: -64..64)
      L("RANGE", tag, lo, hi)
      if env then
        reaper.InsertEnvelopePoint(env, 0.0, lo, 0, 0, false, true)
        reaper.InsertEnvelopePoint(env, 0.4, lo, 0, 0, false, true)
        reaper.InsertEnvelopePoint(env, 8.4, hi, 0, 0, false, true)
        reaper.Envelope_SortPoints(env)
        L("ENVELOPE", tag, reaper.CountEnvelopePoints(env), "points")
      else
        L("ENVELOPE", tag, "none")
      end
    end
    render(tag .. "_auto")

    -- 3b. Automation: the same sweep carried as the plugin's own stream in the region
    if P.studio then
      clear()
      tr, fx = newTrack(P.name)
      streamItem(tr)
      render(tag .. "_stream")
    end

    -- 4. state: change two knobs, copy the track (save + load of the plugin's state), compare
    if pc >= 0 then
      clear()
      tr, fx = newTrack(P.name)
      local pv = findParam(tr, fx, "VCO 2 Level")
      reaper.TrackFX_SetParamNormalized(tr, fx, pc, 0.123)
      if pv >= 0 then reaper.TrackFX_SetParamNormalized(tr, fx, pv, 0.789) end
      local ok, chunk = reaper.GetTrackStateChunk(tr, "", false)
      reaper.InsertTrackAtIndex(1, true)
      local tr2 = reaper.GetTrack(0, 1)
      reaper.SetTrackStateChunk(tr2, chunk, false)
      local a = reaper.TrackFX_GetParamNormalized(tr2, 0, pc)
      local b = pv >= 0 and reaper.TrackFX_GetParamNormalized(tr2, 0, pv) or -1
      L("STATE", tag, string.format("%.4f %.4f", a, b), "chunk", #chunk)
    end
    ::continue::
  end

  -- 5. both plugins in one project (two tracks)
  local inst, studio
  for _, P in ipairs(plugs) do
    if P.kind == "VST3" and not P.studio then inst = P.name end
    if P.kind == "VST3" and P.studio then studio = P.name end
  end
  if inst and studio then
    clear()
    local t1 = newTrack(inst) midiItem(t1, nil)
    local t2 = newTrack(studio) midiItem(t2, 74)
    render("both_vst3")
  end
  clear()
  L("DONE")
end

reaper.defer(function()
  local ok, err = pcall(run)
  if not ok then L("ERROR", err) L("DONE") end
end)
