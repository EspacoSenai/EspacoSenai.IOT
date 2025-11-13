#include <WiFi.h>
#include <Keypad.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// -------- CONFIG LEDS / RELÉ --------
#define ledRed 2      // D2
#define ledGreen 4    // D4
#define ledYellow 5   // D5
#define RELE4 22      // Relé no D22

// -------- SENSOR DE TEMPERATURA (AM2302 / DHT22) --------
#define DHTPIN1 18     // Sensor 1 no D18
#define DHTPIN2 35     // Sensor 2 no D35
#define DHTTYPE DHT22  // Tipo de sensor
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

float temperatura1 = 0.0;
float temperatura2 = 0.0;

// -------- TECLADO (3x4) --------
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {13, 14, 15, 16};   // D13 a D16 para as linhas (ROWS)
byte colPins[COLS] = {17, 18, 19};      // D17 a D19 para as colunas (COLS)
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// -------- VARIÁVEIS GLOBAIS --------
String pinBuffer = "";
bool energia = false;  // variável recebida da API
int ledStatus1 = LOW;  // Status do LED do sensor 1
int ledStatus2 = LOW;  // Status do LED do sensor 2

// -------- CONFIG API --------
const char* apiURL = "http://seu-backend.com/api/pin";  // <-- coloque aqui sua rota de API

// ---------- FUNÇÕES ----------

void handleRoot();
void handlePin();
void handleTemp();
void tratarEntrada(char entrada);
void enviarPinParaAPI(String pin);
void atualizarLEDs(bool token, int sensorId);

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(RELE4, OUTPUT);

  digitalWrite(ledRed, LOW);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledYellow, LOW);
  digitalWrite(RELE4, HIGH);

  dht1.begin();  // Inicializa o primeiro sensor (DHT1)
  dht2.begin();  // Inicializa o segundo sensor (DHT2)

  WiFi.begin(ssid, password);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 40) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Falha ao conectar ao Wi-Fi.");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/pin", HTTP_POST, handlePin);
  server.on("/temp", HTTP_GET, handleTemp);

  server.onNotFound([](){
    addCorsHeaders();
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  // Leitura da temperatura periodicamente
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 5000) {  // a cada 5 segundos
    lastRead = millis();
    temperatura1 = dht1.readTemperature();
    temperatura2 = dht2.readTemperature();

    if (isnan(temperatura1)) {
      Serial.println("Erro ao ler temperatura do DHT1!");
    } else {
      Serial.print("Temperatura Sensor 1: ");
      Serial.print(temperatura1);
      Serial.println(" °C");
    }

    if (isnan(temperatura2)) {
      Serial.println("Erro ao ler temperatura do DHT2!");
    } else {
      Serial.print("Temperatura Sensor 2: ");
      Serial.print(temperatura2);
      Serial.println(" °C");
    }
  }

  // Leitura do teclado
  char key = keypad.getKey();
  if (key != NO_KEY) {
    Serial.print("Tecla pressionada: ");
    Serial.println(key);
    tratarEntrada(key);
  }
}

// ---------- ROTAS ----------

void handleRoot() {
  addCorsHeaders();
  server.send(200, "text/plain", "ESP32 Web Server ativo.");
}

void handlePin() {
  addCorsHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"erro\":\"Sem corpo JSON\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err || !doc.containsKey("pin") || !doc.containsKey("token")) {
    server.send(400, "application/json", "{\"erro\":\"JSON inválido\"}");
    return;
  }

  String pin = doc["pin"];
  bool token = doc["token"];
  int sensorId = doc["id"];

  // Enviar o PIN para o backend
  enviarPinParaAPI(pin);

  // Atualizar o estado do LED baseado no token e no ID
  atualizarLEDs(token, sensorId);

  String json = "{\"energia\":" + String(energia ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleTemp() {
  addCorsHeaders();
  String json1 = "{\"id\":1, \"temperatura\":" + String(temperatura1, 1) + "}";
  String json2 = "{\"id\":2, \"temperatura\":" + String(temperatura2, 1) + "}";
  String response = "[" + json1 + "," + json2 + "]";
  server.send(200, "application/json", response);
}

// Função para tratar a entrada do teclado
void tratarEntrada(char entrada) {
  if (entrada >= '0' && entrada <= '9') {
    if (pinBuffer.length() < 4) {
      pinBuffer += entrada;
      Serial.print("PIN parcial: ");
      Serial.println(pinBuffer);
    }
  } 
  else if (entrada == '*') {
    if (pinBuffer.length() == 4) {
      Serial.println("Enviando PIN...");
      enviarPinParaAPI(pinBuffer);
      pinBuffer = "";
    } else {
      Serial.println("PIN incompleto, necessário 4 dígitos.");
    }
  } 
  else if (entrada == '#') {
    pinBuffer = "";
    Serial.println("PIN resetado.");
  }
}

// Função para enviar o PIN para a API
void enviarPinParaAPI(String pin) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi - não foi possível enviar PIN.");
    return;
  }

  HTTPClient http;
  http.begin(apiURL);
  http.addHeader("Content-Type", "application/json");

  String jsonBody = "{\"pin\":\"" + pin + "\"}";
  int httpCode = http.POST(jsonBody);

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Resposta da API: " + payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error && doc.containsKey("energia")) {
      energia = doc["energia"];
      Serial.print("Energia agora: ");
      Serial.println(energia ? "ATIVA" : "DESLIGADA");
    } else {
      Serial.println("Erro ao interpretar resposta JSON da API.");
    }
  } else {
    Serial.printf("Erro HTTP: %d\n", httpCode);
  }

