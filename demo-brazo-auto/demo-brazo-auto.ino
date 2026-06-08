#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// --- Configuración de Red WiFi ---
const char* ssid     = "elquesabesabe";       // Cambia esto por el nombre de tu red
const char* password = "yelquenovaalauade";   // Cambia esto por la contraseña

// --- Asignación de Pines del ESP32 ---
const int PIN_SERVO_A = 13; 
const int PIN_SERVO_B = 12; 
const int PIN_SERVO_C = 14; 
const int PIN_SERVO_D = 27; 

const int ENCODER_HOMBRO_CLK = 32;
const int ENCODER_HOMBRO_DT  = 33;
const int ENCODER_CODO_CLK   = 25;
const int ENCODER_CODO_DT    = 26;

Servo servoA; 
Servo servoB; 
Servo servoC;  
Servo servoD;  

// Variables globales
volatile int posA = 90;
volatile int posB = 90; // Límite inferior de 65
volatile int posC = 90; // Límite superior de 100
volatile int posD = 90;

volatile int ultimoEstadoCLK_Hombro;
volatile int ultimoEstadoCLK_Codo;

volatile bool cambioHombro = false;
volatile bool cambioCodo = false;

// Variables de la secuencia y estado
bool triggerSequence = false;
bool triggerReverse = false;
String currentMovement = "Ninguno"; // Guarda el último comando de movimiento recibido

const int NUM_POINTS = 13;
int sequence[NUM_POINTS][2] = {
  {180, 0}, {140, 0}, {140, 10}, {130, 10}, {130, 20},
  {120, 20}, {120, 35}, {110, 35}, {100, 55}, {100, 65},
  {85, 65}, {85, 50}, {65, 80}
};

WebServer server(80);

// --- Interrupciones ---
void IRAM_ATTR alCambiarHombro() {
  int estadoCLK = digitalRead(ENCODER_HOMBRO_CLK);
  if (estadoCLK != ultimoEstadoCLK_Hombro && estadoCLK == LOW) {
    if (digitalRead(ENCODER_HOMBRO_DT) != estadoCLK) {
      posB = constrain(posB + 2, 65, 180); 
    } else {
      posB = constrain(posB - 2, 65, 180); 
    }
    cambioHombro = true;
  }
  ultimoEstadoCLK_Hombro = estadoCLK;
}

void IRAM_ATTR alCambiarCodo() {
  int estadoCLK = digitalRead(ENCODER_CODO_CLK);
  if (estadoCLK != ultimoEstadoCLK_Codo && estadoCLK == LOW) {
    if (digitalRead(ENCODER_CODO_DT) != estadoCLK) {
      posC = constrain(posC + 2, 0, 100); 
    } else {
      posC = constrain(posC - 2, 0, 100); 
    }
    cambioCodo = true;
  }
  ultimoEstadoCLK_Codo = estadoCLK;
}

// --- Funciones de Interpolación ---
void moveToPoint(int index) {
  int targetB = constrain(sequence[index][0], 65, 180);
  int targetC = constrain(sequence[index][1], 0, 100);

  int startB = posB;
  int startC = posC;

  int diffB = targetB - startB;
  int diffC = targetC - startC;

  int steps = max(abs(diffB), abs(diffC));

  if (steps > 0) {
    for (int s = 1; s <= steps; s++) {
      posB = startB + (diffB * s) / steps;
      posC = startC + (diffC * s) / steps;
      
      servoB.write(posB);
      servoC.write(posC);
      
      server.handleClient(); 
      delay(15);             
    }
  }
  Serial.printf("Punto alcanzado: Hombro=%d, Codo=%d\n", posB, posC);
}

void playSequence() {
  Serial.println("--- Iniciando Secuencia ---");
  for (int i = 0; i < NUM_POINTS; i++) {
    moveToPoint(i);
    delay(200); 
  }
  Serial.println("--- Secuencia Finalizada ---");
}

void playReverseSequence() {
  Serial.println("--- Iniciando Secuencia Inversa ---");
  for (int i = NUM_POINTS - 1; i >= 0; i--) {
    moveToPoint(i);
    delay(200); 
  }
  Serial.println("--- Secuencia Inversa Finalizada ---");
}

