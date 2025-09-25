#include <WiFi.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <WebServer.h>
#include <DHT.h>

// -------- CONFIG SENSOR --------
#define DHTPIN 15      // pino DATA do AM2302
#define DHTTYPE DHT22  // AM2302 é compatível com DHT22
DHT dht(DHTPIN, DHTTYPE);

// -------- CONFIG LEDS / RELÉ --------
#define ledRed 18
#define ledGreen 21
#define ledYellow 22
#define RELE4 23

// LCD: rs, en, d4, d5, d6, d7
LiquidCrystal lcd(15, 2, 4, 5, 32, 35);

// -------- TECLADO --------
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

// -------- WIFI --------
const char* ssid = "Senai";
const char* password = "Senaisp@115";
IPAddress local_IP(192, 168, 0, 123);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// -------- SERVIDOR --------
WebServer server(80);

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot();
void handleLed();
void handleTemperature();
void tratarEntrada(char entrada);

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

  // inicia sensor
  dht.begin();

  // WiFi
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha IP fixo, usando DHCP");
  }
  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(tentativas % 16, 2);
    lcd.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.setCursor(0, 2);
    lcd.print("Conectado!");
    lcd.setCursor(0, 3);
    lcd.print(WiFi.localIP());
    Serial.println();
    Serial.print("Wi-Fi conectado. IP: ");
    Serial.println(WiFi.localIP());
  }

  // rotas
  server.on("/", HTTP_GET, handleRoot);
  server.on("/led", HTTP_POST, handleLed);
  server.on("/led", HTTP_OPTIONS, [](){
    addCorsHeaders();
    server.send(204, "text/plain", "");
  });

  server.on("/temperature", HTTP_GET, handleTemperature);
  server.on("/temperature", HTTP_OPTIONS, [](){
    addCorsHeaders();
    server.send(204, "text/plain", "");
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  server.handleClient();
  char key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.println(key);
    tratarEntrada(key);
  }
}

// ---- Rotas ----
void handleRoot() {
  addCorsHeaders();
  server.send(200, "text/plain", "ESP32 Web Server ativo.");
}

void handleLed() {
  addCorsHeaders();
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

// ---- SENSOR TEMPERATURA + UMIDADE ----
void handleTemperature() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    addCorsHeaders();
    server.send(500, "application/json", "{\"error\":\"Falha ao ler DHT\"}");
    Serial.println("Falha ao ler do DHT");
    return;
  }

  String json = "{\"temperature\":" + String(temp, 2) +
                ", \"humidity\":" + String(hum, 2) + "}";

  addCorsHeaders();
  server.send(200, "application/json", json);

  Serial.print("Temperatura: ");
  Serial.print(temp);
  Serial.print(" °C | Umidade: ");
  Serial.print(hum);
  Serial.println(" %");

  lcd.clear();
  lcd.print("Temp:");
  lcd.print(temp, 1);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Umid:");
  lcd.print(hum, 1);
  lcd.print("%");
}

// ---- TECLADO ----
void tratarEntrada(char entrada) {
  switch (entrada) {
    case '1': case '5':
      digitalWrite(ledRed, !digitalRead(ledRed));
      lcd.clear();
      lcd.print("LED Vermelho: ");
      lcd.print(digitalRead(ledRed) == LOW ? "ON" : "OFF");
      break;
    case '2': case '6':
      digitalWrite(ledGreen, !digitalRead(ledGreen));
      lcd.clear();
      lcd.print("LED Verde: ");
      lcd.print(digitalRead(ledGreen) == LOW ? "ON" : "OFF");
      break;
    case '3': case '7':
      digitalWrite(ledYellow, !digitalRead(ledYellow));
      lcd.clear();
      lcd.print("LED Amarelo: ");
      lcd.print(digitalRead(ledYellow) == LOW ? "ON" : "OFF");
      break;
    case '4': case '8':
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
