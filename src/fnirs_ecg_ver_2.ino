//Libraries (SPI protocol, wifi/web server, max30003 ecg, LED Driver, Photodiodes)
#include <Wire.h>
#include <lp55231.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "protocentral_max30003.h"
#include <WebSocketsServer.h>
#include <lp55231.h> // LED driver

//Pin numbers
#define SD_CS 26 //chip select
#define SD_MOSI 6 //Multiple output, single input
#define SD_MISO 5 //Multiple input, single output
#define SD_SCLK 7 //Serial clock
//constant values as #defines rather than const variables to use less memory, as microcontroller RAM is quite limited
#define sample_num 20 //Number of samples to calculate HRV with
#define volt_bias 1.2 //Account for 3.3V power supply instead of 5

//fNIRS acquisition timing. One frame = the full 8-LED sequence, and the
//band-pass filter downstream assumes exactly FNIRS_FS frames per second.
//Per-frame budget at 50 Hz: 8 * (LED_SETTLE_US + FNIRS_AVG*~33us + I2C) ~= 5 ms.
#define FNIRS_FS        50.0f  //fNIRS frame rate [Hz]
#define FNIRS_PERIOD_US 20000  //1e6 / FNIRS_FS
#define LED_SETTLE_US   300    //LED turn-on settling before the detector is sampled
#define FNIRS_AVG       4      //ADC samples averaged per detector read
#define BASELINE_FRAMES 250    //frames averaged for the reference intensities (5 s at 50 Hz)
#define PUSH_PERIOD_MS  200    //websocket update interval (broadcasting every frame floods the link)
#define NIR_BATCH       16     //fNIRS frames buffered between pushes (10 at 50 Hz / 200 ms)
#define ECG_FS          256    //ECG sample rate [Hz]; must match max30003.setSamplingRate() below
#define ECG_BATCH       96     //ECG samples buffered between pushes (51 at 256 sps / 200 ms, ~2x margin)
#define ECG_DRAIN       16     //max ECG FIFO reads per frame (256 sps @ 20 ms fNIRS frame -> ~5.1 samples/frame, ~3x margin)
#define RR_BATCH        8      //accepted R-R intervals buffered between pushes

int det_LSP = 1; // Pin 1 - Left short channel pin
int det_LLP = 2; // left long channel pin
int det_RSP = 3; // Right short channel pin
int det_RLP = 4; // Right long channel pin


SPIClass ecg_spi(HSPI); //SPI protocol object (4 wire synchronous communication with sensors)
MAX30003 max30003(SD_CS, ecg_spi); //Object used to reference Protocenter MAX30003 ECG
Lp55231 ledChip;

//global variables (referenced by several functions)
uint16_t cRrInt = 0; //Current RR interval (keeps track of current RR interval to ensure that only values that differ are used in calculation. Many may be the same due to high sampling rate)
uint16_t rrInt[sample_num]; //Array of RR interval values to calculate HRV
float det_LS_740; // left side short
float det_LL_740; // left side long
float det_LS_850;
float det_RS_740;
float det_RS_850; // right side short
float det_RL_740; // right_side_long
float det_RL_850;

float dHbO2_raw = 0.0, dHb_raw = 0.0;    //set by calculateHb [uM]
float dHbO2_filt = 0.0, dHb_filt = 0.0;  //band-passed, what gets plotted and logged [uM]

//Buffers drained by the websocket push, so acquisition never waits on the link
float   nirO[NIR_BATCH], nirH[NIR_BATCH];
int32_t ecgBuf[ECG_BATCH];
uint16_t rrBuf[RR_BATCH];
uint32_t rrFrameBuf[RR_BATCH];   // fNIRS frame index (see fnirsFrameIdx) each R-peak landed in
uint8_t nirCount = 0, ecgCount = 0, rrCount = 0;

//Shared clock between the ECG and fNIRS streams: one tick per acquired fNIRS
//frame, so an R-peak detected in the same loop() pass as frame N is tagged N.
//Used only to timestamp R-R intervals accurately - it does not touch the chart.
uint32_t fnirsFrameIdx = 0;

unsigned int currentRR = 0; //Counter of RR interval values to know when there are enough to calculate HRV
double rmssd = 0.0; //HRV value calculated over a short period of time (RMSSD)
String rmssd_string; //String HRV value in order to display on website
double meanHRV = 0.0; //Moving average HRV value
unsigned int numHRV = 0; //Number of total HRV values used to calculate moving average
String meanHRV_string; //String mean HRV value in order to display on website

AsyncWebServer website(80); //Web server
WebSocketsServer socket(81); //Socket connection in order to send and recieve data

bool readData = false; //Whether or not the user has clicked to capture and display data

