#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// 引脚定义
#define DHTPIN 4
#define DHTTYPE DHT22
#define SOIL_PIN 34 // 土壤湿度传感器（电位器）
#define LIGHT_PIN 35 // 光敏电阻
#define LED_PIN 2 // 执行器

DHT dht(DHTPIN, DHTTYPE);

// WiFi配置（Wokwi专用）
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT配置
const char* mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* client_id = "esp32_greenhouse_1";

// 主题（按照设计文档）
const char* topic_temp = "team1/greenhouse/1/sensor/temperature";
const char* topic_humid = "team1/greenhouse/1/sensor/humidity";
const char* topic_soil = "team1/greenhouse/1/sensor/soil_moisture";
const char* topic_light = "team1/greenhouse/1/sensor/light";
const char* topic_actuator = "team1/greenhouse/1/actuator/led";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastPublishTime = 0;
const long publishInterval = 5000;

// 传感器读取函数
float readTemperature() {
  float t = dht.readTemperature();
  if (isnan(t)) return -1;
  return t;
}
float readHumidity() {
  float h = dht.readHumidity();
  if (isnan(h)) return -1;
  return h;
}
int readSoilMoisture() {
  int adc = analogRead(SOIL_PIN);
  return constrain(map(adc, 0, 4095, 0, 100), 0, 100);
}
int readLightIntensity() {
  int adc = analogRead(LIGHT_PIN);
  return constrain(map(adc, 0, 4095, 0, 100), 0, 100);
}

// 发布传感器数据（JSON格式）
void publishSensorData() {
  unsigned long now = millis();

  float temp = readTemperature();
  if (temp != -1) {
    String payload = "{\"device_id\":\"greenhouse_1\",\"sensor\":\"temperature\",\"value\":" + String(temp) + ",\"timestamp\":" + String(now) + ",\"unit\":\"celsius\"}";
    mqttClient.publish(topic_temp, payload.c_str());
    Serial.println("📤 温度: " + payload);
  }

  float hum = readHumidity();
  if (hum != -1) {
    String payload = "{\"device_id\":\"greenhouse_1\",\"sensor\":\"humidity\",\"value\":" + String(hum) + ",\"timestamp\":" + String(now) + ",\"unit\":\"%\"}";
    mqttClient.publish(topic_humid, payload.c_str());
    Serial.println("📤 湿度: " + payload);
  }

  int soil = readSoilMoisture();
  String soilPayload = "{\"device_id\":\"greenhouse_1\",\"sensor\":\"soil_moisture\",\"value\":" + String(soil) + ",\"timestamp\":" + String(now) + ",\"unit\":\"%\"}";
  mqttClient.publish(topic_soil, soilPayload.c_str());
  Serial.println("📤 土壤湿度: " + soilPayload);

  int light = readLightIntensity();
  String lightPayload = "{\"device_id\":\"greenhouse_1\",\"sensor\":\"light_intensity\",\"value\":" + String(light) + ",\"timestamp\":" + String(now) + ",\"unit\":\"%\"}";
  mqttClient.publish(topic_light, lightPayload.c_str());
  Serial.println("📤 光照强度: " + lightPayload);

  Serial.println("-------------------------------------");
}

// MQTT回调函数：处理远程控制指令
void mqttCallback(char* topic, byte* payload, unsigned int length) { // <--- 注意这个花括号
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  Serial.print("📨 收到指令 [主题:" + String(topic) + "] 内容:" + message);

  if (String(topic) == topic_actuator) {
    String cmd = message;
    cmd.toLowerCase();
    bool ledState = false;
    if (cmd == "on" || cmd.indexOf("\"state\":\"on\"") != -1) ledState = true;
    else if (cmd == "off" || cmd.indexOf("\"state\":\"off\"") != -1) ledState = false;
    else return;

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    Serial.println(ledState ? " 💡 LED点亮（水泵/风扇开启）" : " 💡 LED熄灭（水泵/风扇关闭）");

    // 发送状态反馈（符合设计文档的 status 主题）
    String feedbackTopic = "team1/greenhouse/1/status/led";
    String feedback = "{\"device_id\":\"greenhouse_1\",\"actuator\":\"led\",\"state\":" + String(ledState ? "\"on\"" : "\"off\"") + ",\"timestamp\":" + String(millis()) + "}";
    mqttClient.publish(feedbackTopic.c_str(), feedback.c_str());
    Serial.println("🔄 状态反馈已发送: " + feedback);
  }
}

// 连接WiFi
void connectWiFi() {
  Serial.print("📡 连接WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi已连接，IP:" + WiFi.localIP().toString());
}

// 连接MQTT
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("🔌 连接HiveMQ...");
    if (mqttClient.connect(client_id)) {
      Serial.println("✅ 已连接");
      mqttClient.subscribe(topic_actuator);
      Serial.println("📡 已订阅控制主题: " + String(topic_actuator));
    } else {
      Serial.println("❌ 失败，5秒后重试");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();
  analogReadResolution(12);

  Serial.println("\n🌱 智慧农业大棚系统 - 设备端启动");
  connectWiFi();
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  if (millis() - lastPublishTime >= publishInterval) {
    lastPublishTime = millis();
    publishSensorData();
  }
  delay(100);
}
