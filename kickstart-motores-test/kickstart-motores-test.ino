#include <WiFi.h>
#include <WebServer.h>

// --- Wi-Fi Configuration ---
const char* ssid = "elquesabesabe";
const char* password = "yelquenovaalauade";

WebServer server(80);

// --- User-Defined GPIO Pin Mapping ---
const int PIN_ENA = 22; 
const int PIN_IN1 = 21; 
const int PIN_IN2 = 19; 
const int PIN_IN3 = 18; 
const int PIN_IN4 = 5;  
const int PIN_ENB = 23; 

// --- PWM Properties ---
const int pwmFreq = 5000;
const int pwmResolution = 8; 

// --- Motor State & Control Variables ---
int targetPWMA = 0;      
int targetPWMB = 0;      
int startingPWM = 180;   
int kickstartMs = 200;   
bool isRunning = false;  

int activePWMA = 0;       
int activePWMB = 0;       

unsigned long kickstartStartTime = 0;
bool isKickstarting = false;

// --- HTML / Webpage UI ---
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Balanced Motor Control</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 30px auto; max-width: 500px; background: #f4f4f9; color: #333; }
        .card { background: white; padding: 25px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        h1 { color: #0056b3; margin-bottom: 5px; }
        .status-panel { font-weight: bold; margin-bottom: 20px; font-size: 1.1em; }
        .status-on { color: #28a745; }
        .status-off { color: #dc3545; }
        .slider-container { margin: 20px 0; text-align: left; }
        .master-label { color: #0056b3; }
        .motor-a-label { color: #e67e22; }
        .motor-b-label { color: #9b59b6; }
        label { font-weight: bold; display: block; margin-bottom: 5px; }
        input[type=range] { width: 100%; height: 10px; border-radius: 5px; background: #ddd; outline: none; }
        .value-display { font-weight: bold; color: #555; float: right; }
        .btn-container { display: flex; gap: 15px; justify-content: center; margin-top: 25px; }
        button { padding: 12px 30px; font-size: 16px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: background 0.2s; width: 45%; }
        .btn-start { background-color: #28a745; color: white; }
        .btn-start:hover { background-color: #218838; }
        .btn-stop { background-color: #dc3545; color: white; }
        .btn-stop:hover { background-color: #c82333; }
        hr { border: 0; height: 1px; background: #eee; margin: 20px 0; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Dual Motor Balancer</h1>
        <div class="status-panel">System Status: <span id="statusLabel" class="status-off">STOPPED</span></div>
        <hr>
        
        <div class="slider-container">
            <label class="master-label">MASTER SPEED (Both Motors): <span id="masterVal" class="value-display">0</span></label>
            <input type="range" id="masterSlider" min="0" max="255" value="0" oninput="updateMaster(this.value)">
        </div>
        
        <hr>

        <div class="slider-container">
            <label class="motor-a-label">Motor A Fine Tune (PWM): <span id="targetValA" class="value-display">0</span></label>
            <input type="range" id="targetSliderA" min="0" max="255" value="0" onchange="updateValues()">
        </div>

        <div class="slider-container">
            <label class="motor-b-label">Motor B Fine Tune (PWM): <span id="targetValB" class="value-display">0</span></label>
            <input type="range" id="targetSliderB" min="0" max="255" value="0" onchange="updateValues()">
        </div>

        <hr>

        <div class="slider-container">
            <label>Kickstart Minimum PWM: <span id="startVal" class="value-display">180</span></label>
            <input type="range" id="startSlider" min="100" max="255" value="180" onchange="updateValues()">
        </div>

        <div class="slider-container">
            <label>Kickstart Duration (ms): <span id="durationVal" class="value-display">200</span></label>
            <input type="range" id="durationSlider" min="50" max="1000" step="50" value="200" onchange="updateValues()">
        </div>

        <div class="btn-container">
            <button class="btn-start" onclick="sendAction('start')">START</button>
            <button class="btn-stop" onclick="sendAction('stop')">STOP</button>
        </div>
    </div>

    <script>
        // Adjusts both motor sliders to follow the master slider value
        function updateMaster(val) {
            document.getElementById("masterVal").innerText = val;
            document.getElementById("targetSliderA").value = val;
            document.getElementById("targetSliderB").value = val;
            updateValues();
        }

        function updateValues() {
            let targetA = document.getElementById("targetSliderA").value;
            let targetB = document.getElementById("targetSliderB").value;
            let start = document.getElementById("startSlider").value;
            let duration = document.getElementById("durationSlider").value;

            document.getElementById("targetValA").innerText = targetA;
            document.getElementById("targetValB").innerText = targetB;
            document.getElementById("startVal").innerText = start;
            document.getElementById("durationVal").innerText = duration;

            let xhr = new XMLHttpRequest();
            xhr.open("GET", `/update?targetA=${targetA}&targetB=${targetB}&start=${start}&duration=${duration}`, true);
            xhr.send();
        }

        function sendAction(action) {
            let xhr = new XMLHttpRequest();
            xhr.open("GET", `/action?state=${action}`, true);
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 4 && xhr.status == 200) {
                    let label = document.getElementById("statusLabel");
                    if (action === 'start') {
                        label.innerText = "RUNNING";
                        label.className = "status-on";
                    } else {
                        label.innerText = "STOPPED";
                        label.className = "status-off";
                    }
                }
            };
            xhr.send();
        }

        window.onload = function() { updateValues(); }
    </script>
</body>
</html>
)rawliteral";

void applyMotorOutputs() {
  // Motor A Execution State
  if (!isRunning || targetPWMA == 0) {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    ledcWrite(PIN_ENA, 0);
  } else {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
    ledcWrite(PIN_ENA, activePWMA);
  }

  // Motor B Execution State
  if (!isRunning || targetPWMB == 0) {
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, LOW);
    ledcWrite(PIN_ENB, 0);
  } else {
    digitalWrite(PIN_IN3, HIGH);
    digitalWrite(PIN_IN4, LOW);
    ledcWrite(PIN_ENB, activePWMB);
  }
}

void handleRoot() {
  server.send(200, "text/html", HTML_PAGE);
}

void handleUpdate() {
  if (server.hasArg("targetA") && server.hasArg("targetB") && server.hasArg("start") && server.hasArg("duration")) {
    int newTargetA = server.arg("targetA").toInt();
    int newTargetB = server.arg("targetB").toInt();
    startingPWM = server.arg("start").toInt();
    kickstartMs = server.arg("duration").toInt();

    // Trigger kickstart if either motor is turning on from a resting threshold
    bool kickstartNeeded = (isRunning && 
                           ((newTargetA > 0 && targetPWMA == 0 && newTargetA < startingPWM) || 
                            (newTargetB > 0 && targetPWMB == 0 && newTargetB < startingPWM)));

    if (kickstartNeeded) {
      isKickstarting = true;
      kickstartStartTime = millis();
      activePWMA = (newTargetA > 0) ? startingPWM : 0;
      activePWMB = (newTargetB > 0) ? startingPWM : 0;
    } else if (!isKickstarting) {
      activePWMA = newTargetA;
      activePWMB = newTargetB;
    }

    targetPWMA = newTargetA;
    targetPWMB = newTargetB;
    
    applyMotorOutputs();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleAction() {
  if (server.hasArg("state")) {
    String state = server.arg("state");
    
    if (state == "start") {
      if (!isRunning) {
        isRunning = true;
        if ((targetPWMA > 0 && targetPWMA < startingPWM) || (targetPWMB > 0 && targetPWMB < startingPWM)) {
          isKickstarting = true;
          kickstartStartTime = millis();
          activePWMA = (targetPWMA > 0) ? startingPWM : 0;
          activePWMB = (targetPWMB > 0) ? startingPWM : 0;
        } else {
          activePWMA = targetPWMA;
          activePWMB = targetPWMB;
        }
      }
    } else if (state == "stop") {
      isRunning = false;
      isKickstarting = false;
    }

    applyMotorOutputs();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  
  ledcAttach(PIN_ENA, pwmFreq, pwmResolution);
  ledcAttach(PIN_ENB, pwmFreq, pwmResolution);

  applyMotorOutputs();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.on("/action", handleAction);
  server.begin();
}

void loop() {
  server.handleClient();

  if (isKickstarting && (millis() - kickstartStartTime >= (unsigned long)kickstartMs)) {
    isKickstarting = false;
    if (isRunning) {
      activePWMA = targetPWMA;
      activePWMB = targetPWMB;
      
      ledcWrite(PIN_ENA, activePWMA);
      ledcWrite(PIN_ENB, activePWMB);
      
      Serial.println("Kickstart complete. Reverted to master/fine-tuned balanced targets.");
    }
  }
}