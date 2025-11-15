# 🌡️ Multi-Sensor IoT Dashboard

Dashboard monitoring dan kontrol untuk 2 Wokwi dengan sensor berbeda menggunakan Streamlit dan MQTT.

## 📋 Fitur

### Wokwi 1 - Monitoring Suhu
- 🌤️ **Sensor Suhu Udara** - Monitoring suhu lingkungan
- 🌱 **Sensor Suhu Tanah** - Monitoring suhu tanah

### Wokwi 2 - Monitoring & Kontrol Air
- 💦 **Sensor Level Air** - Monitoring persediaan air
- 🎛️ **Kontrol Servo** - Mengontrol aliran air (ON/OFF)

### Dashboard Features
- 📊 Real-time monitoring dengan metrics cards
- 📈 Grafik real-time untuk semua sensor
- 🔄 Auto-refresh dengan interval yang dapat disesuaikan
- 💡 Status koneksi MQTT
- 🎨 UI modern dengan Streamlit

## 🛠️ Instalasi

### 1. Install Dependencies

```bash
pip install -r requirements.txt
```

### 2. Jalankan Dashboard

```bash
streamlit run dashboard.py
```

Dashboard akan terbuka di browser pada `http://localhost:8501`

## 📡 Konfigurasi MQTT

Dashboard menggunakan MQTT broker public. Anda dapat menggunakan salah satu dari:
- `broker.hivemq.com`
- `broker.emqx.io`
- `test.mosquitto.org`

**Port:** 1883

### MQTT Topics

#### Wokwi 1 (Subscribe):
- `wokwi/sensor/temp_air` - Data suhu udara
- `wokwi/sensor/temp_soil` - Data suhu tanah

#### Wokwi 2 (Subscribe):
- `wokwi/sensor/water_level` - Data level air
- `wokwi/status/servo` - Status servo

#### Wokwi 2 (Publish):
- `wokwi/control/servo` - Kontrol servo (ON/OFF)

## 🔧 Format Pesan JSON

### Data Sensor

**Suhu (Udara/Tanah):**
```json
{
  "temperature": 25.5
}
```

**Level Air:**
```json
{
  "level": 75.0
}
```

**Status Servo:**
```json
{
  "status": "ON"
}
```

### Kontrol Servo

**Mengirim Perintah:**
```json
{
  "action": "ON"
}
```
atau
```json
{
  "action": "OFF"
}
```

## 📝 Konfigurasi Wokwi

### Wokwi 1 - Sensor Suhu

**diagram.json contoh:**
```json
{
  "version": 1,
  "author": "Your Name",
  "editor": "wokwi",
  "parts": [
    { "type": "wokwi-esp32-devkit-v1", "id": "esp" },
    { "type": "wokwi-dht22", "id": "dht1" },
    { "type": "wokwi-ntc-temperature-sensor", "id": "ntc1" }
  ],
  "connections": [
    [ "dht1:VCC", "esp:3V3", "red", [ "h0" ] ],
    [ "dht1:SDA", "esp:D15", "green", [ "h0" ] ],
    [ "dht1:GND", "esp:GND", "black", [ "h0" ] ],
    [ "ntc1:VCC", "esp:3V3", "red", [ "h0" ] ],
    [ "ntc1:OUT", "esp:D34", "yellow", [ "h0" ] ],
    [ "ntc1:GND", "esp:GND", "black", [ "h0" ] ]
  ]
}
```

**sketch.ino contoh:**
```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT22
#define SOIL_TEMP_PIN 34

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Baca suhu udara
  float temp_air = dht.readTemperature();
  if (!isnan(temp_air)) {
    String payload = "{\"temperature\":" + String(temp_air) + "}";
    client.publish("wokwi/sensor/temp_air", payload.c_str());
  }
  
  // Baca suhu tanah (simulasi dari NTC)
  int sensorValue = analogRead(SOIL_TEMP_PIN);
  float temp_soil = map(sensorValue, 0, 4095, 15, 35);
  String payload2 = "{\"temperature\":" + String(temp_soil) + "}";
  client.publish("wokwi/sensor/temp_soil", payload2.c_str());
  
  delay(2000);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("WokwiClient1")) {
      Serial.println("Connected to MQTT");
    } else {
      delay(5000);
    }
  }
}
```

### Wokwi 2 - Sensor Air & Servo

