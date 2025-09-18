#include <WiFi.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <WebServer.h>
#include <DHT.h>

// --- Definições de Pinos ---
#define ledRed 18
#define ledGreen 21
#define ledYellow 22
#define RELE4 23

#define DHT_PIN 34      // ⚠️ Se der erro, troque para pino 15, 4 ou 5
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// LCD: rs, en, d4, d5, d6, d7
LiquidCrystal lcd(15, 2, 4, 5, 32, 35);

// Teclado Matricial
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'#','0','*'}
};

byte rowPins[ROWS] = {33, 25, 26, 27};
byte colPins[COLS] = {14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Wi-Fi
const char* ssid = "Senai";
const char* password = "Senaisp@115";

IPAddress local_IP(192, 168, 0, 123);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// Servidor HTTP
WebServer server(80);

// Temperatura
float lastTemperature = NAN;
unsigned long lastDHTRead = 0;

void setup() {
  Serial.begin(115200);

  lcd.begin(16, 4);
  lcd.clear();
  lcd.print("Iniciando...");

  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(RELE4, OUTPUT);

  digitalWrite(ledRed, HIGH);
  digitalWrite(ledGreen, HIGH);
  digitalWrite(ledYellow, HIGH);
  digitalWrite(RELE4, HIGH);

  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi: Conectando");

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha ao configurar IP estático");
    lcd.setCursor(0, 3);
    lcd.print("Erro IP fixo");
    return;
  }

  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(tentativas % 16, 2);
    lcd.print(".");
    tentativas++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    lcd.setCursor(0, 3);
    lcd.print("Erro Wi-Fi");
    return;
  }

  lcd.setCursor(0, 2);
  lcd.print("Conectado!");
  lcd.setCursor(0, 3);
  lcd.print(WiFi.localIP());
  Serial.println();
  Serial.println("Wi-Fi conectado.");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());

  dht.begin();

  server.on("/", handleRoot);
  server.on("/led", HTTP_POST, handleLed);
  server.on("/temperature", HTTP_GET, handleTemperature);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();

  // Leitura do teclado
  char key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.println(key);
    tratarEntrada(key);
  }

  // Leitura periódica do DHT22 (a cada 2 segundos)
  if (millis() - lastDHTRead > 2000) {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      lastTemperature = temp;
      Serial.print("Temperatura atualizada: ");
      Serial.println(lastTemperature);
    } else {
      Serial.println("Falha ao ler o sensor DHT");
    }
    lastDHTRead = millis();
  }
}

// --- Rotas Web ---
void handleRoot() {
  server.send(200, "text/plain", "ESP32 Web Server ativo.");
}

void handleLed() {
  if (!server.hasArg("led")) {
    server.send(400, "text/plain", "Parâmetro 'led' não informado.");
    return;
  }

  String led = server.arg("led");
  String response;

  if (led == "red") {
    digitalWrite(ledRed, !digitalRead(ledRed));
    response = "LED Vermelho alternado.";
  } else if (led == "green") {
    digitalWrite(ledGreen, !digitalRead(ledGreen));
    response = "LED Verde alternado.";
  } else if (led == "yellow") {
    digitalWrite(ledYellow, !digitalRead(ledYellow));
    response = "LED Amarelo alternado.";
  } else if (led == "rele") {
    digitalWrite(RELE4, !digitalRead(RELE4));
    response = "RELE4 alternado.";
  } else {
    server.send(400, "text/plain", "Valor inválido para 'led'.");
    return;
  }

  Serial.println(response);
  lcd.clear();
  lcd.print(response);
  server.send(200, "text/plain", response);
}

void handleTemperature() {
  if (isnan(lastTemperature)) {
    server.send(500, "text/plain", "Temperatura não disponível.");
    return;
  }

  String response = "{\"temperature\": " + String(lastTemperature) + "}";
  server.send(200, "application/json", response);
}

// --- Entrada do Teclado ---
void tratarEntrada(char entrada) {
  switch (entrada) {
    case '1':
    case '5':
      digitalWrite(ledRed, !digitalRead(ledRed));
      lcd.clear();
      lcd.print("LED Vermelho: ");
      lcd.print(digitalRead(ledRed) == LOW ? "ON" : "OFF");
      break;

    case '2':
    case '6':
      digitalWrite(ledGreen, !digitalRead(ledGreen));
      lcd.clear();
      lcd.print("LED Verde: ");
      lcd.print(digitalRead(ledGreen) == LOW ? "ON" : "OFF");
      break;

    case '3':
    case '7':
      digitalWrite(ledYellow, !digitalRead(ledYellow));
      lcd.clear();
      lcd.print("LED Amarelo: ");
      lcd.print(digitalRead(ledYellow) == LOW ? "ON" : "OFF");
      break;

    case '4':
    case '8':
      digitalWrite(RELE4, !digitalRead(RELE4));
      lcd.clear();
      lcd.print("RELE4: ");
      lcd.print(digitalRead(RELE4) == LOW ? "ON" : "OFF");
      break;

    default:
      lcd.setCursor(0, 1);
      lcd.print("Tecla: ");
      lcd.print(entrada);
      lcd.print(" - Sem acao");
      break;
  }
}