//HTML program
/*
Instrument-style single page UI served from flash.
Everything is self-contained: the SoftAP has no internet, so no CDN assets.
Protocol: every websocket message is a 3-character tag followed by its payload.
    hrv<f>            RMSSD [ms]
    avg<f>            mean RMSSD [ms]
    rso<f>            band-passed delta rSO2 [%]  nir<o,h;o,h;...>  band-passed delta HbO2 / delta HHb [uM], one pair per frame
    ecg<s,s,s,...>    raw ECG samples [ADC counts]
    rri<i,ms;i,ms;...> accepted R-R intervals; i is the fNIRS frame index (same
                       clock as nir) the R-peak landed in, ms is the interval
*/
char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Multimodal CRS Monitor</title>
<style>
  /* Neomorphic surfaces: one background colour, depth comes only from paired
     light/dark shadows. Raised = controls and readouts, inset = data wells. */
  :root{
    --bg:#232a36; --dk:#191e27; --lt:#2d3543;
    --txt:#e2e9f4; --dim:#8b9bb2;
    --hbo:#ff7a7a; --hhb:#4fd2ff; --ecg:#5ce894; --accent:#8aa4ff;
    --up:6px 6px 14px var(--dk), -6px -6px 14px var(--lt);
    --up-s:4px 4px 9px var(--dk), -4px -4px 9px var(--lt);
    --in:inset 5px 5px 11px var(--dk), inset -5px -5px 11px var(--lt);
  }
  *{box-sizing:border-box}
  body{margin:0;min-height:100vh;display:flex;flex-direction:column;
       color:var(--txt);font-size:19px;
       font-family:"Inter","Helvetica Neue",Arial,sans-serif;
       background:var(--bg);
       background-image:radial-gradient(900px 500px at 15% -10%, #2b3443 0%, transparent 60%),
                        radial-gradient(700px 400px at 100% 0%, #26303d 0%, transparent 55%)}

  header{display:flex;align-items:center;gap:14px;margin:16px;padding:14px 20px;flex:none;
         border-radius:18px;background:var(--bg);box-shadow:var(--up)}
  header h1{margin:0;font-size:27px;font-weight:600;letter-spacing:.14em;text-transform:uppercase;
            background:linear-gradient(90deg,var(--txt),var(--accent));
            -webkit-background-clip:text;-webkit-text-fill-color:transparent}
  #led{width:20px;height:20px;border-radius:50%;background:var(--bg);
       box-shadow:var(--in), 0 0 0 0 rgba(255,120,120,0);position:relative;flex:none}
  #led::after{content:"";position:absolute;inset:5px;border-radius:50%;
              background:#6b3a3a;box-shadow:0 0 6px #6b3a3a;transition:.3s}
  #led.up::after{background:var(--ecg);box-shadow:0 0 12px var(--ecg),0 0 24px var(--ecg)}
  #link{color:var(--dim);font-size:18px;margin-left:auto;letter-spacing:.04em}

  /* Full-height column: the charts absorb whatever the fixed rows leave over. */
  main{padding:0 16px 14px;flex:1 1 auto;display:flex;flex-direction:column;min-height:0}
  .tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:16px;margin-bottom:14px;flex:none}
  .tile{background:var(--bg);border-radius:20px;padding:9px 22px;box-shadow:var(--up)}
  .tile .lbl{color:var(--dim);font-size:14px;letter-spacing:.16em;text-transform:uppercase}
  .tile .val{font-family:"SF Mono",Menlo,Consolas,monospace;font-size:40px;line-height:1.25;
             font-weight:500;color:var(--accent);text-shadow:0 0 18px rgba(138,164,255,.3)}
  .tile .unit{color:var(--dim);font-size:19px;margin-left:8px;text-shadow:none}

  .chart{background:var(--bg);border-radius:18px;margin-bottom:16px;padding:12px;box-shadow:var(--up);
         flex:1 1 0;min-height:172px;display:flex;flex-direction:column}
  .chart .head{display:flex;flex-wrap:wrap;align-items:center;gap:6px 20px;padding:2px 8px 10px;flex:none;
               font-size:17px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim)}
  .key{display:flex;align-items:center;gap:8px;white-space:nowrap}
  .key i{width:26px;height:5px;border-radius:3px;display:inline-block}
  .scale{margin-left:auto;font-family:Menlo,Consolas,monospace;text-transform:none;letter-spacing:0;white-space:nowrap}
  .well{border-radius:14px;box-shadow:var(--in);padding:8px;flex:1 1 auto;min-height:0}
  canvas{display:block;width:100%;height:100%}

  /* three-column bar so the transport stays centred whatever the side widths are */
  .bar{display:grid;grid-template-columns:1fr auto 1fr;align-items:center;gap:16px;flex:none}
  /* stacked and stretched to the widest label, so all three read as one group */
  .side{display:flex;flex-direction:column;align-items:stretch;width:max-content;gap:10px;justify-self:start}
  .side button{padding:11px 20px;font-size:18px}
  .transport{display:flex;gap:30px;justify-self:center}
  .ctl{display:flex;flex-direction:column;align-items:center;gap:8px}
  .cap{font-size:19px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);white-space:nowrap}
  button{background:var(--bg);color:var(--txt);border:0;border-radius:16px;
         padding:15px 24px;font-size:20px;font-weight:500;letter-spacing:.03em;
         cursor:pointer;box-shadow:var(--up-s);transition:box-shadow .15s,color .15s}
  button:hover{color:var(--accent)}
  button:active{box-shadow:var(--in)}
  button.go{color:var(--ecg)}
  button.stop{color:var(--hbo)}
  /* transport controls: circular, larger hit area, pressed state reads as recessed */
  .rnd{width:104px;height:104px;border-radius:50%;padding:0;box-shadow:var(--up);
       display:flex;align-items:center;justify-content:center}
  .rnd .ic{font-size:36px;line-height:1}
  .rnd:active{box-shadow:var(--in)}
  #count{color:var(--dim);font-size:21px;justify-self:end;font-family:Menlo,Consolas,monospace}
