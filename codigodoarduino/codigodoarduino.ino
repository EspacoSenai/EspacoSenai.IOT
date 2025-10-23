#include <WiFi.h>
#include <Keypad.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// -------- CONFIG LEDS / RELÉ --------
#define ledRed 18
#define ledGreen 19
#define ledYellow 21
#define RELE4 22

// -------- TECLADO (3x4) --------
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// -------- WIFI --------
const char* ssid = "Senai";
const char* password = "Senaisp@115";
IPAddress local_IP(192, 168, 0, 123);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// -------- SERVIDOR --------
WebServer server(80);

// -------- VARIÁVEIS GLOBAIS --------
String pinBuffer = "";
bool energia = false;  // variável recebida da API

// -------- CONFIG API --------
const char* apiURL = "http://seu-backend.com/api/pin";  // <-- coloque aqui sua rota de API

// ---------- FUNÇÕES ----------
void handleRoot();
void handlePin();
void tratarEntrada(char entrada);
void enviarPinParaAPI(String pin);

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

  digitalWrite(ledRed, HIGH);
  digitalWrite(ledGreen, HIGH);
  digitalWrite(ledYellow, HIGH);
  digitalWrite(RELE4, HIGH);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Falha IP fixo, usando DHCP");
  }
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

  // ---------- ROTAS ----------
  server.on("/", HTTP_GET, handleRoot);
  server.on("/pin", HTTP_POST, handlePin);

  // habilitar CORS para OPTIONS
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

// ---- RECEBE PIN VIA API ----
void handlePin() {
  addCorsHeaders();
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"erro\":\"Sem corpo JSON\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err || !doc.containsKey("pin")) {
    server.send(400, "application/json", "{\"erro\":\"JSON inválido\"}");
    return;
  }

  String pin = doc["pin"];
  enviarPinParaAPI(pin);
  String json = "{\"energia\":" + String(energia ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// ---- TECLADO ----
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

// ---- ENVIO PARA API EXTERNA ----
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

  http.end();
}
