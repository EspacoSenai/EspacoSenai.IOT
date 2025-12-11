#include <WiFi.h>
#include <WiFiClientSecure.h> // Necessário para HTTPS
#include <Keypad.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// -------- CONFIGURAÇÕES DE REDE --------
const char* ssid = "AAPM";
const char* password = "alunosenai";
const char* backendUrl = "https://espacosenai.azurewebsites.net";

// -------- CREDENCIAIS DA API (PREENCHA AQUI) --------
const char* apiUser = "user@";
const char* apiPass = "password";

// -------- CONFIG RELÉS --------
#define RELE1 23
#define RELE2 19
#define RELE3 22
#define RELE4 4

// -------- SENSORES DE TEMPERATURA (DHT22) --------
#define DHTPIN1 18
#define DHTPIN2 32
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
String jwtToken = ""; // Armazena o accessToken

bool impressorasOcupadas[5] = {false, false, false, false, false}; 
unsigned long ultimaVerificacaoTemp[5] = {0, 0, 0, 0, 0}; 

// Timer para tentar reconectar o token se falhar
unsigned long lastLoginAttempt = 0;

// ---------- DECLARAÇÃO DE FUNÇÕES ----------
void tratarEntrada(char entrada);
bool obterToken(); 
void enviarPinParaAPI(String pin);
void enviarTemperatura(long id, double temp);
void controlarRele(long id, bool ligar);
bool todasImpressorasEstaoOcupadas();

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);

  // Inicializa relés desligados (HIGH = desligado em módulo relé padrão)
  digitalWrite(RELE1, HIGH);
  digitalWrite(RELE2, HIGH);
  digitalWrite(RELE3, HIGH);
  digitalWrite(RELE4, HIGH);

  dht1.begin();
  dht2.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Tenta obter o token logo na inicialização
  obterToken();
}

// ---------- LOOP ----------
void loop() {
  // Se não tiver token, tenta obter a cada 30 segundos sem travar o loop
  if (jwtToken == "" && (millis() - lastLoginAttempt > 30000)) {
     obterToken();
  }

  char key = keypad.getKey();
  if (key) {
    tratarEntrada(key);
  }

  unsigned long agora = millis();
  
  for (int id = 1; id <= 4; id++) {
    if (impressorasOcupadas[id]) {
        // Verifica temperatura a cada 2 minutos (120000ms)
        if (agora - ultimaVerificacaoTemp[id] > 120000) {
            ultimaVerificacaoTemp[id] = agora;
            double temp = NAN;
            String erroMsg = "";

            if (id == 1) {
                temp = dht1.readTemperature();
                erroMsg = "Erro Sensor 1!";
            } else if (id == 2) {
                temp = dht2.readTemperature();
                erroMsg = "Erro Sensor 2!";
            }

            if (!isnan(temp)) {
                Serial.printf("Temp Sensor %d: %.2f°C\n", id, temp);
                enviarTemperatura(id, temp);
            } else if (id == 1 || id == 2) {
                Serial.println(erroMsg);
            }
        }
    }
  }
}

// ---------- FUNÇÕES ----------

// --- FUNÇÃO DE LOGIN ATUALIZADA ---
bool obterToken() {
  if (WiFi.status() != WL_CONNECTED) return false;

  lastLoginAttempt = millis();
  Serial.println("Autenticando...");

  WiFiClientSecure client;
  client.setInsecure(); // Necessário para aceitar o certificado SSL da Azure/ESP32
  HTTPClient http;

  // URL ATUALIZADA PARA /auth/signin
  String url = String(backendUrl) + "/auth/signin"; 
  
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Cria o JSON de login
  StaticJsonDocument doc;
  doc["username"] = apiUser; 
  doc["password"] = apiPass;
  
  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);

  if (httpCode == 200 || httpCode == 201) {
    String payload = http.getString();
    StaticJsonDocument docResp;
    DeserializationError error = deserializeJson(docResp, payload);
    
    if (error) {
      Serial.println("Erro ao ler JSON de login");
      http.end();
      return false;
    }

    // CHAVE ATUALIZADA PARA "accessToken"
    const char* token = docResp["accessToken"]; 
    
    if (token) {
      jwtToken = String(token);
      Serial.println("Login com sucesso! Token obtido.");
      http.end();
      return true;
    } else {
      Serial.println("Campo 'accessToken' não encontrado na resposta.");
    }
  } else {
    Serial.printf("Falha no login. Código: %d\n", httpCode);
    // Serial.println(http.getString()); // Descomente para ver o erro detalhado
  }
  
  http.end();
  return false;
}

bool todasImpressorasEstaoOcupadas() {
    return (impressorasOcupadas[1] && impressorasOcupadas[2] && impressorasOcupadas[3] && impressorasOcupadas[4]);
}

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
      
    if (todasImpressorasEstaoOcupadas()) {
        Serial.println("\nBLOQUEADO: Todas as impressoras estão em uso.");
        pinBuffer = ""; 
        return;         
    }

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
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Se não tem token, tenta logar antes de prosseguir
  if (jwtToken == "") {
    Serial.println("Sem Token. Tentando logar...");
    if(!obterToken()) return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = String(backendUrl) + "/reservas-impressora/liberar";
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  // Adiciona o Bearer Token
  http.addHeader("Authorization", "Bearer " + jwtToken);

  StaticJsonDocument docRequest;
  docRequest["pin"] = pin;
  String jsonRequest;
  serializeJson(docRequest, jsonRequest);

  int httpCode = http.POST(jsonRequest);

  // Verifica se o token expirou (401/403)
  if (httpCode == 401 || httpCode == 403) {
    Serial.println("Token expirado. Resetando...");
    jwtToken = ""; 
  } else if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("Resposta API: " + payload);

    StaticJsonDocument docResponse;
    deserializeJson(docResponse, payload);

    bool confirmacao = docResponse["confirmacao"];
    if (confirmacao) {
      long idRecebido = docResponse["id"].as<long>();
      
      if(idRecebido >= 1 && idRecebido <= 4) {
          Serial.printf("Aprovado impressora %ld.\n", idRecebido);
          controlarRele(idRecebido, true);
      } else {
          Serial.println("ID inválido recebido.");
      }
    } else {
      Serial.println("PIN recusado.");
    }
  } else {
    Serial.printf("Erro HTTP: %d\n", httpCode);
  }
  http.end();
}

void enviarTemperatura(long id, double temp) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (jwtToken == "") return; // Sem token, não envia temperatura

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = String(backendUrl) + "/reservas-impressora/temperatura";
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwtToken);

  StaticJsonDocument docRequest;
  docRequest["id"] = id;
  docRequest["temperatura"] = temp;
  String jsonRequest;
  serializeJson(docRequest, jsonRequest);

  int httpCode = http.POST(jsonRequest);
  
  if (httpCode == 401 || httpCode == 403) {
      jwtToken = ""; // Token expirou
  }
  
  if (httpCode <= 0) {
    Serial.printf("Erro envio temp: %d\n", httpCode);
  }
  http.end();
}

void controlarRele(long id, bool ligar) {
  if (id >= 1 && id <= 4) {
      impressorasOcupadas[id] = ligar;
  }
  int estado = ligar ? LOW : HIGH;
  switch (id) {
    case 1: digitalWrite(RELE1, estado); break;
    case 2: digitalWrite(RELE2, estado); break;
    case 3: digitalWrite(RELE3, estado); break;
    case 4: digitalWrite(RELE4, estado); break;
  }
  Serial.printf("Relé %ld %s.\n", id, ligar ? "LIGADO" : "DESLIGADO");
}
