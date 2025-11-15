# Wokwi 1 - Sensor Suhu Udara dan Tanah

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// Pin Configuration
#define DHTPIN 15
#define DHTTYPE DHT22
#define SOIL_TEMP_PIN 34

// WiFi Credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Configuration
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "WokwiClient1";

// MQTT Topics
const char* topic_temp_air = "wokwi/sensor/temp_air";
const char* topic_temp_soil = "wokwi/sensor/temp_soil";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

unsigned long lastMsg = 0;
const long interval = 2000; // Interval pengiriman data (ms)

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

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
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
  
  // Initialize DHT sensor
  dht.begin();
  
  // Setup WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("Wokwi 1 - Sensor Suhu Ready!");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    
    // Baca Suhu Udara dari DHT22
    float temp_air = dht.readTemperature();
    
    if (!isnan(temp_air)) {
      // Format JSON untuk suhu udara
      String payload_air = "{\"temperature\":" + String(temp_air, 1) + "}";
      Serial.print("Suhu Udara: ");
      Serial.print(temp_air);
      Serial.print(" °C -> ");
      Serial.println(payload_air);
      
      client.publish(topic_temp_air, payload_air.c_str());
    } else {
      Serial.println("Failed to read from DHT sensor!");
    }
    
    // Baca Suhu Tanah dari NTC (simulasi menggunakan analog)
    int sensorValue = analogRead(SOIL_TEMP_PIN);
    // Map nilai analog ke rentang suhu realistis (15-35°C)
    float temp_soil = map(sensorValue, 0, 4095, 150, 350) / 10.0;
    
    // Format JSON untuk suhu tanah
    String payload_soil = "{\"temperature\":" + String(temp_soil, 1) + "}";
    Serial.print("Suhu Tanah: ");
    Serial.print(temp_soil);
    Serial.print(" °C -> ");
    Serial.println(payload_soil);
    
    client.publish(topic_temp_soil, payload_soil.c_str());
    
    Serial.println("---");
  }
}