</style>
</head>
<body>
<header>
  <div id="led"></div>
  <h1>Multimodal Cognitive Restructuring Monitor</h1>
  <div id="link">disconnected</div>
</header>

<main>
  <div class="tiles">
    <div class="tile"><div class="lbl">HRV RMSSD</div><div class="val"><span id="hrv">--</span><span class="unit">ms</span></div></div>
    <div class="tile"><div class="lbl">Mean HRV</div><div class="val"><span id="mean">--</span><span class="unit">ms</span></div></div>
    <div class="tile"><div class="lbl">Heart rate</div><div class="val"><span id="bpm">--</span><span class="unit">bpm</span></div></div>
    <div class="tile"><div class="lbl">Mean &Delta;HbO&#8322;</div><div class="val"><span id="mo">--</span><span class="unit">uM</span></div></div>
    <div class="tile"><div class="lbl">Mean &Delta;HHb</div><div class="val"><span id="mh">--</span><span class="unit">uM</span></div></div>
    <div class="tile"><div class="lbl">Mean &Delta;HbT</div><div class="val"><span id="mt">--</span><span class="unit">uM</span></div></div>
    <div class="tile"><div class="lbl">Delta rSO2</div><div class="val"><span id="m">--</span><span class="unit">ratio</span></div></div>
  </div>

  <div class="chart">
    <div class="head">
      <span class="key"><i style="background:var(--hbo)"></i>&Delta;HbO&#8322;</span>
      <span class="key"><i style="background:var(--hhb)"></i>&Delta;HHb</span>
      <span>0.01-0.2 Hz band-pass &middot; 60 s &middot; 6 s/div</span>
      <span class="scale" id="nsc">&plusmn;-- uM</span>
    </div>
    <div class="well"><canvas id="cn"></canvas></div>
  </div>

  <div class="chart">
    <div class="head">
      <span class="key"><i style="background:var(--ecg)"></i>ECG</span>
      <span>raw MAX30003 &middot; 60 s &middot; 6 s/div</span>
      <span class="scale" id="esc">-- counts</span>
    </div>
    <div class="well"><canvas id="ce"></canvas></div>
  </div>

  <div class="bar">
    <div class="side">
      <button onclick="dlNirs()">Download fNIRS CSV</button>
      <button onclick="dlRR()">Download R-R CSV</button>
      <button onclick="clearLog()">Clear log</button>
    </div>
    <div class="transport">
      <div class="ctl">
        <button class="rnd go" onclick="cmd('start capture')" title="Start acquisition"><span class="ic">&#9654;</span></button>
        <span class="cap">Start Acquisition</span>
      </div>
      <div class="ctl">
        <button class="rnd stop" onclick="cmd('stop capture')" title="Stop acquisition"><span class="ic">&#9632;</span></button>
        <span class="cap">Stop Acquisition</span>
      </div>
    </div>
    <span id="count">0 fNIRS rows / 0 R-R rows</span>
  </div>
</main>

<script>
var WIN_S = 60;                                 // plot window [s]
var NWIN = WIN_S * 50, EWIN = WIN_S * 256, LOGCAP = 400000;   // EWIN must match the device's ECG_FS
var pO = [], pH = [], pE = [];                  // plot ring buffers
var logN = [], logR = [];                       // full-resolution logs held on this device
var nIdx = 0, ws = null;                        // nIdx doubles as the shared 50 Hz timebase
var ready = false;                              // device band-pass converged
var RSO_MIN = 0.05;                             // min |mean dHbT| [uM] to divide by

function $(id){ return document.getElementById(id); }

function connect(){
  ws = new WebSocket('ws://' + location.hostname + ':81/');
  ws.onopen = function(){ $('led').className = 'up'; $('link').textContent = location.hostname + ':81 connected'; };
  ws.onclose = function(){ $('led').className = ''; $('link').textContent = 'disconnected - retrying'; setTimeout(connect, 2000); };
  ws.onmessage = onMsg;
}
function cmd(c){ if (ws && ws.readyState == 1) ws.send(c); }

function onMsg(ev){
  var tag = ev.data.substring(0, 3), v = ev.data.substring(3);
  if (tag == 'hrv'){ $('hrv').textContent = v; }
  else if (tag == 'avg'){ $('mean').textContent = v; }
  else if (tag == 'rdy'){ ready = (v == '1'); }
  else if (tag == 'nir'){
    var f = v.split(';');
    for (var i = 0; i < f.length; i++){
      var p = f[i].split(',');
      var o = parseFloat(p[0]), h = parseFloat(p[1]);
      pO.push(o); pH.push(h);
      if (pO.length > NWIN){ pO.shift(); pH.shift(); }
      if (logN.length < LOGCAP) logN.push([(nIdx / 50).toFixed(2), o.toFixed(4), h.toFixed(4)]);
      nIdx++;
    }
    updCount();
  }
  else if (tag == 'ecg'){
    var s = v.split(',');
    for (var j = 0; j < s.length; j++) pE.push(parseInt(s[j], 10));
    while (pE.length > EWIN) pE.shift();
  }
  else if (tag == 'rri'){
    // "frameIdx,ms" per entry: frameIdx is the device's fNIRS sample clock, so
    // the logged time lines up with the exact nir sample the beat coincided with.
    var pairs = v.split(';');
    for (var k = 0; k < pairs.length; k++){
      var p = pairs[k].split(',');
      var idx = parseInt(p[0], 10), ms = parseInt(p[1], 10);
      if (logR.length < LOGCAP) logR.push([(idx / 50).toFixed(2), ms]);
      $('bpm').textContent = (60000 / ms).toFixed(0);
    }
    updCount();
  }
}

