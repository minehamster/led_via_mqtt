#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi настройки
const char* ssid = "iPhone Andrey Shiz";
const char* password = "11111111";

// MQTT настройки
const char* mqtt_server = "172.20.10.4";  // IP адрес MQTT брокера
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/led/command";
const char* device_id = "esp32_01";  // Уникальный ID устройства

// GPIO настройки
const int LED_PIN = 13;  // Встроенный LED
const int EXTERNAL_LED_PIN = 18;  // Внешний LED

// Объекты для WiFi и MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Переменные состояния
bool ledState = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000; // 30 секунд

// Функция подключения к WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Callback функция для обработки MQTT сообщений
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Создаем строку из payload
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Парсим JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Проверяем, предназначено ли сообщение для этого устройства
  const char* target_device = doc["device_id"];
  if (target_device && strcmp(target_device, device_id) == 0) {
    const char* state = doc["state"];
    
    if (state) {
      if (strcmp(state, "on") == 0) {
        ledState = true;
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(EXTERNAL_LED_PIN, HIGH);
        Serial.println("LED turned ON");
      } else if (strcmp(state, "off") == 0) {
        ledState = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(EXTERNAL_LED_PIN, LOW);
        Serial.println("LED turned OFF");
      }
      
      // Отправляем подтверждение
      sendStatusUpdate();
    }
  }
}

// Функция отправки обновления статуса
void sendStatusUpdate() {
  DynamicJsonDocument doc(1024);
  doc["device_id"] = device_id;
  doc["state"] = ledState ? "on" : "off";
  doc["timestamp"] = millis();
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();

  String statusMessage;
  serializeJson(doc, statusMessage);

  String statusTopic = String("esp32/led/status/") + device_id;
  client.publish(statusTopic.c_str(), statusMessage.c_str());
  
  Serial.println("Status update sent: " + statusMessage);
}
// Функция переподключения к MQTT
boolean reconnect() {
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);
  
  Serial.print("Attempting MQTT connection...");
  
  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    
    // Подписываемся на топик команд
    client.subscribe(mqtt_topic);
    Serial.print("Subscribed to: ");
    Serial.println(mqtt_topic);
    
    // Отправляем сообщение о подключении
    DynamicJsonDocument doc(1024);
    doc["device_id"] = device_id;
    doc["status"] = "online";
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    
    String onlineMessage;
    serializeJson(doc, onlineMessage);
    
    String onlineTopic = String("esp32/device/online/") + device_id;
    client.publish(onlineTopic.c_str(), onlineMessage.c_str());
    
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    return false;
  }
}
void setup() {
  // Инициализация Serial порта
  Serial.begin(115200);
  
  // Настройка GPIO
  pinMode(LED_PIN, OUTPUT);
  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(EXTERNAL_LED_PIN, LOW);
  
  // Подключение к WiFi
  setup_wifi();
  
  // Настройка MQTT клиента
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("ESP32 LED Controller started");
  Serial.print("Device ID: ");
  Serial.println(device_id);
}

void loop() {
  // Проверяем подключение к WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    setup_wifi();
  }
  
  // Обрабатываем MQTT
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
    
    // Отправляем heartbeat каждые 30 секунд
    unsigned long now = millis();
    if (now - lastHeartbeat > heartbeatInterval) {
      lastHeartbeat = now;
      sendStatusUpdate();
    }
  }
  
  delay(100);
}