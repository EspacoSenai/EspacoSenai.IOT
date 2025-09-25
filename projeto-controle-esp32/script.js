const ESP32_IP = "http://192.168.0.123"; // ajuste para seu IP

function toggleLed(led) {
  fetch(`${ESP32_IP}/led`, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: `led=${led}`,
  })
    .then(res => res.text())
    .then(data => alert(data))
    .catch(err => alert("Erro: " + err));
}

async function fetchTemperature() {
  try {
    const res = await fetch(`${ESP32_IP}/temperature`);
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const data = await res.json();

    document.getElementById("temperatureValue").innerText = Number(data.temperature).toFixed(2);
    document.getElementById("humidityValue").innerText = Number(data.humidity).toFixed(2);
  } catch (err) {
    console.error(err);
    document.getElementById("temperatureValue").innerText = "Erro!";
    document.getElementById("humidityValue").innerText = "Erro!";
    alert("Erro ao obter dados do ESP32: " + err);
  }
}

window.onload = fetchTemperature;
