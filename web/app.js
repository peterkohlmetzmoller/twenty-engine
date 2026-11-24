// app.js - Twenty Engine in the browser (the free web edition): the panel (ui.js / ui.wasm on a WebGL canvas),
// the sound (worklet.js + dsp.wasm in the audio thread) and Web MIDI (Chrome, Edge): the MS-20ic plays it with
// its keys, knobs and cables and gets its LEDs back; any other MIDI keyboard plays the keys; so does the
// computer's keyboard (A W S E D F T G Y H U J K = one octave, Z / X = octave down / up).
// Sounds saved on the keys and learned MIDI controllers stay in this browser (localStorage).
(function () {
  'use strict';
  const VERSION = '1.1.0';
  const SUPPORT = 'twentyenginesupport@gmail.com';
  const canvas = document.getElementById('panel');
  const overlay = document.getElementById('start');
  const logLines = [];
  const log = (s) => { const t = new Date().toTimeString().slice(0, 8); logLines.push(t + '  ' + s); if (logLines.length > 600) logLines.shift(); };
  log('START  Twenty Engine ' + VERSION + ' (web)');

  let ui = null, ctx = null, node = null, midi = null, sysex = false, icOut = null, icIn = false, leds = 0x10;
  const send = (m) => { if (node) node.port.postMessage(m); };

  // ---------------- storage
  const b64 = (u8) => { let s = ''; for (let i = 0; i < u8.length; i++) s += String.fromCharCode(u8[i]); return btoa(s); };
  const unb64 = (s) => { const r = atob(s), u = new Uint8Array(r.length); for (let i = 0; i < r.length; i++) u[i] = r.charCodeAt(i); return u; };
  const store = { get: (k) => { try { return localStorage.getItem(k); } catch (e) { return null; } }, set: (k, v) => { try { localStorage.setItem(k, v); } catch (e) {} } };

  // ---------------- the panel
  function fit() {
    const kw = ui ? ui._te_ui_kw() : 1224, kh = ui ? ui._te_ui_kh() : 580;
    const bar = document.getElementById('bar').offsetHeight;
    const aw = window.innerWidth, ah = window.innerHeight - bar;
    const s = Math.min(aw / kw, ah / kh);
    const w = Math.floor(kw * s), h = Math.floor(kh * s), dpr = window.devicePixelRatio || 1;
    canvas.style.width = w + 'px'; canvas.style.height = h + 'px';
    canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    return { w, h, dpr };
  }
  let size = { w: 0, h: 0, dpr: 1 }, force = true;
  window.addEventListener('resize', () => { size = fit(); force = true; });

  function frame() {
    if (ui) { ui._te_ui_frame(size.w, size.h, size.dpr, force ? 1 : 0); force = false; }
    requestAnimationFrame(frame);
  }

  function mods(e) { return (e.shiftKey ? 1 : 0) | ((e.ctrlKey || e.metaKey) ? 2 : 0) | (e.altKey ? 4 : 0); }
  function pos(e) { const r = canvas.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; }
  let down = false;
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('mousedown', (e) => { if (!ui) return; down = true; const [x, y] = pos(e); ui._te_ui_mouse(0, x, y, e.button, mods(e), performance.now(), 0); e.preventDefault(); });
  window.addEventListener('mouseup', (e) => { if (!ui || !down) return; down = false; const [x, y] = pos(e); ui._te_ui_mouse(1, x, y, e.button, mods(e), performance.now(), 0); });
  window.addEventListener('mousemove', (e) => { if (!ui) return; const [x, y] = pos(e); ui._te_ui_mouse(2, x, y, 0, mods(e), performance.now(), 0); });
  canvas.addEventListener('mouseleave', () => { if (ui && !down) ui._te_ui_mouse(4, 0, 0, 0, 0, 0, 0); });
  canvas.addEventListener('wheel', (e) => { if (!ui) return; const [x, y] = pos(e); ui._te_ui_mouse(3, x, y, 0, mods(e), performance.now(), e.deltaY < 0 ? 1 : -1); e.preventDefault(); }, { passive: false });

  // ---------------- the bug log
  function bugLog() {
    const d = new Date();
    const ins = midi ? Array.from(midi.inputs.values()).map((p) => p.name).join(', ') : '';
    const outs = midi ? Array.from(midi.outputs.values()).map((p) => p.name).join(', ') : '';
    return 'Twenty Engine bug log  ·  ' + d.toISOString().slice(0, 16).replace('T', ' ') + '\n' +
      'Please email it to ' + SUPPORT + ' with a few words about what happened.\n\n' +
      'Edition:     Twenty Engine ' + VERSION + ' (web)\n' +
      'Browser:     ' + navigator.userAgent + '\n' +
      'Audio:       ' + (ctx ? ctx.sampleRate + ' Hz, ' + ctx.state + ', base latency ' + Math.round((ctx.baseLatency || 0) * 1000) + ' ms' : 'not started') + '\n' +
      'MIDI:        ' + (midi ? 'allowed' + (sysex ? ', SysEx allowed (LEDs)' : ', no SysEx (no LEDs)') : 'not allowed / not available') + '\n' +
      'Inputs:      ' + (ins || 'none') + '\n' +
      'Outputs:     ' + (outs || 'none') + '\n' +
      'MS-20ic:     ' + (icIn ? 'found' : 'not found') + (icOut ? ', LED output open' : '') + '\n\n' +
      'What happened (newest last):\n' + logLines.join('\n') + '\n';
  }
  function copyLog() {
    log('LOG copied by the user');
    const t = bugLog();
    if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(t).catch(() => fallbackCopy(t)); else fallbackCopy(t);
  }
  function fallbackCopy(t) { const a = document.createElement('textarea'); a.value = t; document.body.appendChild(a); a.select(); try { document.execCommand('copy'); } catch (e) {} a.remove(); }
  function mailLog() {
    const body = 'Twenty Engine ' + VERSION + ' (web)\n' + navigator.userAgent + '\n\nWhat happened:\n\n\n\nThe log (on the clipboard: paste it here):\n\n';
    window.location.href = 'mailto:' + SUPPORT + '?subject=' + encodeURIComponent('Twenty Engine bug report (web ' + VERSION + ')') + '&body=' + encodeURIComponent(body);
    log('LOG: email to support opened');
  }

  // ---------------- MIDI
  const isIc = (port) => /ms-20/i.test(port.name || '');
  function status() {
    if (!ui) return;
    const st = !midi ? 0 : (!icIn ? 1 : (icOut && sysex ? 3 : 2));
    ui._te_ui_status(icIn ? 1 : 0, st);
  }
  function onMidi(port, e) {
    const d = e.data; if (!d || d.length < 1) return;
    const s = d[0];
    if (s >= 0xF0) return;                                   // SysEx, clock: not for the engine
    const ic = isIc(port);
    if ((s & 0xF0) === 0x90 && d[2] > 0 && logLines.length < 5000) log('MIDI in ' + (ic ? 'MS-20ic ' : 'keyboard') + '  NOTE ON  ' + d[1] + ' ' + d[2]);
    if (ui) ui._te_ui_midi(s, d.length > 1 ? d[1] : 0, d.length > 2 ? d[2] : 0, ic ? 1 : 0);
  }
  function bind() {
    if (!midi) return;
    let foundIn = false, out = null;
    midi.inputs.forEach((p) => { p.onmidimessage = (e) => onMidi(p, e); if (isIc(p)) foundIn = true; });
    if (sysex) midi.outputs.forEach((p) => { if (isIc(p)) out = p; });
    if (foundIn !== icIn) log('CONTROLLER ' + (foundIn ? 'MS-20ic found' : 'MS-20ic gone'));
    const newOut = out && out !== icOut;
    icIn = foundIn; icOut = out;
    if (newOut) { log('CONTROLLER LED output open'); send({ t: 'lights' }); }   // the start-up light show: the LEDs work
    status();
  }
  async function startMidi() {
    if (!navigator.requestMIDIAccess) { log('MIDI: this browser has no Web MIDI (use Chrome or Edge)'); status(); return; }
    try { midi = await navigator.requestMIDIAccess({ sysex: true }); sysex = true; }
    catch (e) {
      log('MIDI: SysEx not allowed (' + e.name + ') - no LEDs');
      try { midi = await navigator.requestMIDIAccess(); } catch (e2) { log('MIDI: not allowed (' + e2.name + ')'); midi = null; }
    }
    if (midi) { midi.onstatechange = () => bind(); log('MIDI allowed' + (sysex ? ' (with SysEx)' : '')); bind(); }
    status();
  }
  function ledMessage(b) {
    if (icOut) { try { icOut.send(b); } catch (e) {} }
    // F0 42 40 6E 01 01 <led> <3F on / 1F off> F7: the panel's LEDs too
    if (b.length >= 8 && b[0] === 0xF0 && b[5] === 0x01 && b[6] < 5) { const bit = 1 << b[6]; leds = b[7] === 0x3F ? (leds | bit) : (leds & ~bit); if (ui) ui._te_ui_leds(leds); }
  }

  // ---------------- the computer's keyboard plays notes
  const keyMap = { a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7, y: 8, h: 9, u: 10, j: 11, k: 12 };
  let octave = 60; const held = {};
  window.addEventListener('keydown', (e) => {
    if (e.repeat || e.ctrlKey || e.metaKey || e.altKey || !ui) return;
    const k = e.key.toLowerCase();
    if (k === 'z') { octave = Math.max(24, octave - 12); return; }
    if (k === 'x') { octave = Math.min(96, octave + 12); return; }
    if (k in keyMap && !held[k]) { held[k] = octave + keyMap[k]; ui._te_ui_midi(0x90, held[k], 100, 0); }
  });
  window.addEventListener('keyup', (e) => { const k = e.key.toLowerCase(); if (held[k] && ui) { ui._te_ui_midi(0x80, held[k], 0, 0); delete held[k]; } });

  // ---------------- start: the panel at once, the sound and MIDI after a click (the browser asks for one)
  TwentyEngineUI({
    canvas: canvas,
    onSet: (i, v) => send({ t: 'set', i: i, v: v }),
    onMidi: (s, d1, d2) => send({ t: 'midi', s: s, d1: d1, d2: d2 }),
    onCopyLog: copyLog,
    onMailLog: mailLog,
    onCursor: (hand) => { canvas.style.cursor = hand ? 'pointer' : 'default'; },
    onSaveCcMap: (t) => store.set('twenty-engine-ccmap', t),
    onLog: log,
  }).then((m) => {
    ui = m;
    if (!ui._te_ui_init()) { log('WebGL not available'); document.getElementById('nogl').style.display = 'block'; return; }
    const cc = store.get('twenty-engine-ccmap');
    if (cc) { const p = ui._malloc(cc.length * 4 + 1); ui.stringToUTF8 ? ui.stringToUTF8(cc, p, cc.length * 4 + 1) : null; ui._te_ui_ccmap(p); ui._free(p); }
    size = fit(); force = true; status();
    requestAnimationFrame(frame);
  });

  overlay.addEventListener('click', async () => {
    overlay.textContent = 'Starting…';
    try {
      ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
      await ctx.audioWorklet.addModule('worklet.js');
      const module = await WebAssembly.compile(await (await fetch('dsp.wasm')).arrayBuffer());
      const memText = store.get('twenty-engine-mem');
      node = new AudioWorkletNode(ctx, 'twenty-engine', { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
        processorOptions: { module: module, mem: memText ? unb64(memText) : null } });
      node.port.onmessage = (e) => {
        const m = e.data;
        if (m.t === 'led') ledMessage(m.b);
        else if (m.t === 'values') { for (let i = 0; i < m.v.length; i += 2) ui._te_ui_engine(m.v[i], m.v[i + 1]); }
        else if (m.t === 'mem') { store.set('twenty-engine-mem', b64(m.b)); log('MEMORY saved in this browser (' + m.b.length + ' bytes)'); }
      };
      node.connect(ctx.destination);
      await ctx.resume();
      log('AUDIO ' + ctx.sampleRate + ' Hz');
      overlay.style.display = 'none';
      startMidi();
    } catch (e) {
      log('AUDIO could not start: ' + e);
      overlay.textContent = 'The sound could not start here (' + e.message + '). Try Chrome or Edge.';
    }
  });
})();
