# The free web edition in headless Chrome, with a pretend MS-20ic and a pretend keyboard (Web MIDI mocked):
# the panel draws (WebGL), the sound starts, the iC gets its LEDs (the start-up light show), the iC's keys reach the
# sound, the iC's knobs move the panel's knobs, CC learn on another keyboard, a sound saved on the keys stays in
# the browser, LOG copies the log. Needs: pip install playwright, and Chrome.
#   python3 tests/web_page_test.py <folder with the built page> [<chrome>] [<picture.png>]
import asyncio, functools, http.server, sys, threading
from playwright.async_api import async_playwright

ROOT = sys.argv[1]
CHROME = sys.argv[2] if len(sys.argv) > 2 else '/usr/bin/google-chrome'
PICTURE = sys.argv[3] if len(sys.argv) > 3 else 'web_panel.png'

MOCK = r"""
(() => {
  window.__sent = []; window.__toSound = [];
  const orig = MessagePort.prototype.postMessage;
  MessagePort.prototype.postMessage = function (m, ...r) {
    try { if (m && m.t) window.__toSound.push(JSON.parse(JSON.stringify(m))); } catch (e) {}
    return orig.call(this, m, ...r);
  };
  const ic = { name: 'MS-20ic', onmidimessage: null }, kb = { name: 'Keystation 49', onmidimessage: null };
  const out = { name: 'MS-20ic', send: (b) => window.__sent.push(Array.from(b)) };
  window.__ic = ic; window.__kb = kb;
  const access = { inputs: new Map([['a', ic], ['b', kb]]), outputs: new Map([['c', out]]), onstatechange: null, sysexEnabled: true };
  navigator.requestMIDIAccess = () => Promise.resolve(access);
})();
"""

failed = 0
def check(ok, what):
    global failed
    print(('ok    ' if ok else 'FAIL  ') + what)
    if not ok: failed += 1

class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a): pass

async def main():
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Quiet, directory=ROOT))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = 'http://127.0.0.1:%d/index.html' % srv.server_address[1]
    async with async_playwright() as p:
        b = await p.chromium.launch(executable_path=CHROME, headless=True,
            args=['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--autoplay-policy=no-user-gesture-required'])
        ctx = await b.new_context(viewport={'width': 1400, 'height': 760})
        await ctx.grant_permissions(['clipboard-read', 'clipboard-write'], origin=url.rsplit('/', 1)[0])
        await ctx.add_init_script(MOCK)
        pg = await ctx.new_page(); errs = []
        pg.on('pageerror', lambda e: errs.append(str(e)))
        await pg.goto(url); await pg.wait_for_timeout(2000)
        check(await pg.evaluate("getComputedStyle(document.getElementById('nogl')).display") == 'none', 'the panel draws (WebGL)')

        await pg.click('#start'); await pg.wait_for_timeout(3000)
        check(await pg.evaluate("document.getElementById('start').style.display") == 'none', 'the sound started')
        sent = await pg.evaluate('window.__sent')
        check(len(sent) >= 50 and sent[0][:6] == [0xF0, 0x42, 0x40, 0x6E, 0x01, 0x01], 'the iC gets its LEDs (light show: %d messages)' % len(sent))

        await pg.evaluate("window.__toSound.length = 0; window.__ic.onmidimessage({ data: new Uint8Array([0x90, 60, 100]) })")
        await pg.wait_for_timeout(200)
        await pg.evaluate("window.__ic.onmidimessage({ data: new Uint8Array([0x80, 60, 0]) })")
        await pg.wait_for_timeout(200)
        ts = await pg.evaluate('window.__toSound')
        check({'t': 'midi', 's': 0x90, 'd1': 60, 'd2': 100} in ts, "the iC's key reaches the sound")

        box = await pg.eval_on_selector('#panel', 'e => { const r = e.getBoundingClientRect(); return [r.left, r.top, r.width]; }')
        s = box[2] / 1224.0
        at = lambda x, y: (box[0] + x * s, box[1] + y * s)
        before = await pg.screenshot()
        await pg.evaluate("for (const c of [18, 19, 20, 22, 70, 71, 72, 73, 74]) window.__ic.onmidimessage({ data: new Uint8Array([0xB0, c, 127]) })")
        await pg.wait_for_timeout(1200)
        check(await pg.screenshot() != before, "the iC's knobs move the panel's knobs")

        kx, ky = at(356, 120)   # LPF CUTOFF FREQUENCY
        await pg.mouse.click(kx, ky, button='right'); await pg.wait_for_timeout(300)
        await pg.evaluate("window.__toSound.length = 0; window.__kb.onmidimessage({ data: new Uint8Array([0xB0, 21, 64]) })")
        await pg.wait_for_timeout(300)
        await pg.evaluate("window.__kb.onmidimessage({ data: new Uint8Array([0xB0, 21, 10]) })")
        await pg.wait_for_timeout(600)
        ts = await pg.evaluate('window.__toSound')
        cc = await pg.evaluate("localStorage.getItem('twenty-engine-ccmap') || ''")
        check(any(m.get('t') == 'set' for m in ts) and '21 Filter_Macro lpfcut' in cc, 'CC learn: CC 21 on another keyboard moves LPF cutoff, the map is kept')

        sx, sy = at(684, 559)   # SWITCH: hold it and play a key = save the sound on that key
        await pg.mouse.move(sx, sy); await pg.mouse.down()
        await pg.keyboard.press('z'); await pg.keyboard.down('d'); await pg.wait_for_timeout(2600); await pg.keyboard.up('d')
        await pg.mouse.up(); await pg.wait_for_timeout(800)
        check(len(await pg.evaluate("localStorage.getItem('twenty-engine-mem') || ''")) > 4000, 'a sound saved on the keys stays in the browser')

        lx, ly = at(853, 559)   # LOG
        await pg.mouse.click(lx, ly); await pg.wait_for_timeout(700)
        clip = await pg.evaluate("navigator.clipboard.readText()")
        check('Twenty Engine bug log' in clip and 'MS-20ic:     found, LED output open' in clip and 'AUDIO' in clip, 'LOG copies the log')
        await pg.mouse.click(10, 10); await pg.wait_for_timeout(400)
        await pg.screenshot(path=PICTURE)
        check(not errs, 'no script errors' + ('' if not errs else ': ' + '; '.join(errs)))
        await b.close()
    srv.shutdown()
    print('ALL WEB PAGE CHECKS PASSED' if failed == 0 else '%d WEB PAGE CHECKS FAILED' % failed)
    sys.exit(1 if failed else 0)

asyncio.run(main())