function updCount(){ $('count').textContent = logN.length + ' fNIRS rows / ' + logR.length + ' R-R rows'; }

// ---- strip charts -------------------------------------------------------
function fit(c){
  var r = window.devicePixelRatio || 1;
  c.width = c.clientWidth * r; c.height = c.clientHeight * r;
  var x = c.getContext('2d'); x.setTransform(r, 0, 0, r, 0, 0);
  return x;
}
function grid(x, w, h){
  x.clearRect(0, 0, w, h);
  var i, j, gx, gy;
  // minor: 0.2 s columns against the 10 s window, 16 rows
  x.strokeStyle = 'rgba(200,220,255,.075)'; x.lineWidth = 1; x.beginPath();
  for (i = 1; i < 50; i++){ gx = Math.round(w * i / 50) + .5; x.moveTo(gx, 0); x.lineTo(gx, h); }
  for (j = 1; j < 16; j++){ gy = Math.round(h * j / 16) + .5; x.moveTo(0, gy); x.lineTo(w, gy); }
  x.stroke();
  // major: 1 s columns, quarter-scale rows
  x.strokeStyle = 'rgba(138,164,255,.26)'; x.beginPath();
  for (i = 1; i < 10; i++){ gx = Math.round(w * i / 10) + .5; x.moveTo(gx, 0); x.lineTo(gx, h); }
  for (j = 1; j < 4; j++){ gy = Math.round(h * j / 4) + .5; x.moveTo(0, gy); x.lineTo(w, gy); }
  x.stroke();
  // zero / mid-scale reference
  x.strokeStyle = 'rgba(138,164,255,.5)'; x.lineWidth = 1.2; x.beginPath();
  x.moveTo(0, h / 2 + .5); x.lineTo(w, h / 2 + .5); x.stroke();
}
function trace(x, d, w, h, n, lo, hi, col){
  if (d.length < 2) return;
  x.strokeStyle = col; x.lineWidth = 1.6; x.lineJoin = 'round';
  x.shadowColor = col; x.shadowBlur = 6;   // soft emissive edge to match the raised surfaces
  x.beginPath();
  var sp = hi - lo || 1;
  for (var i = 0; i < d.length; i++){
    var px = (i / (n - 1)) * w;
    var py = h - ((d[i] - lo) / sp) * h;
    if (i) x.lineTo(px, py); else x.moveTo(px, py);
  }
  x.stroke();
  x.shadowBlur = 0;
}
function draw(){
  var cn = $('cn'), ce = $('ce');
  var xn = fit(cn), wn = cn.clientWidth, hn = cn.clientHeight;
  grid(xn, wn, hn);
  var m = 0.5;   // floor so a flat trace does not get amplified into noise
  var sO = 0, sH = 0;
  for (var i = 0; i < pO.length; i++){
    m = Math.max(m, Math.abs(pO[i]), Math.abs(pH[i]));
    sO += pO[i]; sH += pH[i];
  }
  if (pO.length){
    var mO = sO / pO.length, mH = sH / pH.length, mT = mO + mH;
    $('mo').textContent = mO.toFixed(3);
    $('mh').textContent = mH.toFixed(3);
    $('mt').textContent = mT.toFixed(3);
    // delta rSO2 = mean(dHbO2) / mean(dHbT) over the displayed window
    var txt = !ready ? 'settling...' : (Math.abs(mT) < RSO_MIN ? '--' : (mO / mT).toFixed(2));
    $('m').textContent = txt;
    $('m').nextElementSibling.style.display = isNaN(parseFloat(txt)) ? 'none' : '';
  }
  trace(xn, pO, wn, hn, NWIN, -m, m, '#ff7a7a');
  trace(xn, pH, wn, hn, NWIN, -m, m, '#4fd2ff');
  $('nsc').textContent = '\u00b1' + m.toFixed(2) + ' uM';

  var xe = fit(ce), we = ce.clientWidth, he = ce.clientHeight;
  grid(xe, we, he);
  if (pE.length > 1){
    var lo = Infinity, hi = -Infinity;
    for (var j = 0; j < pE.length; j++){ if (pE[j] < lo) lo = pE[j]; if (pE[j] > hi) hi = pE[j]; }
    var pad = (hi - lo) * 0.1 || 1;
    trace(xe, pE, we, he, EWIN, lo - pad, hi + pad, '#5ce894');
    $('esc').textContent = (hi - lo) + ' counts p-p';
  }
  requestAnimationFrame(draw);
}

