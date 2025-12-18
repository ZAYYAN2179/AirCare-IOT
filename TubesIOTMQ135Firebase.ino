/* 
   ESP32 + MQ135 + DHT11 + PIR + LCD I2C
   Firebase Realtime Database (REST HTTP)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= WIFI =================
const char* WIFI_SSID = "siroyo";
const char* WIFI_PASSWORD = "zzzzzzzz";

// ================= FIREBASE =================
const char* DATABASE_URL =
  "https://aircare-a9f93-default-rtdb.asia-southeast1.firebasedatabase.app";

// ================= PIN =================
const int MQ135_ANALOG_PIN = 36;
const int PIR_PIN = 27;  // <<< PIR

// DHT11
#define DHTPIN 14
#define DHTTYPE DHT11

// LED & Buzzer
const int LED_HIJAU_PIN = 19;
const int LED_KUNING_PIN = 18;
const int LED_MERAH_PIN = 5;
const int BUZZER_PIN = 4;

// ================= LCD I2C =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= THRESHOLD =================
int AMBANG_PERINGATAN = 2400;
int AMBANG_BAHAYA = 3000;

// ================= KALIBRASI =================
const unsigned long BASELINE_CAL_MS = 8000UL;
const int ADC_SAMPLES = 8;

// ================= VARIABEL =================
int sensorValue = 0;
int kondisi_sebelumnya = 0;
int baselineADC = 0;

float temperature = 0.0;
float humidity = 0.0;

bool pirDetected = false;

// ================= OBJECT =================
DHT dht(DHTPIN, DHTTYPE);

// ================= TEST MODE =================
bool testMode = false;
int simulatedMQ = 0;

// =================================================
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(LED_HIJAU_PIN, OUTPUT);
  pinMode(LED_KUNING_PIN, OUTPUT);
  pinMode(LED_MERAH_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);  // PIR

  digitalWrite(BUZZER_PIN, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ135_ANALOG_PIN, ADC_11db);

  dht.begin();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.print("AirCare ESP32");
  delay(2000);
  lcd.clear();

  // WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  lcd.clear();

  // KALIBRASI MQ-135
  lcd.print("Calibrating...");
  unsigned long start = millis();
  long sum = 0;
  int cnt = 0;

  while (millis() - start < BASELINE_CAL_MS) {
    sum += readSmoothedADC();
    cnt++;
    delay(20);
  }

  baselineADC = (cnt > 0) ? sum / cnt : 0;
  AMBANG_PERINGATAN = baselineADC + 600;
  AMBANG_BAHAYA = baselineADC + 1200;

  lcd.clear();
}

// =================================================
int readSmoothedADC() {
  long s = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    s += analogRead(MQ135_ANALOG_PIN);
    delay(5);
  }
  return s / ADC_SAMPLES;
}

// =================================================
void loop() {
  // ================= SERIAL TEST INPUT =================
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "TEST") {
      testMode = true;
      Serial.println(">> TEST MODE AKTIF");
    }

    else if (cmd == "REAL") {
      testMode = false;
      Serial.println(">> REAL SENSOR MODE");
    }

    else if (cmd.startsWith("MQ=")) {
      simulatedMQ = cmd.substring(3).toInt();
      Serial.println(">> SIMULATED MQ: " + String(simulatedMQ));
    }
  }

  // SENSOR
  if (testMode) {
    sensorValue = simulatedMQ;
  } else {
    sensorValue = readSmoothedADC();
  }
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  pirDetected = digitalRead(PIR_PIN);

  bool dhtValid = !isnan(humidity) && !isnan(temperature);

  // KONDISI ASAP
  int kondisi;
  if (sensorValue < AMBANG_PERINGATAN) kondisi = 0;
  else if (sensorValue < AMBANG_BAHAYA) kondisi = 1;
  else kondisi = 2;

  if (kondisi != kondisi_sebelumnya) {
    BUNYIKAN_BUZZER(200);
    kondisi_sebelumnya = kondisi;
  }

  digitalWrite(LED_HIJAU_PIN, kondisi == 0);
  digitalWrite(LED_KUNING_PIN, kondisi == 1);
  digitalWrite(LED_MERAH_PIN, kondisi == 2);

  String statusText =
    (kondisi == 0) ? "AMAN" : (kondisi == 1) ? "WARN"
                                             : "BAHAYA";

  // ================= LCD DISPLAY MODE =================
  char line1[17];
  char line2[17];

  if (kondisi == 0) {
    // ===== NORMAL (LED HIJAU) =====
    snprintf(
      line1,
      sizeof(line1),
      "MQ:%4d  ST:OK",
      sensorValue);

    if (dhtValid) {
      snprintf(
        line2,
        sizeof(line2),
        "T:%2dC H:%2d%% P:%c",
        (int)temperature,
        (int)humidity,
        pirDetected ? '1' : '0');
    } else {
      snprintf(
        line2,
        sizeof(line2),
        "T:--C H:--%% P:%c",
        pirDetected ? '1' : '0');
    }

  } else if (kondisi == 1) {
    // ===== PERINGATAN (LED KUNING) =====
    snprintf(
      line1,
      sizeof(line1),
      "WASPADA ASAP !!");

    snprintf(
      line2,
      sizeof(line2),
      "MQ:%4d P:%c",
      sensorValue,
      pirDetected ? '1' : '0');

  } else {
    // ===== BAHAYA (LED MERAH) =====
    snprintf(
      line1,
      sizeof(line1),
      "BAHAYA ASAP !! ");

    snprintf(
      line2,
      sizeof(line2),
      "MQ:%4d P:%c",
      sensorValue,
      pirDetected ? '1' : '0');
  }

  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);

  // SERIAL
  Serial.print("MQ:");
  Serial.print(sensorValue);
  Serial.print(" ");
  Serial.print(statusText);
  Serial.print(" | PIR:");
  Serial.println(pirDetected ? "DETECTED" : "NO");

  // FIREBASE
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(DATABASE_URL) + "/aircare.json");
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"sensorValue\":" + String(sensorValue) + ",";
    payload += "\"baseline\":" + String(baselineADC) + ",";
    payload += "\"condition\":" + String(kondisi) + ",";
    payload += "\"status\":\"" + statusText + "\",";
    payload += "\"pir\":" + String(pirDetected ? "true" : "false") + ",";
    if (dhtValid) {
      payload += "\"temperature\":" + String(temperature, 1) + ",";
      payload += "\"humidity\":" + String(humidity, 1) + ",";
    }
    payload += "\"timestamp\":" + String(millis() / 1000);
    payload += "}";

    http.sendRequest("PATCH", payload);
    http.end();
  }

  delay(2000);
}

// =================================================
void BUNYIKAN_BUZZER(int durasi_ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durasi_ms);
  digitalWrite(BUZZER_PIN, LOW);
}