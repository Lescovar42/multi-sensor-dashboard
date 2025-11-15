# Wokwi 2 - Sensor Air dan Kontrol Servo

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// Pin Configuration
#define WATER_SENSOR_PIN 32
#define SERVO_PIN 13

// WiFi Credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Configuration
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "WokwiClient2";

// MQTT Topics
const char* topic_water_level = "wokwi/sensor/water_level";
const char* topic_servo_control = "wokwi/control/servo";
const char* topic_servo_status = "wokwi/status/servo";

WiFiClient espClient;
PubSubClient client(espClient);
Servo waterServo;

unsigned long lastMsg = 0;
const long interval = 2000; // Interval pengiriman data (ms)
bool servoState = false; // false = OFF, true = ON

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Parse JSON
  if (String(topic) == topic_servo_control) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (!error) {
      String action = doc["action"];
      
      if (action == "ON") {
        // Putar servo ke 90 derajat (ON)
        waterServo.write(90);
        servoState = true;
        
        // Publish status
        String statusPayload = "{\"status\":\"ON\"}";
        client.publish(topic_servo_status, statusPayload.c_str());
        
        Serial.println("Servo ON - Air mengalir");
      } 
      else if (action == "OFF") {
        // Putar servo ke 0 derajat (OFF)
        waterServo.write(0);
        servoState = false;
        
        // Publish status
        String statusPayload = "{\"status\":\"OFF\"}";
        client.publish(topic_servo_status, statusPayload.c_str());
        
        Serial.println("Servo OFF - Air berhenti");
      }
    } else {
      Serial.println("Failed to parse JSON");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      
      // Subscribe to servo control topic
      client.subscribe(topic_servo_control);
      Serial.print("Subscribed to: ");
      Serial.println(topic_servo_control);
      
      // Publish initial servo status
      String statusPayload = servoState ? "{\"status\":\"ON\"}" : "{\"status\":\"OFF\"}";
      client.publish(topic_servo_status, statusPayload.c_str());
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Servo
  waterServo.attach(SERVO_PIN);
  waterServo.write(0); // Initial position (OFF)
  
  // Setup WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("Wokwi 2 - Water Sensor & Servo Ready!");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    
    // Baca Level Air dari Potentiometer (simulasi sensor air)
    int sensorValue = analogRead(WATER_SENSOR_PIN);
    // Map nilai analog ke persentase (0-100%)
    float waterLevel = map(sensorValue, 0, 4095, 0, 1000) / 10.0;
    
    // Format JSON untuk level air
    String payload = "{\"level\":" + String(waterLevel, 1) + "}";
    Serial.print("Water Level: ");
    Serial.print(waterLevel);
    Serial.print(" % -> ");
    Serial.println(payload);
    
    client.publish(topic_water_level, payload.c_str());
    
    // Display servo status
    Serial.print("Servo Status: ");
    Serial.println(servoState ? "ON" : "OFF");
    Serial.println("---");
  }
}