// ---- logging to this device --------------------------------------------
function save(name, header, rows){
  if (!rows.length){ alert('Nothing logged yet.'); return; }
  var out = header + '\n';
  for (var i = 0; i < rows.length; i++) out += rows[i].join(',') + '\n';
  var a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([out], {type: 'text/csv'}));
  a.download = name;
  a.click();
  URL.revokeObjectURL(a.href);
}
function stamp(){ var d = new Date(); return d.toISOString().replace(/[:.]/g, '-').slice(0, 19); }
function dlNirs(){ save('fnirs_' + stamp() + '.csv', 't_s,dHbO2_uM,dHHb_uM', logN); }
function dlRR(){ save('rr_' + stamp() + '.csv', 't_s,rr_ms', logR); }
function clearLog(){ logN = []; logR = []; nIdx = 0; updCount(); }

connect();
draw();
</script>
</body>
</html>
)=====";

//required function for Async Web Server library 
void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Page Not found");
}

//handle websocket events (data recieved)
void EventsHandler(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED: {
            break;
        }
        case WStype_DISCONNECTED: {
            break;
        }
        //When button is pressed, client side sends message through the established socket connection. This checks whether the message is to start capturing data or stop capturing data
        case WStype_TEXT: {
            String msg = String((char*)payload);
            msg.trim();
            msg.toLowerCase();   // the button labels and the protocol must not have to agree on case
            if (msg == "start capture") {
                readData = true;
            }
            if (msg == "stop capture") {
                readData = false;
            }
        }
    }
}

void setupLED() {
    ledChip.Begin();
    ledChip.Enable();
    Wire.setClock(400000);   // 8 PWM writes per frame have to fit the 20 ms budget
    delay(500);
}

// Read fnirs samples
float readAvg(int pin, int samples = FNIRS_AVG) {
    long sum = 0;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(20);
    }
    return (float)sum / samples;
}
/*
 * Delta rSO2 Calculator using NIRS (Near-Infrared Spectroscopy)
 * Wavelengths : 740 nm and 850 nm
 * Short channel: 1.0 cm (removes superficial / skin layer signal)
 * Long  channel: 3.3 cm (measures deep tissue signal)
 *
 * Method: Modified Beer-Lambert Law + short-channel regression subtraction
 */

const float SHORT_SDS = 1.0;   // Source-detector separation, short channel (cm)
const float LONG_SDS  = 3.3;   // Source-detector separation, long  channel (cm)

/*
 * Differential Pathlength Factor (DPF)
 * Typical values: 6.26 @ 740 nm, 5.97 @ 850 nm (adult head, literature values)
 * Adjust these for your tissue type and age group.
 */
const float DPF_740 = 6.26;
const float DPF_850 = 5.97;

/*
 * Molar extinction coefficients [cm^-1 / mM]
 * Source: Matcher et al. / Prahl tabulated values at 740 & 850 nm
 */
const float E_HbO2_740 = 0.446;   // Oxyhaemoglobin   @ 740 nm
const float E_Hb_740   = 1.116;   // Deoxyhaemoglobin @ 740 nm
const float E_HbO2_850 = 1.058;   // Oxyhaemoglobin   @ 850 nm
const float E_Hb_850   = 0.691;   // Deoxyhaemoglobin @ 850 nm

// ─── Globals ────────────────────────────────────────────────────────────────
// Reference intensities, one per detector reading produced by readFNIRS()
float baseline_LS_740, baseline_LS_850;   // Left  short channel
float baseline_RS_740, baseline_RS_850;   // Right short channel
float baseline_LL_740, baseline_LL_850;   // Left  long  channel
float baseline_RL_740, baseline_RL_850;   // Right long  channel

// read photo detector sequence
void readFNIRS() {
    // Start the LED sequence
    // SHORT CHANNEL
    for (int j = 0; j < 4; j++) {
        // 740nm L, 850nm L, 740nm R, 850nm R
        ledChip.SetChannelPWM(j, 200);
        if (j == 0) {
            det_LS_740 = readAvg(0);
        } else if (j == 1) {
            det_LS_850 = readAvg(1);
        } else if (j == 2) {
            det_RS_740 = readAvg(0);
        } else if (j == 3) {
            det_RS_850 = readAvg(1);
        }
        delayMicroseconds(LED_SETTLE_US);
        ledChip.SetChannelPWM(j,0);
    }
    // LONG CHANNEL
    for (int j = 0; j < 4; j++) {
        // 740nm L, 850nm L, 740nm R, 850nm R
        ledChip.SetChannelPWM(j, 200);
        if (j == 0) {
            det_LL_740 = readAvg(2);
        } else if (j == 1) {
            det_LL_850 = readAvg(3);
        } else if (j == 2) {
            det_RL_740 = readAvg(2);
        } else if (j == 3) {
            det_RL_850 = readAvg(3);
        }
        delayMicroseconds(LED_SETTLE_US);
        ledChip.SetChannelPWM(j,0);
    }
}

/*
 * Capture the reference intensities by averaging whole acquisition frames.
 * Driving the identical LED/detector sequence as the measurement loop is what
 * makes ln(baseline / current) valid: per-channel gain, LED output and ambient
 * offset are then common to both terms and cancel.
 */
