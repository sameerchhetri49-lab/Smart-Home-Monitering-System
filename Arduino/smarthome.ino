#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Servo.h>

// ===================== SENSORS =====================
#define MQ2_GAS A0
#define FLAME_SENSOR 9

// --- RAIN SENSOR ---
#define RAIN_ANALOG A2
#define RAIN_DIGITAL 3
int rainThreshold = 500;

// --- SERVOS ---
Servo clothesServo;
Servo doorServo;
#define CLOTHES_SERVO_PIN 10
#define DOOR_SERVO_PIN 12

// --- HC-SR04 ---
#define TRIG_PIN 5
#define ECHO_PIN 11

// --- DHT11 SENSOR ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- OUTPUTS ---
#define GREEN_LED 6
#define RED_LED 7
#define BUZZER 8

// ===================== BLE UUIDs =====================
const char* SERVICE_UUID     = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* SENSOR_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // Notify Arduino -> Pi
const char* ACTION_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // Write  Pi -> Arduino

BLEService shmService(SERVICE_UUID);
BLECharacteristic sensorChar(SENSOR_CHAR_UUID, BLERead | BLENotify, 220);
BLECharacteristic actionChar(ACTION_CHAR_UUID, BLEWrite | BLEWriteWithoutResponse, 220);

// ===================== TIMERS =====================
unsigned long lastSerial = 0;
unsigned long lastBleSend = 0;
unsigned long lastBuzzerToggle = 0;

const unsigned long BLE_SEND_INTERVAL_MS = 1000;
const unsigned long ACTION_TIMEOUT_MS = 3000;
unsigned long lastActionMs = 0;

// ===================== State =====================
bool motionDetected = false;
bool doorOpen = false;
bool buzzerState = false;
bool rainStatus = false;

// ===================== Remote override =====================
bool remoteOverrideActive = false;

int  remoteBuzzer = -1;       // -1=no override, 0/1=force
char remoteLed = 0;           // 0=no override, 'R','G','Y','O'
int  remoteClothesServo = -1; // -1=no override, else angle
int  remoteDoorServo = -1;    // -1=no override, else angle

// ===================== Safe DHT Read =====================
float readTemp() {
  float t = dht.readTemperature();
  int tries = 0;
  while (isnan(t) && tries < 5) {
    delay(200);
    t = dht.readTemperature();
    tries++;
  }
  return isnan(t) ? -1 : t;
}

float readHum() {
  float h = dht.readHumidity();
  int tries = 0;
  while (isnan(h) && tries < 5) {
    delay(200);
    h = dht.readHumidity();
    tries++;
  }
  return isnan(h) ? -1 : h;
}

// ===================== Helpers =====================
void setLED(char code) {
  if (code == 'R') {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  } else if (code == 'G') {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  } else if (code == 'Y') {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
  } else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
  }
}

long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 999;
  return (long)(duration * 0.034 / 2);
}

String buildSensorJSON(int gasValue, int flameRaw, int rainAnalog, float temp, float hum, int distance) {
  int flameDetected = (flameRaw == LOW) ? 1 : 0;

  StaticJsonDocument<256> doc;
  doc["gas"] = gasValue;
  doc["flame"] = flameDetected;
  doc["temp"] = temp;
  doc["hum"] = hum;
  doc["rainAnalog"] = rainAnalog;
  doc["raining"] = rainStatus ? 1 : 0;
  doc["distance"] = distance;
  doc["motion"] = motionDetected ? 1 : 0;
  doc["doorOpen"] = doorOpen ? 1 : 0;

  String out;
  serializeJson(doc, out);
  return out;
}

void handleActionJSON(const String& jsonStr) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) return;

  lastActionMs = millis();
  remoteOverrideActive = true;

  if (doc.containsKey("buzzer")) remoteBuzzer = (int)doc["buzzer"];

  if (doc.containsKey("led")) {
    const char* led = doc["led"];
    if (led && led[0]) remoteLed = led[0];
  }

  if (doc.containsKey("clothesServo")) remoteClothesServo = (int)doc["clothesServo"];
  if (doc.containsKey("doorServo")) remoteDoorServo = (int)doc["doorServo"];
}

void checkForPiActions() {
  if (!actionChar.written()) return;

  int len = actionChar.valueLength();
  if (len <= 0 || len >= 220) return;

  char buf[221];
  actionChar.readValue((uint8_t*)buf, len);
  buf[len] = '\0';

  handleActionJSON(String(buf));
}

void applyRemoteOverrides() {
  if (remoteOverrideActive && (millis() - lastActionMs > ACTION_TIMEOUT_MS)) {
    remoteOverrideActive = false;
    remoteBuzzer = -1;
    remoteLed = 0;
    remoteClothesServo = -1;
    remoteDoorServo = -1;
  }

  if (!remoteOverrideActive) return;

  if (remoteLed != 0) setLED(remoteLed);

  if (remoteBuzzer == 0) {
    noTone(BUZZER);
  } else if (remoteBuzzer == 1) {
    tone(BUZZER, 1000);
  }

  if (remoteClothesServo >= 0 && remoteClothesServo <= 180) {
    clothesServo.write(remoteClothesServo);
  }
  if (remoteDoorServo >= 0 && remoteDoorServo <= 180) {
    doorServo.write(remoteDoorServo);
  }
}

