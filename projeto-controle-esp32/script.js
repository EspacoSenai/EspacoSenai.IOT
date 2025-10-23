// ⚠️ Substitua pelo IP real da sua ESP32
const ESP32_IP = "http://192.168.0.123";

// ---------------- LED ----------------
function toggleLed(led) {
  fetch(`${ESP32_IP}/led`, {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
    body: `led=${led}`,
  })
    .then((response) => response.text())
    .then((data) => alert(data))
    .catch((err) => alert("Erro ao conectar: " + err));
}

// ---------------- TEMPERATURA + UMIDADE ----------------
function fetchTemperature() {
  fetch(`${ESP32_IP}/temperature`)
    .then((response) => response.json())
    .then((data) => {
      const temp = data.temperature;
      const hum = data.humidity;
      document.getElementById("temperatureValue").innerText = temp.toFixed(2);
      document.getElementById("humidityValue").innerText = hum.toFixed(2);
    })
    .catch((err) => {
      console.error(err);
      document.getElementById("temperatureValue").innerText = "Erro";
      document.getElementById("humidityValue").innerText = "Erro";
    });
}

// ---------------- ENERGIA (boolean) ----------------
function fetchEnergia() {
  fetch(`${ESP32_IP}/energia`)
    .then((response) => response.json())
    .then((data) => {
      const energia = data.energia;
      const statusEl = document.getElementById("energiaStatus");
      statusEl.innerText = energia ? "ATIVA" : "DESLIGADA";
      statusEl.style.color = energia ? "green" : "red";
    })
    .catch((err) => {
      console.error("Erro ao obter energia:", err);
      const statusEl = document.getElementById("energiaStatus");
      statusEl.innerText = "Erro";
      statusEl.style.color = "gray";
    });
}

// ---------------- ENVIAR PIN MANUALMENTE ----------------
function enviarPin() {
  const pin = document.getElementById("pinInput").value.trim();

  if (pin.length !== 4 || isNaN(pin)) {
    alert("Digite um PIN válido de 4 dígitos!");
    return;
  }

  fetch(`${ESP32_IP}/pin`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ pin }),
  })
    .then((response) => response.json())
    .then((data) => {
      const energia = data.energia;
      alert(`PIN enviado! Energia: ${energia ? "ATIVA" : "DESLIGADA"}`);
      fetchEnergia(); // atualiza status na tela
    })
    .catch((err) => {
      console.error(err);
      alert("Erro ao enviar PIN: " + err);
    });
}

// ---------------- AUTO-ATUALIZAÇÃO ----------------
function atualizarTudo() {
  fetchTemperature();
  fetchEnergia();
}

// Carrega tudo ao iniciar
window.onload = function() {
  atualizarTudo();
  setInterval(atualizarTudo, 5000); // Atualiza a cada 5s
};