void captureRestingBaseline(uint16_t frames) {
    double sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    for (uint16_t i = 0; i < frames; i++) {
        uint32_t start = micros();
        readFNIRS();
        sum[0] += det_LS_740;  sum[1] += det_LS_850;
        sum[2] += det_RS_740;  sum[3] += det_RS_850;
        sum[4] += det_LL_740;  sum[5] += det_LL_850;
        sum[6] += det_RL_740;  sum[7] += det_RL_850;
        while ((int32_t)(micros() - start) < (int32_t)FNIRS_PERIOD_US) yield();   // same cadence as loop()
    }

    float avg[8];
    for (int k = 0; k < 8; k++) {
        avg[k] = (float)(sum[k] / frames);
        if (avg[k] < 1.0f) {
            avg[k] = 1.0f;   // a dark channel would make ln(baseline / current) infinite
            Serial.print("WARNING: fNIRS baseline channel ");
            Serial.print(k);
            Serial.println(" is dark - check LED drive and detector wiring.");
        }
    }

    baseline_LS_740 = avg[0];  baseline_LS_850 = avg[1];
    baseline_RS_740 = avg[2];  baseline_RS_850 = avg[3];
    baseline_LL_740 = avg[4];  baseline_LL_850 = avg[5];
    baseline_RL_740 = avg[6];  baseline_RL_850 = avg[7];
}

// Solve the modified Beer-Lambert law for the two chromophore concentrations.
// delta rSO2 is not formed here: it is the ratio of the window MEANS of the two
// band-passed traces, so it is computed once, where those means are taken.

void calculateHb () {
// Guard against zero / negative readings
    if (det_LS_740 < 1 || det_LS_850 < 1 || det_LL_740 < 1 || det_LL_850 < 1 ||
        det_RS_740 < 1 || det_RS_850 < 1 || det_RL_740 < 1 || det_RL_850 < 1) {
        //Runs inside the 20 ms frame, so report without blocking and hold the last values.
        Serial.println("ERROR: Sensor reading near zero - check connections.");
        return;
    }

    // 1. Compute ΔOD (change in optical density) per channel
    //    ΔOD = ln(I_baseline / I_current)
    float dOD_s740 = log(baseline_LS_740 / det_LS_740);
    float dOD_s850 = log(baseline_LS_850 / det_LS_850);
    float dOD_l740 = log(baseline_LL_740 / det_LL_740);
    float dOD_l850 = log(baseline_LL_850 / det_LL_850);

    // 2. Short-channel regression subtraction
    //    Removes scalp/skull (superficial) haemodynamic contributions
    //    Simple coefficient β = SHORT_SDS / LONG_SDS (linear scaling)
    float beta = SHORT_SDS / LONG_SDS;
    float dOD_740 = dOD_l740 - beta * dOD_s740;
    float dOD_850 = dOD_l850 - beta * dOD_s850;

    // 3. Effective pathlength for long channel [cm]
    //    L = SDS * DPF
    float L_740 = LONG_SDS * DPF_740;
    float L_850 = LONG_SDS * DPF_850;

    // 4. Solve modified Beer-Lambert for Δ[HbO₂] and Δ[Hb]
    //    | E_HbO2_740 * L_740   E_Hb_740 * L_740 |   | dHbO |   | dOD_740 |
    //    | E_HbO2_850 * L_850   E_Hb_850 * L_850 | × | dHb  | = | dOD_850 |
    float A = E_HbO2_740 * L_740;
    float B = E_Hb_740   * L_740;
    float C = E_HbO2_850 * L_850;
    float D = E_Hb_850   * L_850;

    float det = A * D - B * C;
    if (abs(det) < 1e-9) {
        Serial.println("ERROR: Singular matrix - check extinction coefficients.");
        return;
    }

    // Concentrations in mM; convert to µM (* 1000)
    dHbO2_raw = ((D * dOD_740 - B * dOD_850) / det) * 1000.0;
    dHb_raw   = ((A * dOD_850 - C * dOD_740) / det) * 1000.0;
}

/*
 * Butterworth band-pass filter: 0.01 Hz - 0.2 Hz
 *
 * Standard pass-band for prefrontal fNIRS haemodynamics. It keeps the task
 * evoked response (period ~5 s to ~100 s) while rejecting
 *   - below 0.01 Hz : baseline / thermal drift and slow trend
 *   - above 0.20 Hz : respiration (~0.2-0.3 Hz), cardiac pulsation (~1 Hz),
 *                     LED and ADC noise
 *
 * A 2nd-order Butterworth high-pass is cascaded with a 2nd-order Butterworth
 * low-pass: 4th order overall, 12 dB/octave on each skirt, maximally flat
 * pass-band. Measured response at fs = 50 Hz: -3.0 dB at both corners,
 * -28 dB at 1 Hz (cardiac) and -40 dB at 2 Hz.
 *
 * Phase is non-linear, so evoked-response onset latency is smeared. If you
 * need exact latency, log the raw signal and filter it forwards and backwards
 * offline.
 *
 * Coefficients are built at run time with the bilinear transform, Q = 1/sqrt(2).
 * Coefficients and state must stay in double: at fc/fs = 2e-4 the high-pass
 * poles sit at radius 0.9991, which float32 cannot resolve without drifting.
 *
 * Written as a class so the Arduino auto-prototype generator cannot hoist a
 * prototype above the type definition.
 */

