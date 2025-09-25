// ⚠️ Substitua pelo IP real da sua ESP32
const ESP32_IP = "http://192.168.0.123";  // Substitua com o IP real

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

// Função para buscar a temperatura do servidor ESP32
function fetchTemperature() {
  fetch(`${ESP32_IP}/temperature`)
    .then((response) => response.json())
    .then((data) => {
      const temp = data.temperature;
      document.getElementById("temperatureValue").innerText = temp.toFixed(2);  // Atualiza o valor da temperatura
    })
    .catch((err) => {
      alert("Erro ao obter a temperatura: " + err);
      document.getElementById("temperatureValue").innerText = "Erro!";
    });
}

// Carregar a temperatura automaticamente ao carregar a página
window.onload = fetchTemperature;
