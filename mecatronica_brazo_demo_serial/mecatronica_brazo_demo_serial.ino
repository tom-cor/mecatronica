#include <Servo.h>

// Creamos los objetos para cada servo
Servo servoA0; 
Servo servoA1; 
Servo servo8;  
Servo servo9;  

void setup() {
  // Iniciamos la comunicación serie a 9600 baudios
  Serial.begin(9600);

  // Asignamos los pines a cada servo
  servoA0.attach(A0);
  servoA1.attach(A1);
  servo8.attach(8);
  servo9.attach(9);

  // Llevamos el brazo a una posición inicial de reposo (90 grados)
  servoA0.write(90);
  servoA1.write(90);
  servo8.write(90);
  servo9.write(90);

  // Imprimimos las instrucciones en el monitor serie
  Serial.println("=== Brazo Robotico Listo ===");
  Serial.println("Uso: LetraDelPin + Angulo");
  Serial.println("Ejemplos:");
  Serial.println("  A45  (Mueve el servo en A0 a 45 grados)");
  Serial.println("  B180 (Mueve el servo en A1 a 180 grados)");
  Serial.println("  C90  (Mueve el servo en pin 8 a 90 grados)");
  Serial.println("  D10  (Mueve el servo en pin 9 a 10 grados)");
  Serial.println("----------------------------");
}

void loop() {
  // Comprobamos si ha llegado algún dato por el puerto serie
  if (Serial.available() > 0) {
    
    // Leemos el primer carácter (la letra indicadora)
    char comando = Serial.read();
    
    // Ignoramos espacios y saltos de línea invisibles
    if (comando == '\n' || comando == '\r' || comando == ' ') return;
    
    // Convertimos a mayúscula para aceptar tanto 'a' como 'A'
    comando = toupper(comando);

    // Si la letra es válida (A, B, C o D), procesamos el número que le sigue
    if (comando == 'A' || comando == 'B' || comando == 'C' || comando == 'D') {
      
      // parseInt() lee los números siguientes hasta encontrar otra letra o salto de línea
      int angulo = Serial.parseInt();
      
      // Limitamos el ángulo entre 0 y 180 para no forzar los motores
      angulo = constrain(angulo, 0, 180);

      // Movemos el servo correspondiente
      switch (comando) {
        case 'A':
          servoA0.write(angulo);
          Serial.print("Servo A0 -> ");
          break;
        case 'B':
          servoA1.write(angulo);
          Serial.print("Servo A1 -> ");
          break;
        case 'C':
          servo8.write(angulo);
          Serial.print("Servo 8 -> ");
          break;
        case 'D':
          servo9.write(angulo);
          Serial.print("Servo 9 -> ");
          break;
      }
      Serial.print(angulo);
      Serial.println(" grados");
    } else {
      // Si escribimos cualquier otra letra, vaciamos el buffer
      Serial.println("Comando no reconocido. Usa A, B, C o D.");
      while(Serial.available()) Serial.read(); 
    }
  }
}