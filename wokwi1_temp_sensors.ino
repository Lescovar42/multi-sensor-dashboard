/*
 * WOKWI 1 - TEMPERATURE SENSOR NODE
 * Multi-Sensor IoT Dashboard Project
 * Sensors: DHT22 (Air Temp) + NTC Temperature Sensor (Soil Temp)
 * 
 * MQTT Topics Published:
 * - wokwi/sensor/temp_air    : {"temperature": 25.5}
 * - wokwi/sensor/temp_soil   : {"temperature": 23.2}
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <math.h>  // For NTC calculations

// ========== WiFi Configuration ==========
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ========== MQTT Configuration ==========
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "WokwiClient1";

// ========== MQTT Topics ==========
const char* TOPIC_TEMP_AIR = "irrigation/sensor/environment";
const char* TOPIC_TEMP_SOIL = "irrigation/sensor/soil";

// ========== Sensor Pins ==========
#define DHTPIN 15              // DHT22 sensor pin
#define DHTTYPE DHT22          // DHT22 sensor type
#define SOIL_TEMP_PIN 34       // NTC Temperature Sensor (ADC pin)

// ========== NTC Thermistor Constants ==========
#define SERIES_RESISTOR 10000   // 10K Ohm series resistor
#define NTC_NOMINAL 10000       // NTC resistance at 25°C (10K)
#define TEMP_NOMINAL 25         // Nominal temperature (25°C)
#define B_COEFFICIENT 3950      // Beta coefficient (typical 3950)
#define ADC_MAX 4095.0          // ESP32 12-bit ADC
#define VCC 3.3                 // Supply voltage

// ========== Initialize Objects ==========
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// ========== Timing ==========
unsigned long lastPublish = 0;
const long publishInterval = 2000;  // Publish every 2 seconds

// ========== WiFi Setup ==========
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
}

// ========== MQTT Reconnect ==========
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// ========== Read NTC Temperature ==========
float readNTCTemperature() {
  // Average multiple readings for stability
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(SOIL_TEMP_PIN);
    delay(10);
  }
  int rawADC = sum / 10;
  
  // Convert ADC to voltage
  float voltage = (rawADC / ADC_MAX) * VCC;
  
  // Calculate NTC resistance using voltage divider
  // R_NTC = R_series * (Vin/Vout - 1)
  float resistance;
  if (voltage > 0.01) {
    resistance = SERIES_RESISTOR * ((VCC / voltage) - 1.0);
  } else {
    resistance = NTC_NOMINAL;
  }
  
  // Steinhart-Hart Simplified (Beta equation)
  // 1/T = 1/T0 + (1/B) * ln(R/R0)
  float steinhart;
  steinhart = resistance / NTC_NOMINAL;          // (R/R0)
  steinhart = log(steinhart);                    // ln(R/R0)
  steinhart /= B_COEFFICIENT;                    // 1/B * ln(R/R0)
  steinhart += 1.0 / (TEMP_NOMINAL + 273.15);    // + (1/T0)
  steinhart = 1.0 / steinhart;                   // Invert
  steinhart -= 273.15;                           // Convert to Celsius
  
  return steinhart;
}

// ========== Publish Sensor Data ==========
void publishSensorData() {
  // Read DHT22 (Air Temperature & Humidity)
  float temp_air = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Read NTC (Soil Temperature)
  float temp_soil = readNTCTemperature();
  
  // Validate DHT22 readings
  if (!isnan(temp_air) && !isnan(humidity)) {
    // Create JSON payload for air temperature
    StaticJsonDocument<100> docAir;
    docAir["temperature"] = round(temp_air * 10) / 10.0;  // 1 decimal
    
    char jsonBufferAir[100];
    serializeJson(docAir, jsonBufferAir);
    
    // Publish air temperature
    if (client.publish(TOPIC_TEMP_AIR, jsonBufferAir)) {
      Serial.print("Air Temp: ");
      Serial.println(jsonBufferAir);
    } else {
      Serial.println("Failed to publish air temp");
    }
  } else {
    Serial.println("Failed to read DHT22 sensor");
  }
  
  // Validate NTC reading (reasonable range for soil: 0-50°C)
  if (temp_soil > -10 && temp_soil < 60) {
    // Create JSON payload for soil temperature
    StaticJsonDocument<100> docSoil;
    docSoil["temperature"] = round(temp_soil * 10) / 10.0;  // 1 decimal
    
    char jsonBufferSoil[100];
    serializeJson(docSoil, jsonBufferSoil);
    
    // Publish soil temperature
    if (client.publish(TOPIC_TEMP_SOIL, jsonBufferSoil)) {
      Serial.print("Soil Temp: ");
      Serial.println(jsonBufferSoil);
    } else {
      Serial.println("Failed to publish soil temp");
    }
  } else {
    Serial.println("NTC reading out of range");
  }
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize DHT22
  dht.begin();

  // Initialize NTC pin
  pinMode(SOIL_TEMP_PIN, INPUT);
  analogReadResolution(12);  // 12-bit ADC
  
  // Connect to WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  
  Serial.println("\nSystem ready! Publishing data...\n");
}

// ========== Main Loop ==========
void loop() {
  // Ensure WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    setup_wifi();
  }
  
  // Ensure MQTT is connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Publish data at interval
  unsigned long currentMillis = millis();
  if (currentMillis - lastPublish >= publishInterval) {
    lastPublish = currentMillis;
    publishSensorData();
  }
}