#define FNIRS_BP_LOW   0.01f   // high-pass corner [Hz]
#define FNIRS_BP_HIGH  0.20f   // low-pass corner [Hz]

class ButterBandpass {
    public:
        void begin(float fs, float fLow, float fHigh) {
            design(hp, fs, fLow,  true);
            design(lp, fs, fHigh, false);
            primed = false;
            n = 0;
            settle = (uint32_t)(3.0 * fs / (2.0 * PI * fLow));   // 3 high-pass time constants
        }

        // Output is only trustworthy once the high-pass transient has decayed (~48 s).
        bool ready() const { return n >= settle; }

        // Push one sample, return the band-passed value (same units as the input).
        float apply(float x) {
            if (!primed) {                               // start from the steady state for x, not from
                primeSection(lp, primeSection(hp, x));     // zero, so there is no step transient at boot
                primed = true;
            }
            if (n < settle) n++;
            return (float)section(lp, section(hp, x));
        }

    private:
        struct Biquad { double b0, b1, b2, a1, a2, z1, z2; };

        static void design(Biquad &s, double fs, double fc, bool highpass) {
            double w  = 2.0 * PI * fc / fs;
            double cw = cos(w);
            double alpha = sin(w) / (2.0 * 0.7071067811865476);   // Q = 1/sqrt(2) -> Butterworth
            double a0 = 1.0 + alpha;
            s.a1 = (-2.0 * cw) / a0;
            s.a2 = (1.0 - alpha) / a0;
            if (highpass) {
                s.b0 = ((1.0 + cw) / 2.0) / a0;
                s.b1 = (-(1.0 + cw)) / a0;
            } else {
                s.b0 = ((1.0 - cw) / 2.0) / a0;
                s.b1 = (1.0 - cw) / a0;
            }
            s.b2 = s.b0;
            s.z1 = 0.0;
            s.z2 = 0.0;
        }

        // Transposed direct form II.
        static double section(Biquad &s, double x) {
            double y = s.b0 * x + s.z1;
            s.z1 = s.b1 * x - s.a1 * y + s.z2;
            s.z2 = s.b2 * x - s.a2 * y;
            return y;
        }

        // Load the delay line with the state a constant input x would settle to.
        static double primeSection(Biquad &s, double x) {
            double y = ((s.b0 + s.b1 + s.b2) / (1.0 + s.a1 + s.a2)) * x;
            s.z2 = s.b2 * x - s.a2 * y;
            s.z1 = s.b1 * x - s.a1 * y + s.z2;
            return y;
        }

        Biquad   hp;
        Biquad   lp;
        uint32_t n;        // samples processed, saturates at settle
        uint32_t settle;   // samples needed before ready()
        bool     primed;
};

ButterBandpass bpHbO2;
ButterBandpass bpHb;

//setup function
void setup() {
    Serial.begin(115200);
    bpHbO2.begin(FNIRS_FS, FNIRS_BP_LOW, FNIRS_BP_HIGH);
    bpHb.begin(FNIRS_FS, FNIRS_BP_LOW, FNIRS_BP_HIGH);
    // Setup LED driver chip
    setupLED();
    analogReadResolution(12); // 12-bit resolution - 0-4095
    analogSetWidth(12);
    analogSetAttenuation(ADC_11db); // ADC_6db gives half attenuation

    // get starting baseline fnirs
    // Averages BASELINE_FRAMES full LED sequences, so the reference intensities
    // line up one-to-one with what readFNIRS() produces during capture.
    captureRestingBaseline(BASELINE_FRAMES);
    /// Baseline done
    //initialize array of rr intervals to prevent memory errors
    for (int i = 0; i < sample_num; i++) {
        rrInt[i] = -1;
    }
    //Initialize serial output for debugging
    //Serial.begin(57600);
    pinMode(SD_CS, OUTPUT); //set pin mode to output to send signals through chip select
    digitalWrite(SD_CS, HIGH); //set write mode to high

    bool ecg_setup = ecg_spi.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); //Open SPI communication with MAX30003

    digitalWrite(SD_CS, LOW); //set write mode to low

    //format/structure data
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);

    //check if device is connected
    bool ret = max30003.readDeviceID(); 
  
    if(ret){
        Serial.println("Max30003 read ID Success");
    }else{

        while(!ret){
            ret = max30003.readDeviceID();
            Serial.println("Failed to read ID, please make sure all the pins are connected");
            delay(10000);
        }
    }
  
    Serial.println("Initializing the chip ...");
    max30003.begin();
    max30003.setSamplingRate(SR_256);   // must match ECG_FS
  
    WiFi.softAP("cte_monitor", "1234"); //Initialize web server access point/wifi network

    MDNS.begin("Anirudh"); //start dns

    //send the HTML for the website to be displayed client-side if the user accesses the home page of the web server
    website.on("/", [](AsyncWebServerRequest* request) {
        String message = "Anirudh's FNIRS_ECG Monitor";
        request->send_P(200, "text/html", webpage);
    });
    website.begin(); //Start web server
    socket.begin(); //Start socket for communication with client
    socket.onEvent(EventsHandler); //Set event handler function
}