// --- Interfaz Web dinámica ---
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="utf-8">
    <title>Control de Brazo Robótico</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; padding: 20px; color: #333; }
        .container { max-width: 500px; margin: auto; background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; font-size: 24px; margin-bottom: 20px; }
        .status-box { background-color: #ecf0f1; padding: 15px; border-radius: 8px; margin-bottom: 25px; font-size: 18px; font-weight: bold;}
        .status-box span { color: #e74c3c; text-transform: uppercase; }
        .control-group { margin-bottom: 25px; text-align: left; }
        label { font-weight: bold; display: block; margin-bottom: 8px; color: #555; }
        .slider { width: 100%; height: 10px; border-radius: 5px; background: #ddd; outline: none; -webkit-appearance: none; }
        .slider::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%; background: #3498db; cursor: pointer; }
        .value-display { font-weight: bold; color: #3498db; float: right; }
        
        .button-group { display: flex; gap: 10px; margin-top: 15px; }
        .btn { color: white; border: none; padding: 12px 10px; font-size: 16px; border-radius: 8px; cursor: pointer; width: 100%; font-weight: bold;}
        .btn-seq { background-color: #2ecc71; }
        .btn-seq:hover { background-color: #27ae60; }
        .btn-rev { background-color: #e74c3c; }
        .btn-rev:hover { background-color: #c0392b; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Control de Brazo Robótico</h1>
        
        <div class="status-box">
            Dirección actual: <span id="movStatus">Ninguno</span>
        </div>

        <div class="control-group">
            <label>Servo A (Base) <span id="valA" class="value-display">90</span>°</label>
            <input type="range" min="0" max="180" value="90" class="slider" id="servoA" oninput="updateServo('A', this.value)">
        </div>
        
        <div class="control-group">
            <label>Servo B (Hombro - Min 65°) <span id="valB" class="value-display">90</span>°</label>
            <input type="range" min="65" max="180" value="90" class="slider" id="servoB" oninput="updateServo('B', this.value)">
        </div>
        
        <div class="control-group">
            <label>Servo C (Codo - Max 100°) <span id="valC" class="value-display">90</span>°</label>
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
    </div>

    <script>
        function updateServo(servo, val) {
            document.getElementById('val' + servo).innerText = val;
            fetch('/set?servo=' + servo + '&pos=' + val);
        }

        function runSequence() { fetch('/sequence'); }
        function runReverse() { fetch('/reverse'); }

        setInterval(function() {
            fetch('/status').then(response => response.json()).then(data => {
                document.getElementById('servoA').value = data.A; document.getElementById('valA').innerText = data.A;
                document.getElementById('servoB').value = data.B; document.getElementById('valB').innerText = data.B;
                document.getElementById('servoC').value = data.C; document.getElementById('valC').innerText = data.C;
                document.getElementById('servoD').value = data.D; document.getElementById('valD').innerText = data.D;
                document.getElementById('movStatus').innerText = data.mov; // Actualiza el texto de movimiento
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
  // Ahora incluimos el campo 'mov' en el JSON
  String json = "{\"A\":" + String(posA) + ",\"B\":" + String(posB) + ",\"C\":" + String(posC) + ",\"D\":" + String(posD) + ",\"mov\":\"" + currentMovement + "\"}";
  server.send(200, "application/json", json);
}

void handleServoControl() {
  if (server.hasArg("servo") && server.hasArg("pos")) {
    String servoChar = server.arg("servo");
    int angulo = server.arg("pos").toInt();

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

// --- Nuevo handler para recibir direcciones en el cuerpo POST ---
void handleMoveRequest() {
  if (server.hasArg("plain")) { 
    String body = server.arg("plain");
    
    // Buscamos la clave "command" en el cuerpo JSON
    int cmdIndex = body.indexOf("\"command\"");
    
    if (cmdIndex != -1) {
      // Encontramos la clave, ahora buscamos los dos puntos y las comillas del valor
      int colonIndex = body.indexOf(':', cmdIndex);
      int valStart = body.indexOf('"', colonIndex) + 1;
      int valEnd = body.indexOf('"', valStart);
      
      if (valStart > 0 && valEnd > valStart) {
        // Extraemos solo la palabra (ej. "right")
        currentMovement = body.substring(valStart, valEnd);
      }
    } else {
      // Fallback: Si por alguna razón mandas texto plano, lo limpia y lo usa
      currentMovement = body;
      currentMovement.trim();
    }
    
    Serial.println("Comando de movimiento recibido: " + currentMovement);
    server.send(200, "text/plain", "Movimiento registrado");
  } else {
    server.send(400, "text/plain", "Cuerpo vacío");
  }
}

void setup() {
  Serial.begin(115200);

  servoA.attach(PIN_SERVO_A); servoB.attach(PIN_SERVO_B);
  servoC.attach(PIN_SERVO_C); servoD.attach(PIN_SERVO_D);
  
  servoA.write(posA); servoB.write(posB);
  servoC.write(posC); servoD.write(posD);

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
  server.on("/sequence", HTTP_GET, handleSequenceRequest);
  server.on("/reverse", HTTP_GET, handleReverseRequest); 
  server.on("/move", HTTP_POST, handleMoveRequest); // Registramos la nueva ruta POST
  
  server.begin();
}

void loop() {
  server.handleClient();

  // Prioriza la secuencia normal o inversa
  if (triggerSequence) {
    triggerSequence = false;
    playSequence();
  }
  else if (triggerReverse) {
    triggerReverse = false;
    playReverseSequence();
  }

  // Actualización por encoders
  if (cambioHombro) {
    servoB.write(posB);
    Serial.printf("Encoder: Hombro (Servo B) -> %d grados\n", posB);
    cambioHombro = false;
  }

  if (cambioCodo) {
    servoC.write(posC);
    Serial.printf("Encoder: Codo (Servo C) -> %d grados [MAX 100]\n", posC);
    cambioCodo = false;
  }

  // Control Serial
  if (Serial.available() > 0) {
    char comando = toupper(Serial.read());
    if (comando == '\n' || comando == '\r' || comando == ' ') return;

    if (comando == 'A' || comando == 'B' || comando == 'C' || comando == 'D') {
      int angulo = Serial.parseInt();
      
      if (comando == 'A') { posA = constrain(angulo, 0, 180); servoA.write(posA); Serial.printf("Serial: A -> %d\n", posA); }
      else if (comando == 'B') { posB = constrain(angulo, 65, 180); servoB.write(posB); Serial.printf("Serial: B -> %d [MIN 65]\n", posB); }
      else if (comando == 'C') { posC = constrain(angulo, 0, 100); servoC.write(posC); Serial.printf("Serial: C -> %d [MAX 100]\n", posC); }
      else if (comando == 'D') { posD = constrain(angulo, 0, 180); servoD.write(posD); Serial.printf("Serial: D -> %d\n", posD); }
    } else {
      while(Serial.available()) Serial.read(); 
    }
  }
}