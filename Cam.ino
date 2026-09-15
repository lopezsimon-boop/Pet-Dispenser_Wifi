#include "esp_camera.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_http_server.h"

// ==========================================
// 1. CREDENCIALES WI-FI Y MQTT
// ==========================================
const char* ssid = "Galaxy A56 5G 3AA4";       // Cambia por el nombre de tu Wi-Fi 2.4 GHz
const char* password = "matematicas";   // Cambia por la contraseña de tu Wi-Fi

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic_sub = "pet_dispenser_lopezsimon/control";

// Definición de pines de la cámara (Modelo AI-THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Pines de control para comida y agua
const int pinComida = 14; 
const int pinAgua   = 13; 

// Pines de indicación de estado Wi-Fi
const int pinLedRojo  = 12; // Encendido = Desconectado / Buscando Wi-Fi
const int pinLedVerde = 2;  // Encendido = Wi-Fi Conectado y Listo

WiFiClient espClient;
PubSubClient client(espClient);
httpd_handle_t stream_httpd = NULL;

// Servidor de video MJPEG
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=123456789000000000000987654321");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Error al capturar marco de cámara");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, "\r\n--123456789000000000000987654321\r\n", 37);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else {
      break;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

// Escucha comandos MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) mensaje += (char)payload[i];

  if (mensaje == "data1=0") digitalWrite(pinComida, HIGH);
  else if (mensaje == "data1=1") digitalWrite(pinComida, LOW);

  if (mensaje == "data2=0") digitalWrite(pinAgua, HIGH);
  else if (mensaje == "data2=1") digitalWrite(pinAgua, LOW);
}

void setup() {
  Serial.begin(115200);

  // Configuración de salidas
  pinMode(pinComida, OUTPUT);
  pinMode(pinAgua, OUTPUT);
  pinMode(pinLedRojo, OUTPUT);
  pinMode(pinLedVerde, OUTPUT);

  digitalWrite(pinComida, LOW);
  digitalWrite(pinAgua, LOW);
  
  // Estado Inicial: Buscando Wi-Fi (Rojo ON, Verde OFF)
  digitalWrite(pinLedRojo, HIGH);
  digitalWrite(pinLedVerde, LOW);

  // Configuración del sensor de cámara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar cámara: 0x%x", err);
    return;
  }

  // Inicio de conexión Wi-Fi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  while (WiFi.status() != WL_CONNECTED) {
    // Parpadeo suave del LED rojo mientras se conecta
    digitalWrite(pinLedRojo, !digitalRead(pinLedRojo));
    delay(500);
    Serial.print(".");
  }
  
  // Conexión exitosa
  digitalWrite(pinLedRojo, LOW);   // Apaga el LED rojo
  digitalWrite(pinLedVerde, HIGH); // Enciende el LED verde
  
  Serial.println("\nWi-Fi Conectado!");
  Serial.print("IP de la ESP32-CAM: ");
  Serial.println(WiFi.localIP());

  startCameraServer();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Comprobación constante de la conexión Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(pinLedVerde, HIGH);
    digitalWrite(pinLedRojo, LOW);
  } else {
    digitalWrite(pinLedVerde, LOW);
    digitalWrite(pinLedRojo, HIGH);
  }

  if (!client.connected()) {
    String clientId = "ESP32CAM-" + String(WiFi.macAddress());
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_sub);
    } else {
      delay(500);
    }
  }
  client.loop();
}
