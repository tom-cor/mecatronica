#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
//tomi rulez

// --- Configuración de Red WiFi ---
const char* ssid     = "elquesabesabe";       
const char* password = "yelquenovaalauade";   

// --- Asignación de Pines del Brazo ---
const int PIN_SERVO_A = 13; 
const int PIN_SERVO_B = 12; 
const int PIN_SERVO_C = 14; 
const int PIN_SERVO_D = 27; 

const int ENCODER_HOMBRO_CLK = 32;
const int ENCODER_HOMBRO_DT  = 33;
const int ENCODER_CODO_CLK   = 25;
const int ENCODER_CODO_DT    = 26;

// --- Asignación de Pines del L298N (Motores Amarillos) ---
const int PIN_ENA = 22; 
const int PIN_IN1 = 21; 
const int PIN_IN2 = 19; 
const int PIN_IN3 = 18; 
const int PIN_IN4 = 5;  
const int PIN_ENB = 23; 

Servo servoA; 
Servo servoB; 
Servo servoC;  
Servo servoD;  

// Variables globales del Brazo
volatile int posA = 81;
volatile int posB = 90; 
volatile int posC = 90; 
volatile int posD = 90;

volatile int ultimoEstadoCLK_Hombro;
volatile int ultimoEstadoCLK_Codo;
volatile bool cambioHombro = false;
volatile bool cambioCodo = false;

// Variables de Secuencia y Tracción
bool triggerSequence = false;
bool triggerReverse = false;
volatile bool stopRequested = false; 

String currentMovement = "stop"; 
bool motorsEnabled = false;
int motorSpeed = 100;        // Velocidad continua (Rango 0 - 255)
int turnSpeed = 100;         // Velocidad al girar (Rango 0 - 255)
int forwardKickPWM = 200;    // Fuerza arranque adelante (150-255)
int forwardKickDur  = 15;    // Duración arranque adelante ms (0-100)
int turnKickPWM     = 200;   // Fuerza arranque giro (150-255)
int turnKickDur     = 15;    // Duración arranque giro ms (0-100)

const int NUM_POINTS = 3;
int sequence[NUM_POINTS][2] = {
  {180, 30},   // Punto 1: B=180, C=30
  {150, 0},    // Punto 2: B=150, C=0
  {76, 100}    // Punto 3: B=76,  C=100
};

WebServer server(80);

// --- Función de Control de Tracción L298N ---
void updateMotorAction(String cmd) {
  static String lastCmd = "stop";

  if (!motorsEnabled || cmd == "stop") {
    digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
    ledcWrite(PIN_ENA, 0);      ledcWrite(PIN_ENB, 0);
    lastCmd = "stop";
    return;
  }

  if (cmd == "forward") {
    digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  } else if (cmd == "backward") {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);
  } else if (cmd == "left") {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, LOW);   // A stop
    digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);   // B forward
  } else if (cmd == "right") {
    digitalWrite(PIN_IN1, LOW);  digitalWrite(PIN_IN2, LOW);   // A stop
    digitalWrite(PIN_IN3, LOW);  digitalWrite(PIN_IN4, HIGH);  // B reverse
  }

  // Kickstart con parámetros separados por modo
  bool isTurn = (cmd == "left" || cmd == "right");
  bool needsKick = (cmd != lastCmd && !isTurn && forwardKickDur > 0) || (cmd != lastCmd && isTurn && turnKickDur > 0);
  if (needsKick) {
    int pwm = isTurn ? turnKickPWM : forwardKickPWM;
    int dur = isTurn ? turnKickDur : forwardKickDur;
    ledcWrite(PIN_ENA, isTurn ? 0 : pwm);
    ledcWrite(PIN_ENB, pwm);
    delay(dur);
  }

  // PWM continuo: turn usa turnSpeed en B, forward/backward usa motorSpeed en ambos
  ledcWrite(PIN_ENA, isTurn ? 0 : motorSpeed);
  ledcWrite(PIN_ENB, isTurn ? turnSpeed : motorSpeed);

  lastCmd = cmd;
}

