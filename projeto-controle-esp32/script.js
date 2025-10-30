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
    .then((data) => alert(`LED ${led.toUpperCase()} -> ${data}`))
    .catch((err) => alert("Erro ao conectar: " + err));
}

// ---------------- TEMPERATURA ----------------
function fetchTemperature() {
  fetch(`${ESP32_IP}/temp`)
    .then((response) => response.json())
    .then((data) => {
      const temp = data.temperatura;
      document.getElementById("temperatureValue").innerText = temp.toFixed(1);
    })
    .catch((err) => {
      console.error(err);
      document.getElementById("temperatureValue").innerText = "Erro";
    });
}

// ---------------- ENVIAR PIN ----------------
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
    .then(() => {
      alert(`PIN ${pin} enviado com sucesso!`);
      document.getElementById("pinInput").value = "";
    })
    .catch((err) => {
      console.error(err);
      alert("Erro ao enviar PIN: " + err);
    });
}

// ---------------- AUTO-ATUALIZAÇÃO ----------------
window.onload = function() {
  fetchTemperature();
  setInterval(fetchTemperature, 5000); // Atualiza a cada 5s
};