//Loop function (runs every CPU cycle)
void loop() {
    socket.loop(); //Update socket data

    static uint32_t nextFrame = 0;   //micros() deadline of the next fNIRS frame
    static uint32_t lastPush = 0;
    static bool running = false;

    //If the user has asked to capture data
    if (!readData) {
        running = false;
        return;
    }

    //Hold a fixed FNIRS_FS frame rate. delay() alone would give a period of
    //20 ms PLUS the acquisition time.
    uint32_t now = micros();
    if (!running) {
        running = true;
        nextFrame = now;
    }
    if ((int32_t)(now - nextFrame) < 0)
        return;
    nextFrame += FNIRS_PERIOD_US;
    if ((int32_t)(micros() - nextFrame) > 0) {
        nextFrame = micros() + FNIRS_PERIOD_US;   //frame overran: resync instead of burst-catching up
    }

    //Shared clock for this pass: the R-R check below and the fNIRS frame
    //acquired further down are tagged with the same index, so the logged R-R
    //time lines up exactly with the optical sample it coincided with.
    uint32_t curFrame = fnirsFrameIdx++;

    max30003.updateHeartRate(); //update MAX30003 sensor calculations
    uint16_t rr = max30003.rrInterval() * volt_bias; //Capture R-R interval
    if (rr <= 1500 && rr >= 500) { //Check if data is noise
        if (cRrInt != rr) { //Check if the data is the same as a previous sample. Likelihood is high, as the sampling rate is faster than the heartbeat
            cRrInt = rr; //Change the preivous sample to the current in order to check the next sample
            rrInt[currentRR] = rr; //Add R-R data to array for calculating HRV
            currentRR += 1; //Increase counter of R-R data
            if (rrCount < RR_BATCH) {
                rrBuf[rrCount] = rr;
                rrFrameBuf[rrCount] = curFrame;   // tag with the fNIRS frame this heartbeat coincided with
                rrCount++;
            }
        }
    }

    //Drain whatever the ECG FIFO has accumulated since the last frame
    //(ECG_FS against a 50 Hz frame rate is ~ECG_FS/50 samples).
    int32_t ecgSample;
    for (uint8_t i = 0; i < ECG_DRAIN; i++) {
        if (!max30003.readEcgSample(ecgSample)) break;
        if (ecgCount < ECG_BATCH) ecgBuf[ecgCount++] = ecgSample;
    }

    //Optical sampling is independent of the ECG: the band-pass expects one
    //fNIRS frame every FNIRS_PERIOD_US, not one per detected heartbeat.
    readFNIRS();
    calculateHb();
    dHbO2_filt = bpHbO2.apply(dHbO2_raw);
    dHb_filt   = bpHb.apply(dHb_raw);
    if (nirCount < NIR_BATCH) {
        nirO[nirCount] = dHbO2_filt;
        nirH[nirCount] = dHb_filt;
        nirCount++;
    }

    if (currentRR == sample_num) { //If the requisite number of R-R samples have been captured
        //Calculate RMSSD (root mean squared of R-R samples)
        int rmssdSum = 0;
        for (int i = 0; i < (sample_num - 1); i++) {
            int hrv_difference = abs(rrInt[i+1] - rrInt[i]);
            rmssdSum += sq(hrv_difference);
        }
        rmssd = sqrt((double)rmssdSum * (1.0/((double)(sample_num - 1))));
        numHRV += 1;
        meanHRV = ((meanHRV * (double)(numHRV - 1)) + rmssd)/(double)numHRV;
        currentRR = 0;
    }

    //Convert values to strings in order to send to the client(website) to be displayed.
    //Each message is a 3-character tag plus the payload; the client strips exactly 3.
    if (millis() - lastPush >= PUSH_PERIOD_MS) {
        lastPush = millis();
        rmssd_string = "hrv" + String(rmssd);
        meanHRV_string = "avg" + String(meanHRV);
        socket.broadcastTXT(rmssd_string);
        socket.broadcastTXT(meanHRV_string);
        socket.broadcastTXT(bpHbO2.ready() ? "rdy1" : "rdy0");   // high-pass converged?

        //Waveform batches. reserve() first: growing a String inside the loop
        //fragments the heap badly at this rate.
        if (nirCount) {
            String m = "nir";
            m.reserve(16 * NIR_BATCH);
            for (uint8_t i = 0; i < nirCount; i++) {
                if (i) m += ';';
                m += String(nirO[i], 3);
                m += ',';
                m += String(nirH[i], 3);
            }
            socket.broadcastTXT(m);
            nirCount = 0;
        }
        if (ecgCount) {
            String m = "ecg";
            m.reserve(10 * ECG_BATCH);
            for (uint8_t i = 0; i < ecgCount; i++) {
                if (i) m += ',';
                m += String(ecgBuf[i]);
            }
            socket.broadcastTXT(m);
            ecgCount = 0;
        }
        if (rrCount) {
            String m = "rri";
            m.reserve(16 * RR_BATCH);
            for (uint8_t i = 0; i < rrCount; i++) {
                if (i) m += ';';
                m += String(rrFrameBuf[i]);
                m += ',';
                m += String(rrBuf[i]);
            }
            socket.broadcastTXT(m);
            rrCount = 0;
        }
    }
} // End loop
