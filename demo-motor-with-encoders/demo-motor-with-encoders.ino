/*
 * ESP32 Dual Motor Controller with Optical Encoders
 *
 * No extra libraries needed — uses only built-in WiFi + WebServer.
 * Web UI: http://<esp32-ip> (check Serial Monitor after connecting)
 *
 * Wiring:
 *   Motor A: ENA=22, IN1=21, IN2=19
 *   Motor B: ENB=23, IN3=18, IN4=5
 *   Encoder A D output -> pin 16
 *   Encoder B D output -> pin 17
 */

#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "elquesabesabe";
const char* password = "yelquenovaalauade";

// Motor driver pins
const int PIN_ENA = 22;
const int PIN_IN1 = 21;
const int PIN_IN2 = 19;
const int PIN_IN3 = 18;
const int PIN_IN4 = 5;
const int PIN_ENB = 23;

// Optical encoder D output pins
const int PIN_ENC_A = 16;
const int PIN_ENC_B = 17;

// Encoder wheel slots (pulses per revolution)
const float PULSES_PER_REV = 20.0;

// Motor speed (0-255)
int speedA = 128;
int speedB = 128;

// Encoder counts
volatile long encCountA = 0;
volatile long encCountB = 0;

// RPM calculation
unsigned long prevTime = 0;
volatile long prevCountA = 0;
volatile long prevCountB = 0;
float rpmA = 0;
float rpmB = 0;

WebServer server(80);

// === MOTOR CONTROL ===

void motorA(int dir, int spd) {
  if (dir == 0) {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
  } else if (dir > 0) {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
  } else {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
  }
  analogWrite(PIN_ENA, spd);
}

void motorB(int dir, int spd) {
  if (dir == 0) {
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
  } else if (dir > 0) {
    digitalWrite(PIN_IN3, HIGH);
    digitalWrite(PIN_IN4, LOW);
  } else {
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, HIGH);
  }
  analogWrite(PIN_ENB, spd);
}

// === ENCODER INTERRUPTS ===

void IRAM_ATTR encA_isr() { encCountA++; }
void IRAM_ATTR encB_isr() { encCountB++; }

