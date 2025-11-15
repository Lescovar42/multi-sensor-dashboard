#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Konfigurasi Wi-Fi & MQTT ---
const char* ssid = "Wokwi-GUEST";
const char* password = ""; 
const char* mqtt_server = "broker.hivemq.com"; 

// Topics MQTT
#define TOPIC_PUBLISH_SENSORS "/QWRTY/SensorData"
#define TOPIC_SUBSCRIBE_STATE "/QWRTY/ControlState"
#define TOPIC_SUBSCRIBE_THRESHOLD "/QWRTY/TempThreshold"

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

// --- Definisi Sensor & Aktor ---
#define ONE_WIRE_BUS 25 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define GAS_SENSOR_PIN 32
const int GAS_THRESHOLD_HIGH = 3627; 
const int GAS_THRESHOLD_MID  = 3497; 

#define LED_PIN_UNUSED 16 
#define LED_FULL_OPEN 21  
#define LED_HALF_OPEN 22  
#define LED_FULL_CLOSE 23 

// --- Variabel Kontrol dari Wokwi 2 ---
bool system_running = true; 
float remote_temp_threshold = 100.0; 

//  VARIABEL BARU: Untuk kontrol reconnect non-blocking
long lastReconnectAttempt = 0; 

// FUNGSI BARU: Mengatur status LED sesuai kondisi
void setLEDState(int openLED, int halfLED, int closeLED, const char* condition) {
    // Matikan semua LED
    digitalWrite(LED_FULL_OPEN, LOW);
    digitalWrite(LED_HALF_OPEN, LOW);
    digitalWrite(LED_FULL_CLOSE, LOW);

    // Nyalakan LED yang sesuai
    if (openLED) digitalWrite(LED_FULL_OPEN, HIGH);
    if (halfLED) digitalWrite(LED_HALF_OPEN, HIGH);
    if (closeLED) digitalWrite(LED_FULL_CLOSE, HIGH);
    
    Serial.println(condition);
}


// Handler ketika pesan MQTT diterima
void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  
  if (String(topic) == TOPIC_SUBSCRIBE_STATE) {
    if (messageTemp == "START") {
      system_running = true;
      Serial.println("Kontrol Diterima: Sistem BERJALAN.");
    } else if (messageTemp == "STOP") {
      system_running = false;
      Serial.println("Kontrol Diterima: Sistem DIHENTIKAN.");
      // Tetapkan LED ke kondisi Tertutup Penuh saat STOP
      setLEDState(LOW, LOW, HIGH, "DIHENTIKAN MANUAL VIA MQTT"); 
    }
  }
  else if (String(topic) == TOPIC_SUBSCRIBE_THRESHOLD) {
    remote_temp_threshold = messageTemp.toFloat();
    Serial.print("Batas Suhu Baru Diterima: ");
    Serial.print(remote_temp_threshold);
    Serial.println(" C");
  }
}

// FUNGSI RECONNECT BARU (Non-Blocking Attempt)
void reconnect() {
  Serial.print("Mencoba koneksi MQTT (Non-blocking)...");
  if (client.connect("ESP32Client1")) {
    Serial.println("terhubung");
    client.subscribe(TOPIC_SUBSCRIBE_STATE);
    client.subscribe(TOPIC_SUBSCRIBE_THRESHOLD);
    
    client.publish(TOPIC_SUBSCRIBE_STATE, system_running ? "START" : "STOP");
  } else {
    Serial.print("gagal, rc=");
    Serial.println(client.state());
  }
}

void setup() {
  Serial.begin(115200);
  
  // Konfigurasi Pin
  pinMode(LED_FULL_OPEN, OUTPUT);
  pinMode(LED_HALF_OPEN, OUTPUT);
  pinMode(LED_FULL_CLOSE, OUTPUT);
  
  // Set status awal LED ke kondisi RUN/NORMAL
  setLEDState(HIGH, LOW, LOW, "INISIALISASI NORMAL"); 

  // Inisialisasi Sensor
  sensors.begin();
  analogReadResolution(12);

  // Inisialisasi Wi-Fi & MQTT (Blokir hanya di setup)
  Serial.print("Menghubungkan ke WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  long now = millis();
  
  // Reconnect Non-blocking di Loop
  if (!client.connected()) {
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect(); // Attempt reconnect
    }
  }

  // MQTT Client loop (HARUS dipanggil untuk memproses pesan masuk jika terhubung)
  client.loop(); 

  // --- Pembacaan Data Sensor ---
  sensors.requestTemperatures(); 
  float temperatureC = sensors.getTempCByIndex(0);
  int gasAnalog = analogRead(GAS_SENSOR_PIN);
  
  // 1. Publikasi Data Sensor hanya jika terhubung
  if (now - lastMsg > 3000) {
    lastMsg = now;
    
    if (client.connected()) { 
        StaticJsonDocument<200> doc;
        doc["temp"] = temperatureC;
        doc["gas"] = gasAnalog;
        doc["running"] = system_running;
        char output[100];
        serializeJson(doc, output);
        client.publish(TOPIC_PUBLISH_SENSORS, output);
    } else {
        Serial.println("Status: OFFLINE. Sensor data tidak dipublikasikan ke remote.");
    }
  }
  
  // 2. Logika Kontrol LED (TETAP BERJALAN TERUS meskipun offline)
  if (system_running) {
    
    // A. KONDISI 1: TUTUP PENUH (Prioritas Tertinggi: Suhu melebihi batas remote ATAU Gas Tinggi)
    if (temperatureC > remote_temp_threshold || gasAnalog > GAS_THRESHOLD_HIGH) {
      setLEDState(LOW, LOW, HIGH, "Safety Override");
    } 
    
    // B. KONDISI 2: Waspada
    else if (gasAnalog > GAS_THRESHOLD_MID) {
      setLEDState(LOW, HIGH, LOW, "Waspada (Gas Menengah)");
    } 
    
    // C. KONDISI 3: AMAN (Normal)
    else {
      setLEDState(HIGH, LOW, LOW, "Kondisi Normal/Aman");
    }
  } else {
    // Jika system_running = false (perintah STOP dari remote)
    setLEDState(LOW, LOW, HIGH, "DIHENTIKAN MANUAL"); 
  }
}