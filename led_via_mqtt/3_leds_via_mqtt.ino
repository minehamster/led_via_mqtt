#include "WiFi.h"
#include "PubSubClient.h"
#include "ArduinoJson.h"

const int blue_led = 15;
const int red_led = 14;
const int white_led = 13;

WiFiClient espClient;
PubSubClient client(espClient);

// WiFi настройки
const char* ssid = "iPhone Andrey Shiz";
const char* password = "11111111";

// MQTT настройки
const char* mqtt_server = "172.20.10.4";  // IP адрес MQTT брокера
const int mqtt_port = 1883;
const char* mqtt_topic = "iotik32/commands";
const char* device_id = "iotik32_b";  // Уникальный ID устройства


bool blueLedState = false;
bool redLedState = false;
bool whiteLedState = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;  // 30 секунд

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
  const char* state = doc["state"];
  const char* ledName = doc["device_id"];
  // Синий светодиод
  if ((strcmp(ledName, "blue") == 0) && strcmp(state, "on") == 0) {
    digitalWrite(blue_led, HIGH);
    blueLedState = true;
    Serial.println("Blue LED on");
  } else if (strcmp(state, "off") == 0) {
    digitalWrite(blue_led, LOW);
    blueLedState = false;
    Serial.println("Blue LED off");
  }
  // Красный светодиод
  if ((strcmp(ledName, "red") == 0) && strcmp(state, "on") == 0) {
    digitalWrite(red_led, HIGH);
    redLedState = true;
    Serial.println("Red LED on");
  } else if (strcmp(state, "off") == 0) {
    digitalWrite(red_led, LOW);
    redLedState = false;
    Serial.println("Red LED off");
  }
  // Белый светодиод
  if ((strcmp(ledName, "white") == 0) && strcmp(state, "on") == 0) {
    digitalWrite(white_led, HIGH);
    whiteLedState = true;
    Serial.println("White LED on");
  } else if (strcmp(state, "off") == 0) {
    digitalWrite(white_led, LOW);
    whiteLedState = false;
    Serial.println("White LED off");
  }
  sendStatusUpdate();
}

void sendStatusUpdate() {
  DynamicJsonDocument doc(1024);
  doc["blue_led_state"] = blueLedState ? "on" : "off";
  doc["red_led_state"] = redLedState ? "on" : "off";
  doc["white_led_state"] = whiteLedState ? "on" : "off";

  String statusMessage;
  serializeJson(doc, statusMessage);

  String statusTopic = String("iotik32/status");
  client.publish(statusTopic.c_str(), statusMessage.c_str());

  Serial.println("Status update sent: " + statusMessage);
}

boolean reconnect() {
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);

  Serial.print("Attempting MQTT connection...");

  if (client.connect(clientId.c_str())) {
    Serial.println("connected");

    // Подписываемся на топик команд
    client.subscribe(mqtt_topic, 1);
    Serial.print("Subscribed to: ");
    Serial.println(mqtt_topic);

    // Отправляем сообщение о подключении
    DynamicJsonDocument doc(1024);
    doc["status"] = "online";

    String onlineMessage;
    serializeJson(doc, onlineMessage);

    String onlineTopic = String("iotik32/status");
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
  Serial.begin(115200);
  // Инициальзация GPIO
  pinMode(blue_led, OUTPUT);
  pinMode(red_led, OUTPUT);
  pinMode(white_led, OUTPUT);
  digitalWrite(blue_led, LOW);
  digitalWrite(red_led, LOW);
  digitalWrite(white_led, LOW);
  // Подключение к WiFi
  setup_wifi();

  // Настройка MQTT клиента
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("IoTik32 LED Controller started");
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