// ===================== Setup =====================
void setup() {
  Serial.begin(9600);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(FLAME_SENSOR, INPUT);
  pinMode(RAIN_DIGITAL, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  clothesServo.attach(CLOTHES_SERVO_PIN);
  clothesServo.write(0);

  doorServo.attach(DOOR_SERVO_PIN);
  doorServo.write(0);

  dht.begin();
  delay(2000);

  // ===== BLE start =====
  if (!BLE.begin()) {
    while (1) { delay(500); }
  }

  BLE.setLocalName("SHM-UnoR4");
  BLE.setDeviceName("SHM-UnoR4");
  BLE.setAdvertisedService(shmService);

  shmService.addCharacteristic(sensorChar);
  shmService.addCharacteristic(actionChar);
  BLE.addService(shmService);

  sensorChar.writeValue("{}");   // ✅ FIXED (no ambiguity)
  BLE.advertise();

  Serial.println("✅ BLE Advertising as SHM-UnoR4");
}

// ===================== Loop =====================
void loop() {
  BLEDevice central = BLE.central();

  // --- Sensor Reads ---
  int gasValue = analogRead(MQ2_GAS);
  int flameValue = digitalRead(FLAME_SENSOR);
  int rainAnalog = analogRead(RAIN_ANALOG);
  float temp = readTemp();
  float hum = readHum();

  // --- Rain + Clothes Servo ---
  bool currentRain = (rainAnalog < rainThreshold);
  if (currentRain != rainStatus) {
    rainStatus = currentRain;
    if (rainStatus) {
      clothesServo.write(90);
      Serial.println("🌧 Rain Detected – Clothes Pulled Inside");
    } else {
      clothesServo.write(0);
      Serial.println("☀️ No Rain – Clothes Outside");
    }
  }

  // --- Motion ---
  int distance = (int)readDistanceCm();
  motionDetected = (distance <= 30);

  // --- Door Control ---
  if (motionDetected) {
    if (!doorOpen) {
      doorServo.write(90);
      doorOpen = true;
      Serial.println("🚶 Motion Detected → Door Open");
    }
  } else {
    if (doorOpen) {
      doorServo.write(0);
      doorOpen = false;
      Serial.println("No Motion → Door Closed");
      noTone(BUZZER);
      buzzerState = false;
    }
  }

  // --- Buzzer Beep Pattern ---
  if (motionDetected && doorOpen) {
    if (millis() - lastBuzzerToggle >= 300) {
      lastBuzzerToggle = millis();
      buzzerState = !buzzerState;
      if (buzzerState) tone(BUZZER, 1000);
      else noTone(BUZZER);
    }
  }

  // --- Flame & Gas ---
  if (flameValue == LOW) {
    setLED('R');
    tone(BUZZER, 1200);
  } else {
    if (gasValue < 170) {
      setLED('G');
      if (!motionDetected) noTone(BUZZER);
    } else if (gasValue < 400) {
      setLED('R');
      tone(BUZZER, 900);
    } else {
      setLED('R');
      tone(BUZZER, 1000);
    }
  }

  // --- BLE section ---
  if (central && central.connected()) {
    BLE.poll();           // ✅ helps BLE reliability
    checkForPiActions();

    if (millis() - lastBleSend >= BLE_SEND_INTERVAL_MS) {
      lastBleSend = millis();
      String payload = buildSensorJSON(gasValue, flameValue, rainAnalog, temp, hum, distance);
      sensorChar.writeValue(payload.c_str());
    }

    applyRemoteOverrides();
  } else {
    remoteOverrideActive = false;
    remoteBuzzer = -1;
    remoteLed = 0;
    remoteClothesServo = -1;
    remoteDoorServo = -1;
  }

  // --- Serial Output Every 3 Seconds ---
  if (millis() - lastSerial >= 3000) {
    lastSerial = millis();

    Serial.print("Gas Level: ");
    Serial.println(gasValue);
    if (gasValue < 250) Serial.println("Air Quality: Good 😊😊");
    else if (gasValue < 400) Serial.println("Air Quality: WARNING ⚠️");
    else Serial.println("Air Quality: DANGEROUS 🚨");
    Serial.println();

    if (flameValue == LOW) Serial.println("Flame Sensor: FLAME DETECTED 🔥");
    else Serial.println("Flame Sensor: No flame");

    if (temp != -1 && hum != -1) {
      Serial.print("| Temp=");
      Serial.print(temp, 2);
      Serial.print(" | Hum=");
      Serial.println(hum, 2);
    }

    Serial.println();
  }

  delay(50);
}