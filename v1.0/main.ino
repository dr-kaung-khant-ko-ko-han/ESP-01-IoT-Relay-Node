/*
  ESP-01 + ESP-01 Relay Module (GPIO0 type) - Web Server

  ESP-01 only exposes GPIO0, GPIO2, TX (GPIO1) and RX (GPIO3).
  This relay board is driven from GPIO0, active-LOW (LOW = relay ON).

  Open  http://relay01.local  (or the IP address from your router)

  HTTP API (for Home Assistant / curl / automations):
    GET /api/state        -> {"on":0|1,"rssi":-58,"up":1234}
    GET /api/relay?s=1    -> relay ON
    GET /api/relay?s=0    -> relay OFF
    GET /api/relay?s=t    -> toggle

  Arduino IDE: Generic ESP8266 Module | Flash Mode DOUT
               Flash Size 512KB (ESP-01) or 1MB (ESP-01S)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// ---------------- User settings ----------------
const char* WIFI_SSID = "REPLACE_WITH_YOUR_SSID";
const char* WIFI_PASS = "REPLACE_WITH_YOUR_PASSWORD";
const char* HOSTNAME  = "relay01"; // open http://relay01.local

const uint8_t RELAY_PIN        = 0;      // GPIO0
const bool    RELAY_ACTIVE_LOW = true;   // LOW turns the relay ON
// -----------------------------------------------

ESP8266WebServer server(80);
bool relayOn = false;

// ---------------- Web page (stored in flash, not RAM) ----------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>Relay</title><link rel="icon" href="data:,">
<style>
:root{--bg:#e3e9f3;--card:#fbfcfe;--ink:#14203a;--mute:#6a768e;--line:#dbe2ed;--t1:#d3dae7;--t2:#bec7d8}
@media(prefers-color-scheme:dark){:root{--bg:#090e1b;--card:#121a2d;--ink:#eaf0fb;--mute:#8b97b1;--line:#222c45;--t1:#34405f;--t2:#28324d}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;min-height:100dvh;display:grid;place-items:center;background:var(--bg);color:var(--ink);font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
body::before{content:"";position:fixed;inset:0;pointer-events:none;opacity:0;transition:opacity .6s;background:radial-gradient(55% 40% at 50% 52%,rgba(255,176,32,.3),transparent 70%)}
body.on::before{opacity:1}
main{position:relative;width:min(92vw,340px);padding:22px 22px 18px;border:1px solid var(--line);border-radius:30px;background:var(--card);box-shadow:0 24px 50px -22px rgba(15,25,55,.4)}
header{display:flex;justify-content:space-between;align-items:flex-start;gap:8px}
h1{margin:0;font-size:19px;font-weight:700;letter-spacing:-.01em}
#h{font:12px ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;color:var(--mute);word-break:break-all}
.st{display:flex;align-items:center;gap:6px;padding:4px 10px;border-radius:99px;background:var(--bg);font-size:12px;color:var(--mute);white-space:nowrap}
.st i{width:7px;height:7px;border-radius:50%;background:#e5484d}
.st.ok i{background:#30a46c;box-shadow:0 0 0 3px rgba(48,164,108,.22)}
#w{position:relative;display:block;width:128px;height:220px;margin:26px auto 18px;padding:0;border:0;border-radius:64px;cursor:pointer;background:linear-gradient(var(--t1),var(--t2));box-shadow:inset 0 4px 10px rgba(0,0,0,.28),inset 0 -2px 4px rgba(255,255,255,.2);transition:background .3s,box-shadow .3s,transform .15s,opacity .3s}
#w i{position:absolute;left:10px;top:10px;width:108px;height:108px;border-radius:50%;display:grid;place-items:center;background:radial-gradient(circle at 35% 28%,#fff,#e8ecf4 72%);box-shadow:0 8px 16px rgba(0,0,0,.32),inset 0 -3px 6px rgba(0,0,0,.08);transform:translateY(92px);transition:transform .34s cubic-bezier(.3,1.5,.5,1)}
#w svg{width:42px;height:42px;fill:none;stroke:#9aa5bd;stroke-width:2.4;stroke-linecap:round;transition:stroke .3s}
#w.on{background:linear-gradient(#ffcd52,#ff9f0a);box-shadow:inset 0 4px 10px rgba(120,60,0,.3),0 0 0 6px rgba(255,176,32,.15),0 0 60px 10px rgba(255,176,32,.5)}
#w.on i{transform:none}
#w.on svg{stroke:#f59e0b}
#w:active:not(:disabled){transform:scale(.97)}
#w:disabled{opacity:.5;cursor:default}
#w:focus-visible{outline:3px solid var(--ink);outline-offset:5px}
#s{margin:0;text-align:center;font-size:34px;font-weight:750;letter-spacing:-.02em;line-height:1.1}
#k{margin:4px 0 0;text-align:center;font-size:13px;color:var(--mute)}
footer{display:flex;justify-content:space-between;align-items:center;margin-top:22px;padding-top:14px;border-top:1px solid var(--line);font-size:13px;color:var(--mute)}
b{color:var(--ink);font-weight:600;font-variant-numeric:tabular-nums}
.sg{display:inline-flex;align-items:flex-end;gap:2px;height:13px;margin-right:6px;vertical-align:-1px}
.sg u{width:3px;border-radius:1px;background:var(--line)}
.sg u:nth-child(1){height:4px}.sg u:nth-child(2){height:7px}.sg u:nth-child(3){height:10px}.sg u:nth-child(4){height:13px}
.sg u.a{background:var(--ink)}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
</style></head><body>
<main>
<header><div><h1>Relay</h1><div id="h"></div></div><div class="st" id="c"><i></i><span id="n">Connecting</span></div></header>
<button id="w" role="switch" aria-checked="false" aria-label="Relay power" disabled><i><svg viewBox="0 0 24 24"><path d="M12 3v9M6.4 6.6a8 8 0 1 0 11.2 0"/></svg></i></button>
<p id="s">&hellip;</p>
<p id="k">&nbsp;</p>
<footer><div><span class="sg" id="g"><u></u><u></u><u></u><u></u></span><b id="r">&ndash;</b></div><div>Uptime <b id="u">&ndash;</b></div></footer>
</main>
<script>
const $=i=>document.getElementById(i),w=$('w'),B=document.body;
const T=t=>{let d=t/86400|0,h=t%86400/3600|0,m=t%3600/60|0;return d?d+'d '+h+'h':h?h+'h '+m+'m':m+'m '+t%60+'s'};
function live(v){$('c').className='st'+(v?' ok':'');$('n').textContent=v?'Connected':'Offline';w.disabled=!v}
function show(d){
w.className=B.className=d.on?'on':'';w.setAttribute('aria-checked',!!d.on);
$('s').textContent=d.on?'On':'Off';$('k').textContent=d.on?'Relay is energized':'Relay is idle';
document.title='Relay \u00b7 '+(d.on?'On':'Off');
$('r').textContent=d.rssi+' dBm';$('u').textContent=T(d.up);
const n=d.rssi>-50?4:d.rssi>-67?3:d.rssi>-80?2:1;
[...$('g').children].forEach((e,i)=>e.className=i<n?'a':'');
live(1)}
async function go(u){try{show(await(await fetch(u,{cache:'no-store'})).json())}catch(e){live(0)}}
w.onclick=()=>{const n=w.className?0:1;w.className=B.className=n?'on':'';
navigator.vibrate&&navigator.vibrate(12);go('/api/relay?s='+n)};
$('h').textContent=location.host;
go('/api/state');setInterval(()=>go('/api/state'),3000);
</script></body></html>)rawliteral";

// ---------------- Relay control ----------------
void relaySet(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// ---------------- HTTP handlers ----------------
void sendState() {
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"on\":%d,\"rssi\":%d,\"up\":%lu}",
           relayOn ? 1 : 0, (int)WiFi.RSSI(), millis() / 1000UL);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleRelay() {
  String s = server.arg("s");
  if (s == "1")      relaySet(true);
  else if (s == "0") relaySet(false);
  else if (s == "t") relaySet(!relayOn);
  sendState();
}

// ---------------- Setup / loop ----------------
void setup() {
  Serial.begin(115200);

  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);   // relay OFF before pin becomes output
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false);

  WiFi.persistent(false);                // don't wear out the flash
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.on("/api/state", sendState);
  server.on("/api/relay", handleRelay);
  server.on("/favicon.ico", []() { server.send(204); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void loop() {
  server.handleClient();
  MDNS.update();
}
