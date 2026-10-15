#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Familia Lopez 2.4";
const char* password = "3102665118";

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic_sub = "pet_dispenser_lopezsimon/control";

const char* topicEstado = "pet_dispenser_lopezsimon/estado";
const char* topicConexion = "pet_dispenser_lopezsimon/conexion";

// Asignación de pines en el ESP32
const int pinMotorAbrir = 2;   // L293D pin 2 (1A)
const int pinMotorCerrar = 4;  // L293D pin 7 (2A)
const int pinAgua = 15;        // Driver de la bomba de agua
const int pinLedAgua = 1;      // LED indicación agua
const int pinLedComida = 14;   // LED indicación comida

// Tiempos ajustables de movimiento en milisegundos (1000 = 1 segundo)
const unsigned long TIEMPO_ABRIR_MS = 1000;
const unsigned long TIEMPO_CERRAR_MS = 1000;
const unsigned long PAUSA_INVERSION_MS = 100;

WiFiClient espClient;
PubSubClient client(espClient);

enum Puerta { DESCONOCIDA, CERRADA, ABIERTA, ABRIENDO, CERRANDO };
Puerta puerta = DESCONOCIDA;

bool agua = false;
bool movimientoPendiente = false;
bool destinoAbierto = false;
unsigned long inicioMovimiento = 0;
unsigned long inicioPausa = 0;
unsigned long ultimoIntento = 0;

void detenerMotor() {
  digitalWrite(pinMotorAbrir, LOW);
  digitalWrite(pinMotorCerrar, LOW);
}

void publicarEstado() {
  if (!client.connected()) return;
  const char* nombre = "desconocida";
  if (movimientoPendiente) nombre = destinoAbierto ? "abriendo" : "cerrando";
  else if (puerta == CERRADA) nombre = "cerrada";
  else if (puerta == ABIERTA) nombre = "abierta";
  else if (puerta == ABRIENDO) nombre = "abriendo";
  else if (puerta == CERRANDO) nombre = "cerrando";
  
  String estado = String("{\"agua\":") + (agua ? "true" : "false") +
                  ",\"puerta\":\"" + nombre + "\"}";
  client.publish(topicEstado, estado.c_str(), true);
}

void controlarAgua(bool encender) {
  agua = encender;
  digitalWrite(pinAgua, agua ? HIGH : LOW);
  digitalWrite(pinLedAgua, agua ? HIGH : LOW);
  publicarEstado();
}

void controlarPuerta(bool abrir) {
  if (movimientoPendiente && destinoAbierto == abrir) return;
  if (!movimientoPendiente &&
      ((abrir && (puerta == ABIERTA || puerta == ABRIENDO)) ||
       (!abrir && (puerta == CERRADA || puerta == CERRANDO)))) return;
       
  detenerMotor();
  puerta = DESCONOCIDA;
  destinoAbierto = abrir;
  movimientoPendiente = true;
  inicioPausa = millis();
  digitalWrite(pinLedComida, HIGH);
  publicarEstado();
}

void actualizarPuerta() {
  if (movimientoPendiente) {
    if (millis() - inicioPausa < PAUSA_INVERSION_MS) return;
    movimientoPendiente = false;
    digitalWrite(pinMotorAbrir, destinoAbierto ? HIGH : LOW);
    digitalWrite(pinMotorCerrar, destinoAbierto ? LOW : HIGH);
    puerta = destinoAbierto ? ABRIENDO : CERRANDO;
    inicioMovimiento = millis();
    publicarEstado();
  }
  
  if (puerta != ABRIENDO && puerta != CERRANDO) return;
  
  unsigned long tiempo = puerta == ABRIENDO ? TIEMPO_ABRIR_MS : TIEMPO_CERRAR_MS;
  if (millis() - inicioMovimiento >= tiempo) {
    bool abierta = puerta == ABRIENDO;
    detenerMotor();
    puerta = abierta ? ABIERTA : CERRADA;
    digitalWrite(pinLedComida, abierta ? HIGH : LOW);
    publicarEstado();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje;
  for (unsigned int i = 0; i < length; i++) mensaje += (char)payload[i];
  
  if (mensaje == "data1=0") controlarPuerta(true);       // Orden de abrir
  else if (mensaje == "data1=1") controlarPuerta(false);  // Orden de cerrar
  else if (mensaje == "data2=0") controlarAgua(true);
  else if (mensaje == "data2=1") controlarAgua(false);
  else if (mensaje == "estado?") publicarEstado();
}

void setup() {
  const int pines[] = {pinMotorAbrir, pinMotorCerrar, pinAgua, pinLedAgua, pinLedComida};
  for (int pin : pines) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setSocketTimeout(1);
}

void loop() {
  actualizarPuerta();
  
  if (WiFi.status() != WL_CONNECTED || !client.connected()) {
    if (agua) controlarAgua(false);
    if (movimientoPendiente || puerta == ABRIENDO || puerta == CERRANDO) {
      detenerMotor();
      movimientoPendiente = false;
      puerta = DESCONOCIDA;
      digitalWrite(pinLedComida, LOW);
    }
    if (WiFi.status() == WL_CONNECTED && millis() - ultimoIntento >= 5000) {
      ultimoIntento = millis();
      String id = "PetFeeder-" + WiFi.macAddress();
      if (client.connect(id.c_str(), topicConexion, 0, true, "offline")) {
        client.subscribe(topic_sub);
        client.publish(topicConexion, "online", true);
        publicarEstado();
      }
    }
    return;
  }
  client.loop();
}
