// ⚠️ Substitua pelo IP real da sua ESP32
const ESP32_IP = "http://192.168.0.123";

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
