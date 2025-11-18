#include <WiFi.h>
#include <Keypad.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// -------- CONFIG RELES (cada rele = uma máquina) --------
#define RELE1 23
#define RELE2 19
#define RELE3 5
#define RELE4 22

// -------- SENSORES DE TEMPERATURA --------
#define DHTPIN1 18 
#define DHTPIN2 32
#define DHTTYPE DHT22

DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

// --- CONFIGURAÇÃO DE MÁQUINAS (2 máquinas por enquanto) ---
struct Maquina {
  int id;                        // id retornado pelo backend
  int relePin;                   // pino do rele
  DHT* sensor;                   // sensor associado
  bool ativa;                    // se está ligada
  unsigned long horaFim;         // millis() de quando deve desligar
  float ultimaTemp;              // última temperatura lida
};

Maquina maquinas[2];

// -------- TECLADO --------
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {13, 33, 25, 26};
byte colPins[COLS] = {27, 14, 21};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String pinBuffer = "";

// -------- WIFI --------
const char* ssid = "Feio";
const char* password = "iurd2023";

// -------- SERVIDOR --------
WebServer server(80);

// -------- API --------
const char* apiURL = "http://192.168.1.6:8080";
const char* apiTempURL = "http://192.168.1.6:8080/temperaturas";


// -------- CONFIG TEMPERATURA --------
unsigned long ultimoEnvioTemp = 0;
const unsigned long intervaloTemp = 15000; // 3 min
const float limiteTemperatura = 15;        // se estiver abaixo disso, desligar

// ------------------------------------------------------
// ---------------------- FUNÇÕES -----------------------
// ------------------------------------------------------

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void setupMaquinas() {
  maquinas[0] = {1, RELE1, &dht1, false, 0, 0};
  maquinas[1] = {2, RELE2, &dht2, false, 0, 0};

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);

  digitalWrite(RELE1, HIGH);
  digitalWrite(RELE2, HIGH);
  digitalWrite(RELE3, HIGH);
  digitalWrite(RELE4, HIGH);
}

void ligarMaquina(int id, unsigned long tempoPermitido) {
  for (int i = 0; i < 2; i++) {
    if (maquinas[i].id == id) {
      maquinas[i].ativa = true;
      maquinas[i].horaFim = millis() + tempoPermitido * 1000;
      digitalWrite(maquinas[i].relePin, LOW);
      Serial.printf("Maquina %d ligada por %lu segundos.\n", id, tempoPermitido);
      return;
    }
  }
}

void desligarMaquina(int id) {
  for (int i = 0; i < 2; i++) {
    if (maquinas[i].id == id) {
      maquinas[i].ativa = false;
      digitalWrite(maquinas[i].relePin, HIGH);
      Serial.printf("Maquina %d desligada.\n", id);
      return;
    }
  }
}

// ---------- LEITURA DOS SENSORES ----------
void lerTemperaturas() {
  maquinas[0].ultimaTemp = maquinas[0].sensor->readTemperature();
  maquinas[1].ultimaTemp = maquinas[1].sensor->readTemperature();
}

// ----------- ENVIA TEMPERATURA PARA API ----------
void enviarTemperaturaAPI(int id, float temp) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(apiTempURL);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"id\":" + String(id) + ",\"temperatura\":" + String(temp,1) + "}";

  int code = http.POST(body);
  http.end();
}

// ---------- PROCESSA PIN ----------
void enviarPinParaAPI(String pin) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi.");
    return;
  }

  HTTPClient http;
  http.begin(apiURL);
  http.addHeader("Content-Type", "application/json");

  String jsonBody = "{\"pin\":\"" + pin + "\"}";
  int httpCode = http.POST(jsonBody);

  if (httpCode <= 0) {
    Serial.println("Erro HTTP.");
    return;
  }

  String resposta = http.getString();
  StaticJsonDocument<200> doc;

  if (deserializeJson(doc, resposta)) {
    Serial.println("Erro no JSON.");
    return;
  }

  if (doc.containsKey("erro")) {
    Serial.println("PIN incorreto.");
    return;
  }

  int id = doc["id"];
  int tempoPermitido = doc["tempoPermitido"]; // em segundos

  Serial.printf("Liberado ID: %d por %d segundos.\n", id, tempoPermitido);

  ligarMaquina(id, tempoPermitido);

  http.end();
}

// ---------- TRATA TECLADO ----------
void tratarEntrada(char entrada) {
  if (entrada >= '0' && entrada <= '9') {
    if (pinBuffer.length() < 4) pinBuffer += entrada;
  }
  else if (entrada == '*') {
    if (pinBuffer.length() == 4) {
      enviarPinParaAPI(pinBuffer);
      pinBuffer = "";
    }
  }
  else if (entrada == '#') {
    pinBuffer = "";
  }
}

// ---------------- ROTAS HTTP ----------------
void handleRoot() {
  addCorsHeaders();
  server.send(200, "text/plain", "ESP32 OK");
}

// ------------------------------------------------------
// ------------------------- SETUP ----------------------
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);

  dht1.begin();
  dht2.begin();

  setupMaquinas();

  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");

  server.on("/", HTTP_GET, handleRoot);
  server.begin();
}

// ------------------------------------------------------
// -------------------------- LOOP ----------------------
// ------------------------------------------------------
void loop() {
  server.handleClient();

  char key = keypad.getKey();
  if (key != NO_KEY) tratarEntrada(key);

  lerTemperaturas();

  // verificação de desligamento automático
  for (int i = 0; i < 2; i++) {
    if (maquinas[i].ativa && millis() >= maquinas[i].horaFim) {
      if (maquinas[i].ultimaTemp < limiteTemperatura) {
        desligarMaquina(maquinas[i].id);
      }
    }
  }

  // envio periódico
  if (millis() - ultimoEnvioTemp > intervaloTemp) {
    ultimoEnvioTemp = millis();
    for (int i = 0; i < 2; i++) {
      enviarTemperaturaAPI(maquinas[i].id, maquinas[i].ultimaTemp);
    }
  }
}