**diagram.json contoh:**
```json
{
  "version": 1,
  "author": "Your Name",
  "editor": "wokwi",
  "parts": [
    { "type": "wokwi-esp32-devkit-v1", "id": "esp" },
    { "type": "wokwi-slide-potentiometer", "id": "pot1" },
    { "type": "wokwi-servo", "id": "servo1" }
  ],
  "connections": [
    [ "pot1:VCC", "esp:3V3", "red", [ "h0" ] ],
    [ "pot1:SIG", "esp:D32", "green", [ "h0" ] ],
    [ "pot1:GND", "esp:GND", "black", [ "h0" ] ],
    [ "servo1:V+", "esp:VIN", "red", [ "h0" ] ],
    [ "servo1:SIG", "esp:D13", "orange", [ "h0" ] ],
    [ "servo1:GND", "esp:GND", "brown", [ "h0" ] ]
  ]
}
```

**sketch.ino contoh:**
```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

#define WATER_SENSOR_PIN 32
#define SERVO_PIN 13

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);
Servo myServo;

void setup() {
  Serial.begin(115200);
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (String(topic) == "wokwi/control/servo") {
    if (message.indexOf("ON") >= 0) {
      myServo.write(90);
      client.publish("wokwi/status/servo", "{\"status\":\"ON\"}");
    } else if (message.indexOf("OFF") >= 0) {
      myServo.write(0);
      client.publish("wokwi/status/servo", "{\"status\":\"OFF\"}");
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Baca level air (dari potentiometer, simulasi)
  int sensorValue = analogRead(WATER_SENSOR_PIN);
  float waterLevel = map(sensorValue, 0, 4095, 0, 100);
  String payload = "{\"level\":" + String(waterLevel) + "}";
  client.publish("wokwi/sensor/water_level", payload.c_str());
  
  delay(2000);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("WokwiClient2")) {
      client.subscribe("wokwi/control/servo");
      Serial.println("Connected to MQTT");
    } else {
      delay(5000);
    }
  }
}
```

## 🎮 Cara Menggunakan Dashboard

1. **Jalankan Wokwi Simulator:**
   - Buka kedua project Wokwi
   - Pastikan code sudah ter-upload
   - Start simulation

2. **Jalankan Dashboard:**
   ```bash
   streamlit run dashboard.py
   ```

3. **Hubungkan ke MQTT:**
   - Klik tombol "Hubungkan ke MQTT" di sidebar
   - Tunggu hingga status berubah menjadi "Terhubung"

4. **Monitor Data:**
   - Lihat tab "Dashboard" untuk metrics real-time
   - Lihat tab "Grafik Real-time" untuk visualisasi data

5. **Kontrol Servo:**
   - Gunakan tombol ON/OFF di sidebar
   - Status servo akan ter-update otomatis

## 📊 Screenshot Fitur

### Tab Dashboard
- Real-time metrics untuk semua sensor
- Delta value untuk melihat perubahan
- Progress bar untuk water level
- Status servo

### Tab Grafik Real-time
- Grafik untuk suhu udara
- Grafik untuk suhu tanah
- Grafik untuk level air
- Semua dengan timestamp

### Tab Info
- Dokumentasi lengkap
- Daftar MQTT topics
- Format JSON messages
- Tips penggunaan

## 🔍 Troubleshooting

### Dashboard tidak dapat terhubung ke MQTT
- Pastikan koneksi internet aktif
- Coba ganti MQTT broker di kode (line 12-13)
- Restart dashboard

### Data sensor tidak muncul
- Pastikan Wokwi simulation sedang running
- Periksa MQTT topics sudah sesuai
- Lihat console Wokwi untuk error messages

### Servo tidak merespon
- Pastikan sudah subscribe ke topic yang benar
- Periksa callback function di Wokwi code
- Pastikan dashboard terhubung ke MQTT

## 📦 Dependencies

- `streamlit>=1.28.0` - Framework web untuk dashboard
- `paho-mqtt>=1.6.1` - MQTT client library
- `pandas>=2.0.0` - Data manipulation
- `plotly>=5.17.0` - Interactive charts

## 👨‍💻 Development

### Struktur File
```
multi-sensor-dashboard/
├── dashboard.py          # Main dashboard application
├── requirements.txt      # Python dependencies
└── README.md            # Documentation
```

### Modifikasi MQTT Topics
Edit variabel di bagian atas `dashboard.py`:
```python
TOPIC_TEMP_AIR = "wokwi/sensor/temp_air"
TOPIC_TEMP_SOIL = "wokwi/sensor/temp_soil"
TOPIC_WATER_LEVEL = "wokwi/sensor/water_level"
TOPIC_SERVO_CONTROL = "wokwi/control/servo"
TOPIC_SERVO_STATUS = "wokwi/status/servo"
```

### Mengganti MQTT Broker
Edit variabel:
```python
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
```

## 📄 License

Free to use for educational purposes.

## 🙏 Credits

Created for IoT multi-sensor monitoring project using Wokwi simulator and Streamlit dashboard.