// === HTML ===

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>Motor Control</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;padding:16px}
    h1{text-align:center;font-size:1.4rem;margin-bottom:16px;color:#e94560}
    .card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:16px}
    .card h2{font-size:1.1rem;margin-bottom:12px;color:#0f3460}
    .btn-row{display:flex;gap:8px;margin-bottom:10px}
    .btn{flex:1;padding:14px;border:none;border-radius:8px;font-size:1rem;font-weight:600;cursor:pointer;-webkit-user-select:none;user-select:none;touch-action:manipulation}
    .btn:active{opacity:0.7}
    .fwd{background:#4ecca3;color:#1a1a2e}
    .bwd{background:#e94560;color:#fff}
    .stop{background:#555;color:#fff;flex:0.5}
    .slider-wrap{margin:10px 0}
    .slider-wrap label{display:flex;justify-content:space-between;font-size:0.85rem;color:#aaa}
    input[type=range]{width:100%;accent-color:#4ecca3}
    .data-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
    .data-item{background:#0f3460;border-radius:6px;padding:8px;text-align:center}
    .data-item .label{font-size:0.7rem;color:#888;text-transform:uppercase}
    .data-item .val{font-size:1.3rem;font-weight:700;margin-top:2px}
    .status{text-align:center;font-size:0.8rem;color:#4ecca3;margin-top:8px}
  </style>
</head>
<body>

<h1>Motor Control</h1>

<div class="card">
  <h2>Motor A</h2>
  <div class="btn-row">
    <button class="btn fwd" data-motor="A" data-dir="fwd">FWD</button>
    <button class="btn bwd" data-motor="A" data-dir="bwd">BWD</button>
    <button class="btn stop" data-motor="A" data-dir="stop">STOP</button>
  </div>
  <div class="slider-wrap">
    <label><span>Speed</span><span id="spdA">128</span></label>
    <input type="range" min="0" max="255" value="128" id="sliderA" data-motor="A">
  </div>
  <div class="data-grid">
    <div class="data-item"><div class="label">Position</div><div class="val" id="posA">0</div></div>
    <div class="data-item"><div class="label">RPM</div><div class="val" id="rpmA">0.0</div></div>
  </div>
</div>

<div class="card">
  <h2>Motor B</h2>
  <div class="btn-row">
    <button class="btn fwd" data-motor="B" data-dir="fwd">FWD</button>
    <button class="btn bwd" data-motor="B" data-dir="bwd">BWD</button>
    <button class="btn stop" data-motor="B" data-dir="stop">STOP</button>
  </div>
  <div class="slider-wrap">
    <label><span>Speed</span><span id="spdB">128</span></label>
    <input type="range" min="0" max="255" value="128" id="sliderB" data-motor="B">
  </div>
  <div class="data-grid">
    <div class="data-item"><div class="label">Position</div><div class="val" id="posB">0</div></div>
    <div class="data-item"><div class="label">RPM</div><div class="val" id="rpmB">0.0</div></div>
  </div>
</div>

<div class="status" id="status">Connecting...</div>

<script>
var speed = { A: 128, B: 128 };

function motorCmd(m, d) {
  fetch('/motor?m=' + m + '&d=' + d + '&s=' + speed[m]);
}

document.querySelectorAll('.btn').forEach(function(btn) {
  var m = btn.dataset.motor, d = btn.dataset.dir;
  if (d === 'stop') {
    btn.addEventListener('click', function() { motorCmd(m, 'stop'); });
  } else {
    btn.addEventListener('mousedown', function() { motorCmd(m, d); });
    btn.addEventListener('mouseup',   function() { motorCmd(m, 'stop'); });
    btn.addEventListener('mouseleave', function() { motorCmd(m, 'stop'); });
    btn.addEventListener('touchstart',  function(e) { e.preventDefault(); motorCmd(m, d); });
    btn.addEventListener('touchend',    function(e) { e.preventDefault(); motorCmd(m, 'stop'); });
    btn.addEventListener('touchcancel', function(e) { e.preventDefault(); motorCmd(m, 'stop'); });
  }
});

document.querySelectorAll('input[type=range]').forEach(function(sl) {
  sl.addEventListener('input', function() {
    var m = this.dataset.motor;
    speed[m] = parseInt(this.value);
    document.getElementById('spd' + m).textContent = speed[m];
  });
});

function poll() {
  fetch('/data').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('posA').textContent = d.ea;
    document.getElementById('posB').textContent = d.eb;
    document.getElementById('rpmA').textContent = d.ra.toFixed(1);
    document.getElementById('rpmB').textContent = d.rb.toFixed(1);
    document.getElementById('status').textContent = 'Connected';
  }).catch(function() {
    document.getElementById('status').textContent = 'Waiting...';
  });
}
setInterval(poll, 200);
poll();
</script>
</body>
</html>
)rawliteral";

// === HTTP HANDLERS ===

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleMotor() {
  if (!server.hasArg("m") || !server.hasArg("d")) {
    server.send(400, "text/plain", "Missing params");
    return;
  }
  String m = server.arg("m");
  String d = server.arg("d");

  if (m == "A") {
    if (d == "fwd") motorA(1, speedA);
    else if (d == "bwd") motorA(-1, speedA);
    else motorA(0, 0);
  } else if (m == "B") {
    if (d == "fwd") motorB(1, speedB);
    else if (d == "bwd") motorB(-1, speedB);
    else motorB(0, 0);
  }

  // Optional speed override
  if (server.hasArg("s")) {
    int s = constrain(server.arg("s").toInt(), 0, 255);
    if (m == "A") speedA = s;
    else speedB = s;
  }

  server.send(200, "text/plain", "OK");
}

void handleData() {
  String json = "{\"ea\":";
  json += String((long)encCountA);
  json += ",\"eb\":";
  json += String((long)encCountB);
  json += ",\"ra\":";
  json += String(rpmA, 1);
  json += ",\"rb\":";
  json += String(rpmB, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encA_isr, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encB_isr, RISING);

  motorA(0, 0);
  motorB(0, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/motor", handleMotor);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();

  prevTime = millis();
}

// === LOOP ===

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - prevTime >= 200) {
    float dt = (now - prevTime) / 1000.0;

    noInterrupts();
    long ca = encCountA;
    long cb = encCountB;
    interrupts();

    rpmA = ((ca - prevCountA) / PULSES_PER_REV) / dt * 60.0;
    rpmB = ((cb - prevCountB) / PULSES_PER_REV) / dt * 60.0;

    prevCountA = ca;
    prevCountB = cb;
    prevTime = now;
  }
}