// --- Interrupciones ---
void IRAM_ATTR alCambiarHombro() {
  int estadoCLK = digitalRead(ENCODER_HOMBRO_CLK);
  if (estadoCLK != ultimoEstadoCLK_Hombro && estadoCLK == LOW) {
    if (digitalRead(ENCODER_HOMBRO_DT) != estadoCLK) posB = constrain(posB + 2, 65, 180); 
    else posB = constrain(posB - 2, 65, 180); 
    cambioHombro = true;
  }
  ultimoEstadoCLK_Hombro = estadoCLK;
}

void IRAM_ATTR alCambiarCodo() {
  int estadoCLK = digitalRead(ENCODER_CODO_CLK);
  if (estadoCLK != ultimoEstadoCLK_Codo && estadoCLK == LOW) {
    if (digitalRead(ENCODER_CODO_DT) != estadoCLK) posC = constrain(posC + 2, 0, 100); 
    else posC = constrain(posC - 2, 0, 100); 
    cambioCodo = true;
  }
  ultimoEstadoCLK_Codo = estadoCLK;
}

// --- Función de Espera Activa ---
void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    server.handleClient();
    delay(10);
  }
}

// --- Funciones de Movimiento ---
void moveBetweenPoints(int fromIdx, int toIdx) {
  int targetB = constrain(sequence[toIdx][0], 65, 180);
  int targetC = constrain(sequence[toIdx][1], 0, 100);

  //if (stopRequested) return;

  if (toIdx == 1) {
    // Destino es punto 2: C se mueve primero
    posB = targetB;
    servoB.write(posB);
    smartDelay(1000);
    //if (stopRequested) return;
    posC = targetC;
    servoC.write(posC);
  } else {
    // Destino es punto 1 o 3: B se mueve primero
    posC = targetC;
    servoC.write(posC);
    smartDelay(1000);
    //if (stopRequested) return;
    posB = targetB;
    servoB.write(posB);
  }
  smartDelay(1000);
  //if (stopRequested) return;
}

void playSequence() {
  stopRequested = false;
  moveBetweenPoints(0, 1);  // 1 -> 2
  //if (stopRequested) return;
  moveBetweenPoints(1, 2);  // 2 -> 3
}

void playReverseSequence() {
  stopRequested = false;
  moveBetweenPoints(2, 1);  // 3 -> 2
  //if (stopRequested) return;
  moveBetweenPoints(1, 0);  // 2 -> 1
}

