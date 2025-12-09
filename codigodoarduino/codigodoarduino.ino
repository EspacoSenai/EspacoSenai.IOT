#include <WiFi.h>
#include <Keypad.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// -------- CONFIGURAÇÕES DE REDE --------
const char* ssid = "AAPM";
const char* password = "alunosenai";
const char* backendUrl = "https://espacosenai.azurewebsites.net";

// -------- CONFIG RELÉS --------
#define RELE1 23      // Relé para impressora 1
#define RELE2 19      // Relé para impressora 2
#define RELE3 22      // Relé para impressora 3
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

// NOVO: Array para rastrear o estado de cada impressora (índices 1 a 4)
// false = Livre/Desligada, true = Ocupada/Ligada
bool impressorasOcupadas[5] = {false, false, false, false, false}; 

// Array para controlar o tempo de leitura de temperatura individualmente
unsigned long ultimaVerificacaoTemp[5] = {0, 0, 0, 0, 0}; 

// ---------- DECLARAÇÃO DE FUNÇÕES ----------
void tratarEntrada(char entrada);
void enviarPinParaAPI(String pin);
void enviarTemperatura(long id, double temp);
void controlarRele(long id, bool ligar);
bool todasImpressorasEstaoOcupadas(); // Nova função auxiliar

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando...");

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);

  // Inicializa tudo desligado (HIGH para relés de lógica inversa)
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

  // NOVO LOGICA: Percorre todas as impressoras para checar temperatura
  // Isso permite que múltiplas impressoras funcionem ao mesmo tempo
  unsigned long agora = millis();
  
  for (int id = 1; id <= 4; id++) {
    // Se a impressora 'id' estiver ligada (ocupada)
    if (impressorasOcupadas[id]) {
        
        // Verifica se passou 2 minutos para ESSA impressora
        if (agora - ultimaVerificacaoTemp[id] > 120000) {
            ultimaVerificacaoTemp[id] = agora;
            double temp = NAN;
            String erroMsg = "";

            // Apenas impressoras 1 e 2 possuem sensores no código original
            if (id == 1) {
                temp = dht1.readTemperature();
                erroMsg = "Erro ao ler temperatura do Sensor 1!";
            } else if (id == 2) {
                temp = dht2.readTemperature();
                erroMsg = "Erro ao ler temperatura do Sensor 2!";
            }
            // Impressoras 3 e 4 não têm sensor definido no código, então ignoramos

            if (!isnan(temp)) {
                Serial.printf("Temperatura Sensor %d: %.2f°C\n", id, temp);
                enviarTemperatura(id, temp);
            } else if (id == 1 || id == 2) {
                // Só mostra erro se for um ID que deveria ter sensor
                Serial.println(erroMsg);
            }
        }
    }
  }
}

// ---------- FUNÇÕES ----------

// Nova função para checar lotação
bool todasImpressorasEstaoOcupadas() {
    if (impressorasOcupadas[1] && impressorasOcupadas[2] && impressorasOcupadas[3] && impressorasOcupadas[4]) {
        return true;
    }
    return false;
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
      
    // --- LÓGICA SOLICITADA ---
    // Verifica se tudo está ocupado ANTES de tentar enviar
    if (todasImpressorasEstaoOcupadas()) {
        Serial.println("\nBLOQUEADO: Todas as impressoras estão em uso.");
        pinBuffer = ""; // Limpa o buffer para impedir envio
        return;         // Sai da função sem chamar a API
    }
    // -------------------------

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
  String url = String(backendUrl) + "/reservas-impressora/liberar";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument docRequest;
  docRequest["pin"] = pin;
  String jsonRequest;
  serializeJson(docRequest, jsonRequest);

  int httpCode = http.POST(jsonRequest);

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Resposta da API: " + payload);

    StaticJsonDocument docResponse;
    DeserializationError error = deserializeJson(docResponse, payload);

    if (error) {
      Serial.println("Falha ao analisar JSON da resposta.");
      return;
    }

    bool confirmacao = docResponse["confirmacao"];
    if (confirmacao) {
      long idRecebido = docResponse["id"].as<long>();
      
      // Validação extra de segurança
      if(idRecebido >= 1 && idRecebido <= 4) {
          Serial.printf("Reserva aprovada para a impressora %ld.\n", idRecebido);
          controlarRele(idRecebido, true);
      } else {
          Serial.println("API retornou um ID de impressora inválido.");
      }

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
  String url = String(backendUrl) + "/reservas-impressora/temperatura";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument docRequest;
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
  // Atualiza o estado global da impressora
  if (id >= 1 && id <= 4) {
      impressorasOcupadas[id] = ligar;
  }

  // Lógica inversa (LOW liga o relé)
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
