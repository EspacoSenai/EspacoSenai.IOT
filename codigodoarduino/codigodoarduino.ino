#include <WiFi.h>
#include <Keypad.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// -------- CONFIGURAÇÕES DE REDE --------
const char* ssid = "Feio";
const char* password = "iurd2023";
const char* backendUrl = "http://192.168.1.6:8080";

// -------- CONFIG RELÉS --------
#define RELE1 23      // Relé para impressora 1
#define RELE2 19      // Relé para impressora 2
#define RELE3 22       // Relé para impressora 3
#define RELE4 4       // Relé para impressora 4

// -------- SENSORES DE TEMPERATURA (DHT22) --------
#define DHTPIN1 18     // Sensor 1, associado ao RELE1
#define DHTPIN2 32     // Sensor 2, associado ao RELE2
#define DHTTYPE DHT22
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

// -------- TECLADO (4x3) --------
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

// -------- VARIÁVEIS GLOBAIS --------
String pinBuffer = "";
long impressoraAtivaId = 0; // 0 = nenhuma impressora ativa
unsigned long ultimaVerificacaoTemp = 0;

// ---------- DECLARAÇÃO DE FUNÇÕES ----------
void tratarEntrada(char entrada);
void enviarPinParaAPI(String pin);
void enviarTemperatura(long id, double temp);
void controlarRele(long id, bool ligar);

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);

  digitalWrite(RELE1, HIGH);
  digitalWrite(RELE2, HIGH);
  digitalWrite(RELE3, HIGH);
  digitalWrite(RELE4, HIGH);

  dht1.begin();
  dht2.begin();

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- LOOP ----------
void loop() {
  char key = keypad.getKey();
  if (key) {
    tratarEntrada(key);
  }

  if (impressoraAtivaId != 0) {
    // ALTERAÇÃO 1: O intervalo foi mudado para 2 minutos (120000 ms)
    if (millis() - ultimaVerificacaoTemp > 120000) {
      ultimaVerificacaoTemp = millis();
      double temp = NAN;
      String erroMsg;

      if (impressoraAtivaId == 1) {
        temp = dht1.readTemperature();
        erroMsg = "Erro ao ler temperatura do Sensor 1!";
      } else if (impressoraAtivaId == 2) {
        temp = dht2.readTemperature();
        erroMsg = "Erro ao ler temperatura do Sensor 2!";
      }

      if (!isnan(temp)) {
        Serial.printf("Temperatura Sensor %ld: %.2f°C\n", impressoraAtivaId, temp);
        enviarTemperatura(impressoraAtivaId, temp);
      } else {
        Serial.println(erroMsg);
      }
    }
  }
}

// ---------- FUNÇÕES ----------

void tratarEntrada(char entrada) {
  Serial.print(entrada);
  if (entrada >= '0' && entrada <= '9') {
    if (pinBuffer.length() < 4) {
      pinBuffer += entrada;
    }
  } else if (entrada == '#') {
    pinBuffer = "";
    Serial.println("\nPIN resetado.");
  } else if (entrada == '*') {
    if (pinBuffer.length() == 4) {
      Serial.println("\nEnviando PIN: " + pinBuffer);
      enviarPinParaAPI(pinBuffer);
    } else {
      Serial.println("\nPIN inválido. Digite 4 números.");
    }
    pinBuffer = "";
  }
}

void enviarPinParaAPI(String pin) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem conexão Wi-Fi.");
    return;
  }

  HTTPClient http;
  String url = String(backendUrl) + "/reserva-impressora/pin";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<100> docRequest;
  docRequest["pin"] = pin;
  String jsonRequest;
  serializeJson(docRequest, jsonRequest);

  int httpCode = http.POST(jsonRequest);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Resposta da API: " + payload);

    StaticJsonDocument<200> docResponse;
    DeserializationError error = deserializeJson(docResponse, payload);

    if (error) {
      Serial.println("Falha ao analisar JSON da resposta.");
      return;
    }

    // ALTERAÇÃO 2: Verificando a chave "confirmacao" em vez de "success"
    bool confirmacao = docResponse["confirmacao"];
    if (confirmacao) {
      impressoraAtivaId = docResponse["id"].as<long>();
      Serial.printf("Reserva aprovada para a impressora %ld.\n", impressoraAtivaId);
      controlarRele(impressoraAtivaId, true);
    } else {
      Serial.println("PIN inválido ou reserva recusada.");
    }
  } else {
    Serial.printf("Erro na requisição HTTP: %d. %s\n", httpCode, http.errorToString(httpCode).c_str());
  }
  http.end();
}

void enviarTemperatura(long id, double temp) {
    if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem conexão Wi-Fi para enviar temperatura.");
    return;
  }

  HTTPClient http;
  String url = String(backendUrl) + "/reserva-impressora/temperatura";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<100> docRequest;
  docRequest["id"] = id;
  docRequest["temperatura"] = temp;
  String jsonRequest;
  serializeJson(docRequest, jsonRequest);

  int httpCode = http.POST(jsonRequest);

  if (httpCode <= 0) {
    Serial.printf("Erro ao enviar temperatura. Código: %d\n", httpCode);
  }
  http.end();
}

void controlarRele(long id, bool ligar) {
  int estado = ligar ? LOW : HIGH;
  switch (id) {
    case 1:
      digitalWrite(RELE1, estado);
      break;
    case 2:
      digitalWrite(RELE2, estado);
      break;
    case 3:
      digitalWrite(RELE3, estado);
      break;
    case 4:
      digitalWrite(RELE4, estado);
      break;
    default:
      Serial.println("ID de relé inválido.");
      return;
  }
  Serial.printf("Relé para impressora %ld foi %s.\n", id, ligar ? "LIGADO" : "DESLIGADO");
}