// --- Interfaz Web dinámica ---
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="utf-8">
    <title>Control de Robot Completo</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; padding: 20px; color: #333; }
        .container { max-width: 500px; margin: auto; background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; font-size: 24px; margin-bottom: 20px; }
        .status-box { background-color: #ecf0f1; padding: 15px; border-radius: 8px; margin-bottom: 20px; font-size: 18px; font-weight: bold;}
        .status-box span { color: #e74c3c; text-transform: uppercase; }
        
        /* Estilos de Sección */
        .section-title { background-color: #34495e; color: white; padding: 10px; border-radius: 5px; margin-top: 25px; margin-bottom: 15px; font-size: 18px; }
        .control-group { margin-bottom: 20px; text-align: left; }
        label { font-weight: bold; display: block; margin-bottom: 8px; color: #555; }
        .slider { width: 100%; height: 10px; border-radius: 5px; background: #ddd; outline: none; -webkit-appearance: none; }
        .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%; background: #3498db; cursor: pointer; }
        .value-display { font-weight: bold; color: #3498db; float: right; }
        
        .button-group { display: flex; gap: 10px; margin-top: 15px; }
        .btn { color: white; border: none; padding: 12px 10px; font-size: 16px; border-radius: 8px; cursor: pointer; width: 100%; font-weight: bold;}
        .btn-seq { background-color: #2ecc71; }
        .btn-seq:hover { background-color: #27ae60; }
        .btn-rev { background-color: #f39c12; }
        .btn-rev:hover { background-color: #d68910; }
        .btn-stop { background-color: #e74c3c; width: 100%; margin-top: 10px;}
        .btn-stop:hover { background-color: #c0392b; }
        
        .toggle-group { display: flex; align-items: center; gap: 10px; margin-bottom: 15px; background: #fdfefe; padding: 10px; border: 1px solid #ccc; border-radius: 8px;}
        .toggle-group input { width: 20px; height: 20px; cursor: pointer; }

        /* Estilos del D-Pad */
        .d-pad { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; margin-bottom: 20px; }
        .btn-dir { background-color: #3498db; color: white; border: none; padding: 15px 5px; border-radius: 8px; font-weight: bold; cursor: pointer; font-size: 14px;}
        .btn-dir:hover { background-color: #2980b9; }
        .btn-dir:active { background-color: #1f618d; transform: scale(0.95); }
        .empty-cell { background: transparent; border: none; }
        .btn-stop-mini { background-color: #e74c3c; color: white; border: none; border-radius: 8px; font-weight: bold; cursor: pointer;}
        .btn-stop-mini:active { transform: scale(0.95); }
    </style>
</head>
<body>
    <div class="container">
        <h1>Centro de Control</h1>
        
        <div class="status-box">
            Dirección Tracción: <span id="movStatus">stop</span>
        </div>

        <div class="section-title">Tracción Base (L298N)</div>
        <div class="toggle-group">
            <input type="checkbox" id="motorEnabled" onchange="updateMotorConfig()">
            <label for="motorEnabled" style="margin:0; font-size: 16px; color: #2c3e50; cursor: pointer;">Habilitar Movimiento del Robot</label>
        </div>
        
        <div class="control-group">
            <label>Velocidad Continua (PWM) <span id="valSpeed" class="value-display">150</span></label>
            <input type="range" min="0" max="255" value="150" class="slider" id="motorSpeed" oninput="updateMotorConfig()">
        </div>
        
        <div class="control-group">
            <label>Velocidad de Giro (Turn PWM) <span id="valTurn" class="value-display">100</span></label>
            <input type="range" min="0" max="255" value="100" class="slider" id="turnSpeed" oninput="updateMotorConfig()">
        </div>

        <div class="control-group">
            <label>Fuerza Arranque Adelante <span id="valFwdKick" class="value-display">200</span></label>
            <input type="range" min="150" max="255" value="200" class="slider" id="forwardKickPWM" oninput="updateMotorConfig()">
        </div>
        <div class="control-group">
            <label>Duración Arranque Adelante (ms) <span id="valFwdDur" class="value-display">15</span></label>
            <input type="range" min="0" max="100" value="15" class="slider" id="forwardKickDur" oninput="updateMotorConfig()">
        </div>
        <div class="control-group">
            <label>Fuerza Arranque Giro <span id="valTurnKick" class="value-display">200</span></label>
            <input type="range" min="150" max="255" value="200" class="slider" id="turnKickPWM" oninput="updateMotorConfig()">
        </div>
        <div class="control-group">
            <label>Duración Arranque Giro (ms) <span id="valTurnDur" class="value-display">15</span></label>
            <input type="range" min="0" max="100" value="15" class="slider" id="turnKickDur" oninput="updateMotorConfig()">
        </div>

        <label>Control de Pruebas Manuales:</label>
        <div class="d-pad">
            <div class="empty-cell"></div>
            <button class="btn-dir" onclick="sendCommand('forward')">Adelante</button>
            <div class="empty-cell"></div>
            
            <button class="btn-dir" onclick="sendCommand('left')">Izquierda</button>
            <button class="btn-stop-mini" onclick="sendCommand('stop')">STOP</button>
            <button class="btn-dir" onclick="sendCommand('right')">Derecha</button>
            
            <div class="empty-cell"></div>
            <button class="btn-dir" onclick="sendCommand('backward')">Atrás</button>
            <div class="empty-cell"></div>
        </div>

        <div class="section-title">Brazo Robótico</div>
        <div class="control-group">
            <label>Servo A (Base) <span id="valA" class="value-display">90</span>°</label>
            <input type="range" min="0" max="180" value="90" class="slider" id="servoA" oninput="updateServo('A', this.value)">
        </div>
        
        <div class="control-group">
            <label>Servo B (Hombro) <span id="valB" class="value-display">90</span>°</label>
            <input type="range" min="65" max="180" value="90" class="slider" id="servoB" oninput="updateServo('B', this.value)">
        </div>
        
        <div class="control-group">
            <label>Servo C (Codo) <span id="valC" class="value-display">90</span>°</label>
            <input type="range" min="0" max="100" value="90" class="slider" id="servoC" oninput="updateServo('C', this.value)">
        </div>
        
        <div class="control-group">
            <label>Servo D (Pinza) <span id="valD" class="value-display">90</span>°</label>
            <input type="range" min="0" max="180" value="90" class="slider" id="servoD" oninput="updateServo('D', this.value)">
        </div>

        <div class="button-group">
            <button class="btn btn-rev" onclick="runReverse()">Secuencia Inversa</button>
            <button class="btn btn-seq" onclick="runSequence()">Secuencia Adelante</button>
        </div>

        <div class="button-group">
            <button class="btn btn-rev" onclick="closeGripper()">Cerrar Pinza</button>
            <button class="btn btn-seq" onclick="openGripper()">Abrir Pinza</button>
        </div>
        
        <button class="btn btn-stop" onclick="sendCommand('stop')">PARADA DE EMERGENCIA GLOBAL</button>
    </div>

    <script>
        function updateServo(servo, val) {
            document.getElementById('val' + servo).innerText = val;
            fetch('/set?servo=' + servo + '&pos=' + val);
        }

        function updateMotorConfig() {
            let isEnabled = document.getElementById('motorEnabled').checked ? 1 : 0;
            let currentSpeed = document.getElementById('motorSpeed').value;
            let currentTurn = document.getElementById('turnSpeed').value;
            let curFwdPWM = document.getElementById('forwardKickPWM').value;
            let curFwdDur = document.getElementById('forwardKickDur').value;
            let curTurnPWM = document.getElementById('turnKickPWM').value;
            let curTurnDur = document.getElementById('turnKickDur').value;
            
            document.getElementById('valSpeed').innerText = currentSpeed;
            document.getElementById('valTurn').innerText = currentTurn;
            document.getElementById('valFwdKick').innerText = curFwdPWM;
            document.getElementById('valFwdDur').innerText = curFwdDur;
            document.getElementById('valTurnKick').innerText = curTurnPWM;
            document.getElementById('valTurnDur').innerText = curTurnDur;
            
            fetch('/motorConfig?enabled=' + isEnabled + '&speed=' + currentSpeed + '&turnPWM=' + currentTurn
                + '&fwdPWM=' + curFwdPWM + '&fwdDur=' + curFwdDur + '&turnKickPWM=' + curTurnPWM + '&turnKickDur=' + curTurnDur);
        }

        function runSequence() { fetch('/sequence'); }
        function runReverse() { fetch('/reverse'); }
        function openGripper() { fetch('/gripper/open'); }
        function closeGripper() { fetch('/gripper/close'); }
        
        function sendCommand(cmd) {
            fetch('/move', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: cmd })
            });
        }

        setInterval(function() {
            fetch('/status').then(response => response.json()).then(data => {
                document.getElementById('servoA').value = data.A; document.getElementById('valA').innerText = data.A;
                document.getElementById('servoB').value = data.B; document.getElementById('valB').innerText = data.B;
                document.getElementById('servoC').value = data.C; document.getElementById('valC').innerText = data.C;
                document.getElementById('servoD').value = data.D; document.getElementById('valD').innerText = data.D;
                
                document.getElementById('movStatus').innerText = data.mov; 
                
                document.getElementById('motorSpeed').value = data.speed; document.getElementById('valSpeed').innerText = data.speed;
                document.getElementById('turnSpeed').value = data.turn; document.getElementById('valTurn').innerText = data.turn;
                document.getElementById('forwardKickPWM').value = data.fwdPWM; document.getElementById('valFwdKick').innerText = data.fwdPWM;
                document.getElementById('forwardKickDur').value = data.fwdDur; document.getElementById('valFwdDur').innerText = data.fwdDur;
                document.getElementById('turnKickPWM').value = data.turnKickPWM; document.getElementById('valTurnKick').innerText = data.turnKickPWM;
                document.getElementById('turnKickDur').value = data.turnKickDur; document.getElementById('valTurnDur').innerText = data.turnKickDur;
                
                document.getElementById('motorEnabled').checked = data.enabled;
            });
        }, 500); 
    </script>
</body>
</html>
)rawliteral";

// --- Handlers del Servidor Web ---
void handleRoot() {
  server.send_P(200, "text/html", HTML_INDEX);
}

void handleStatus() {
  String json = "{\"A\":" + String(posA) + ",\"B\":" + String(posB) + ",\"C\":" + String(posC) + ",\"D\":" + String(posD) + 
                ",\"mov\":\"" + currentMovement + "\",\"speed\":" + String(motorSpeed) + ",\"enabled\":" + (motorsEnabled ? "true" : "false") + 
                ",\"turn\":" + String(turnSpeed) + 
                ",\"fwdPWM\":" + String(forwardKickPWM) + ",\"fwdDur\":" + String(forwardKickDur) + 
                ",\"turnKickPWM\":" + String(turnKickPWM) + ",\"turnKickDur\":" + String(turnKickDur) + "}";
  server.send(200, "application/json", json);
}

void handleMotorConfig() {
  // Ahora requiere también el parámetro kickDur
  if (server.hasArg("enabled") && server.hasArg("speed") && server.hasArg("turnPWM")
      && server.hasArg("fwdPWM") && server.hasArg("fwdDur") && server.hasArg("turnKickPWM") && server.hasArg("turnKickDur")) {
    motorsEnabled = server.arg("enabled") == "1";
    motorSpeed = constrain(server.arg("speed").toInt(), 0, 255);
    turnSpeed = constrain(server.arg("turnPWM").toInt(), 0, 255);
    forwardKickPWM = constrain(server.arg("fwdPWM").toInt(), 150, 255);
    forwardKickDur = constrain(server.arg("fwdDur").toInt(), 0, 100);
    turnKickPWM = constrain(server.arg("turnKickPWM").toInt(), 150, 255);
    turnKickDur = constrain(server.arg("turnKickDur").toInt(), 0, 100);
    
    if (!motorsEnabled) updateMotorAction("stop");
    else updateMotorAction(currentMovement); 

    server.send(200, "text/plain", "Configuracion de motor actualizada");
  } else {
    server.send(400, "text/plain", "Faltan parametros");
  }
}

void handleServoControl() {
  if (server.hasArg("servo") && server.hasArg("pos")) {
    String servoChar = server.arg("servo");
    int angulo = server.arg("pos").toInt();

    stopRequested = true; 

    if (servoChar == "A") { posA = constrain(angulo, 0, 180); servoA.write(posA); }
    else if (servoChar == "B") { posB = constrain(angulo, 65, 180); servoB.write(posB); } 
    else if (servoChar == "C") { posC = constrain(angulo, 0, 100); servoC.write(posC); }  
    else if (servoChar == "D") { posD = constrain(angulo, 0, 180); servoD.write(posD); }
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Mala petición");
  }
}

void handleSequenceRequest() {
  server.send(200, "text/plain", "Secuencia adelante encolada");
  triggerSequence = true; 
}

void handleReverseRequest() {
  server.send(200, "text/plain", "Secuencia inversa encolada");
  triggerReverse = true; 
}

void handleOpenGripper() {
  stopRequested = true;
  posD = 90;
  servoD.write(posD);
  server.send(200, "text/plain", "Pinza abierta");
}

void handleCloseGripper() {
  stopRequested = true;
  posD = 20;
  servoD.write(posD);
  server.send(200, "text/plain", "Pinza cerrada");
}

void handleMoveRequest() {
  if (server.hasArg("plain")) { 
    String body = server.arg("plain");
    
    int cmdIndex = body.indexOf("\"command\"");
    if (cmdIndex != -1) {
      int colonIndex = body.indexOf(':', cmdIndex);
      int valStart = body.indexOf('"', colonIndex) + 1;
      int valEnd = body.indexOf('"', valStart);
      
      if (valStart > 0 && valEnd > valStart) {
        currentMovement = body.substring(valStart, valEnd);
      }
    } else {
      currentMovement = body;
      currentMovement.trim();
    }
    
    Serial.println("Comando traccion recibido: " + currentMovement);

    if (currentMovement.equalsIgnoreCase("stop")) {
      stopRequested = true;
      triggerSequence = false;
      triggerReverse = false;
    } else {
      stopRequested = false;
    }

    updateMotorAction(currentMovement);

    server.send(200, "text/plain", "Movimiento registrado");
  } else {
    server.send(400, "text/plain", "Cuerpo vacío");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IN1, OUTPUT); pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT); pinMode(PIN_IN4, OUTPUT);
  
  ESP32PWM::allocateTimer(0);
  
  servoA.setPeriodHertz(50);
  servoB.setPeriodHertz(50);
  servoC.setPeriodHertz(50);
  servoD.setPeriodHertz(50);

  servoA.attach(PIN_SERVO_A); 
  servoB.attach(PIN_SERVO_B);
  servoC.attach(PIN_SERVO_C); 
  servoD.attach(PIN_SERVO_D);
  
  servoA.write(posA); servoB.write(posB);
  servoC.write(posC); servoD.write(posD);

  ledcAttach(PIN_ENA, 5000, 8); 
  ledcAttach(PIN_ENB, 5000, 8);

  updateMotorAction("stop");

  pinMode(ENCODER_HOMBRO_CLK, INPUT_PULLUP);
  pinMode(ENCODER_HOMBRO_DT, INPUT_PULLUP);
  pinMode(ENCODER_CODO_CLK, INPUT_PULLUP);
  pinMode(ENCODER_CODO_DT, INPUT_PULLUP);

  ultimoEstadoCLK_Hombro = digitalRead(ENCODER_HOMBRO_CLK);
  ultimoEstadoCLK_Codo = digitalRead(ENCODER_CODO_CLK);

  attachInterrupt(digitalPinToInterrupt(ENCODER_HOMBRO_CLK), alCambiarHombro, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CODO_CLK), alCambiarCodo, CHANGE);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nWiFi Conectado. IP: http://%s\n", WiFi.localIP().toString().c_str());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_GET, handleServoControl);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/motorConfig", HTTP_GET, handleMotorConfig); 
  server.on("/sequence", HTTP_GET, handleSequenceRequest);
  server.on("/reverse", HTTP_GET, handleReverseRequest); 
  server.on("/gripper/open", HTTP_GET, handleOpenGripper);
  server.on("/gripper/close", HTTP_GET, handleCloseGripper);
  server.on("/move", HTTP_POST, handleMoveRequest); 
  server.on("/move/", HTTP_POST, handleMoveRequest); 
  
  server.begin();
}

void loop() {
  server.handleClient();

  if (triggerSequence) {
    triggerSequence = false;
    playSequence();
  }
  else if (triggerReverse) {
    triggerReverse = false;
    playReverseSequence();
  }

  if (cambioHombro) {
    servoB.write(posB);
    cambioHombro = false;
  }

  if (cambioCodo) {
    servoC.write(posC);
    cambioCodo = false;
  }

  if (Serial.available() > 0) {
    char comando = toupper(Serial.read());
    if (comando == '\n' || comando == '\r' || comando == ' ') return;

    if (comando == 'A' || comando == 'B' || comando == 'C' || comando == 'D') {
      stopRequested = true; 
      int angulo = Serial.parseInt();
      
      if (comando == 'A') { posA = constrain(angulo, 0, 180); servoA.write(posA); }
      else if (comando == 'B') { posB = constrain(angulo, 65, 180); servoB.write(posB); }
      else if (comando == 'C') { posC = constrain(angulo, 0, 100); servoC.write(posC); }
      else if (comando == 'D') { posD = constrain(angulo, 0, 180); servoD.write(posD); }
    } else {
      while(Serial.available()) Serial.read(); 
    }
  }
}
