#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// 1. CONFIGURACIÓN DE RED Y BROKER MQTT
// ==========================================
const char* ssid = "TU_RED_2.4G";       // Cambia por el nombre de tu Wi-Fi 2.4 GHz
const char* password = "TU_CONTRASEÑA";   // Cambia por la contraseña de tu Wi-Fi

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic_sub = "pet_dispenser_lopezsimon/control";

// ==========================================
// 2. ASIGNACIÓN DE PINES
// ==========================================
const int pinComida = 18; // Control de motor paso a paso / comida
const int pinAgua   = 5;  // Control de la electroválvula / agua
const int pinLED    = 2;  // LED azul integrado para estado de conexión

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================
// 3. FUNCIÓN RECEPCIÓN DE MENSAJES (CALLBACK)
// ==========================================
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  Serial.print("Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  // --- LÓGICA DE CONTROL ---
  
  // Activar / Detener Comida (Pin 18)
  if (mensaje == "data1=0") {
    digitalWrite(pinComida, HIGH);
    Serial.println("-> Orden: Sirviendo Comida (Pin 18 ACTIVADO)");
  } 
  else if (mensaje == "data1=1") {
    digitalWrite(pinComida, LOW);
    Serial.println("-> Orden: Deteniendo Comida (Pin 18 DESACTIVADO)");
  }

  // Activar / Detener Agua (Pin 5)
  if (mensaje == "data2=0") {
    digitalWrite(pinAgua, HIGH);
    Serial.println("-> Orden: Servir Agua (Pin 5 ACTIVADO)");
  } 
  else if (mensaje == "data2=1") {
    digitalWrite(pinAgua, LOW);
    Serial.println("-> Orden: Deteniendo Agua (Pin 5 DESACTIVADO)");
  }
}

// ==========================================
// 4. CONEXIÓN A WI-FI Y RECONEXIÓN MQTT
// ==========================================
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("¡Wi-Fi Conectado!");
  Serial.print("Dirección IP asignada: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar al Broker MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" ¡Conectado con éxito!");
      // Suscribirse al canal de control
      client.subscribe(topic_sub);
      Serial.print("Suscrito al canal: ");
      Serial.println(topic_sub);
      digitalWrite(pinLED, HIGH); // LED Azul encendido indica conexión MQTT activa
    } else {
      Serial.print("Falló la conexión. Código de error: ");
      Serial.print(client.state());
      Serial.println(". Reintentando en 5 segundos...");
      digitalWrite(pinLED, LOW);
      delay(5000);
    }
  }
}

// ==========================================
// 5. CONFIGURACIÓN INICIAL (SETUP)
// ==========================================
void setup() {
  Serial.begin(115200);

  // Configurar pines como salidas
  pinMode(pinComida, OUTPUT);
  pinMode(pinAgua, OUTPUT);
  pinMode(pinLED, OUTPUT);

  // Estado inicial: todo apagado
  digitalWrite(pinComida, LOW);
  digitalWrite(pinAgua, LOW);
  digitalWrite(pinLED, LOW);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ==========================================
// 6. BUCLE PRINCIPAL (LOOP)
// ==========================================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantiene activa la comunicación en segundo plano
}
