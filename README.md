# 💡 Projeto de Controle de Dispositivos com ESP32 via Web

Este projeto permite o controle remoto de **LEDs** e um **relé** conectados a uma **ESP32**, utilizando uma interface web simples, acessível via navegador.

---

## 📦 Tecnologias Utilizadas

- ESP32 (placa de desenvolvimento)
- HTML, CSS e JavaScript (interface web)
- Arduino IDE (para programar a ESP32)
- Biblioteca `WiFi.h`, `WebServer.h`, `Keypad.h`, `LiquidCrystal.h`

---

## ⚙️ Funcionalidades

- Conexão da ESP32 à rede Wi-Fi com IP fixo
- Leitura de teclado matricial 4x3
- Controle de:
  - LED Vermelho
  - LED Verde
  - LED Amarelo
  - RELE4
- Interface web para controle remoto dos dispositivos
- Feedback em display LCD 16x4

---

## 🛠 Estrutura do Projeto

projeto-controle-esp32/
│
├── index.html # Interface HTML
├── style.css # Estilos visuais
├── script.js # Lógica para enviar comandos via HTTP
└── README.md # Instruções e explicações
