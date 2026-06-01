#include <Wire.h>
#include <LiquidCrystal_I2C.h> // Biblioteca para LCD com interface I2C

// Definição dos pinos
const int pinPotenciometro = A0;   // Pino do potenciômetro
const int triggerPin = 13;         // Pino Trigger do sensor ultrassônico
const int echoPin = 12;            // Pino Echo do sensor ultrassônico
const int ledVerde = 10;           // LED Verde
const int ledAmarelo = 9;          // LED Amarelo
const int ledVermelho = 8;         // LED Vermelho
const int ledAzulinho = 6;        
const int buzzer = 7;              // Buzzer

// Endereço do módulo I2C (geralmente 0x27 ou 0x3F, ajuste conforme necessário)
LiquidCrystal_I2C lcd(0x27, 16, 2); // Endereço I2C, 16 colunas e 2 linhas

void setup() {
  // Inicializa os pinos dos LEDs e buzzer
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledAzulinho, OUTPUT);
  pinMode(buzzer, OUTPUT);
  
  // Configuração do sensor ultrassônico
  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // Inicializa a comunicação serial
  Serial.begin(9600);
  
  // Inicializa o LCD I2C
  lcd.init();
  lcd.backlight(); // Liga a luz de fundo do LCD
  lcd.setCursor(0, 0);
  lcd.print("Distancia Medida:");
  
  delay(1000); // Pequeno atraso para garantir a inicialização do LCD
}

long readUltrasonicDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  return pulseIn(echoPin, HIGH);
}

void loop() {
  // Lê o valor do potenciômetro (0 a 1023)
  int valorPotenciometro = analogRead(pinPotenciometro);

  // Ajusta a distância de atuação de cada LED com base no valor do potenciômetro
  float distanciaAzulinho = map(valorPotenciometro, 0, 1023, 0, 250);
  float distanciaVerde = map(valorPotenciometro, 0, 1023, 0, 100);
  float distanciaAmarelo = map(valorPotenciometro, 0, 1023, 0, 12);
  float distanciaVermelho = map(valorPotenciometro, 0, 1023, 0, 6);

  Serial.print(distanciaAzulinho);
  Serial.print("\n\r");
  Serial.print(distanciaVerde);
  Serial.print("\n\r");
  Serial.print(distanciaAmarelo);
  Serial.print("\n\r");
  Serial.print(distanciaVermelho);
  Serial.print("\n\r");


  // Calcula a distância real usando o sensor ultrassônico
  float distanciaReal = 0.01723 * readUltrasonicDistance(triggerPin, echoPin);

  if (distanciaReal <= 300) {
    // Atualiza o LCD com a distância real
    lcd.setCursor(0, 1); // Define a posição no LCD
    lcd.print("Dist: ");
    lcd.print(distanciaReal, 1); // Exibe a distância com uma casa decimal
    lcd.print(" cm  ");
  } else {
    lcd.setCursor(0, 1); // Define a posição no LCD
    lcd.print("Fuera de Rango   ");
  }

  // Lógica para LEDs e buzzer com base na distância real e nas distâncias ajustadas
  if (distanciaReal <= distanciaVermelho) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, HIGH);
    digitalWrite(ledAzulinho, LOW);
    tone(buzzer, 600, 1000); // Som de 600 Hz por 1000 ms
  } 
  else if (distanciaReal <= distanciaAmarelo) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, HIGH);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledAzulinho, LOW);
    tone(buzzer, 500, 300); // Som de 500 Hz por 300 ms
  } 
  else if (distanciaReal <= distanciaVerde) {
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledAzulinho, LOW);
    tone(buzzer, 400, 130); // Som de 400 Hz por 130 ms
  } 
  else if (distanciaReal <= distanciaAzulinho) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(ledAzulinho, HIGH);
    tone(buzzer, 700, 50); // Som de 700 Hz por 50 ms
  } 
  else {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    noTone(buzzer); // Sem som
  }

  delay(500); // Pequeno atraso para suavizar a execução
}
