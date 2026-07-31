#include <WiFi.h>
#include <WebServer.h>

// Credenciales para la red propia de la ESP32
const char* ssid = "DispensadorMascota_Pro";
const char* password = ""; // Sin contraseña para acceso rápido institucional

// Pines de Salida
const int PIN_RELE_AGUA = 18;
const int LED_AGUA = 4;
const int LED_COMIDA = 2; // LED Azul integrado

// Pines Motor Paso a Paso (In1, In2, In3, In4)
const int IN1 = 19;
const int IN2 = 21;
const int IN3 = 22;
const int IN4 = 23;

// Estados de Control
bool aguaActiva = false;
bool comidaActiva = false;

WebServer server(80);

// Secuencia para motor ULN2003
const int pasos[8][4] = {
  {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
  {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};
int pasoActual = 0;

// Interfaz Web - Réplica exacta de tus diapositivas
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang='es'><head>
<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
  body { font-family: Arial, sans-serif; margin: 0; padding: 0; text-align: center; background-color: #fff; }
  .header { background-color: #5b9bd5; height: 60px; display: flex; align-items: center; justify-content: space-between; padding: 0 20px; color: white; border-bottom: 3px solid #3a75ad; }
  .nav { font-weight: bold; font-size: 18px; }
  .nav span { border-right: 2px solid white; padding: 0 15px; }
  .auth-btns { display: flex; gap: 10px; }
  .auth-btn { background-color: #2c3e50; color: white; padding: 8px 15px; border-radius: 5px; font-size: 14px; text-decoration: none; }
  .content { padding: 40px 20px; }
  .title { font-size: 28px; font-weight: bold; font-style: italic; margin-bottom: 30px; color: #000; }
  .cam-placeholder { background-color: #828282; width: 80%; max-width: 800px; height: 350px; margin: 0 auto 30px; border: 1px solid #000; display: flex; align-items: center; justify-content: center; color: white; font-size: 20px; }
  .btn-group { display: flex; justify-content: center; gap: 20px; }
  .btn { width: 220px; padding: 15px; border-radius: 10px; font-weight: bold; font-size: 16px; border: none; cursor: pointer; color: white; text-decoration: none; }
  .btn-blue { background-color: #5b9bd5; border-bottom: 4px solid #3a75ad; }
  .btn-red { background-color: #e11d48; border-bottom: 4px solid #9f1239; }
</style>
</head><body>
  <div class='header'>
    <div class='nav'><span>Inicio</span><span>Nosotros</span><span>Proyecto</span><span style='border:none'>Contacto</span></div>
    <div class='auth-btns'><div class='auth-btn'>Ingresar</div><div class='auth-btn'>Registrarse</div></div>
  </div>
  <div class='content'>
    <div class='title'>Nutrición e innovación para el bienestar de tu mascota</div>
    <div class='cam-placeholder'>Monitoreo / Imagen de la mascota</div>
    <div class='btn-group'>
      <a id='btnAgua' href='/toggleAgua' class='btn'>Cargando...</a>
      <button class='btn btn-blue'>Automatico</button>
      <a id='btnComida' href='/toggleComida' class='btn'>Cargando...</a>
    </div>
  </div>
  <script>
    function updateUI() {
      fetch('/status').then(r => r.json()).then(data => {
        const bA = document.getElementById('btnAgua');
        const bC = document.getElementById('btnComida');
        bA.innerText = data.agua ? 'Detener Agua' : 'Agua / Dispensar';
        bA.className = data.agua ? 'btn btn-red' : 'btn btn-blue';
        bC.innerText = data.comida ? 'Detener Comida' : 'Comida / Dispensar';
        bC.className = data.comida ? 'btn btn-red' : 'btn btn-blue';
      });
    }
    setInterval(updateUI, 1000);
    updateUI();
  </script>
</body></html>
)rawliteral";

void handleStatus() {
  String json = "{\"agua\":" + String(aguaActiva) + ",\"comida\":" + String(comidaActiva) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  pinMode(PIN_RELE_AGUA, OUTPUT); pinMode(LED_AGUA, OUTPUT);
  pinMode(LED_COMIDA, OUTPUT); pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  WiFi.softAP(ssid, password);
  server.on("/", []() { server.send(200, "text/html", INDEX_HTML); });
  server.on("/toggleAgua", []() { aguaActiva = !aguaActiva; server.send(303); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/toggleComida", []() { comidaActiva = !comidaActiva; server.send(303); server.sendHeader("Location", "/"); server.send(302); });
  server.on("/status", handleStatus);
  server.begin();
}

void loop() {
  server.handleClient();

  // Control AGUA
  digitalWrite(PIN_RELE_AGUA, aguaActiva ? HIGH : LOW);
  digitalWrite(LED_AGUA, aguaActiva ? HIGH : LOW);

  // Control COMIDA
  if (comidaActiva) {
    digitalWrite(LED_COMIDA, HIGH);
    for (int p = 0; p < 8; p++) {
      digitalWrite(IN1, pasos[p][0]); digitalWrite(IN2, pasos[p][1]);
      digitalWrite(IN3, pasos[p][2]); digitalWrite(IN4, pasos[p][3]);
      delayMicroseconds(1000);
    }
  } else {
    digitalWrite(LED_COMIDA, LOW);
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  }
}
Q   