#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_VEML7700.h>
TwoWire I2C_VEML = TwoWire(0);
TwoWire I2C_AHT = TwoWire(1);

// Wi-Fi and Firebase Configuration
#define WIFI_SSID ""
#define WIFI_PASSWORD "" 
#define API_KEY ""
#define DATABASE_URL ""
#define USER_EMAIL ""
#define USER_PASSWORD ""

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Pin Definitions
const int ledPin = 2;
const int soilPin = 34;
#define PIR_PIN 32

// AHT10 Sensor
Adafruit_AHTX0 aht;

// Light sensor
Adafruit_VEML7700 veml;
#define SAMPLE_INTERVAL_SEC 30

// Timing Control
unsigned long lastSampleMillis = 0;
unsigned long sendDataPrevMillis = 0;

// Soil calibration
int airValue = 3120;
int waterValue = 1265;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  I2C_VEML.begin(22, 21); // SDA, SCL for VEML7700
  I2C_AHT.begin(18, 19);  // SDA, SCL for AHT10

  // WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  // Initialize AHT10
  if (!aht.begin(&I2C_AHT)) {
    Serial.println("Could not find AHT10 sensor!");
    while (1);
  }
  Serial.println("AHT10 initialized");

  // PIR
  pinMode(PIR_PIN, INPUT);

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  fbdo.setBSSLBufferSize(4096, 4096);
  fbdo.setResponseSize(4096);
  Firebase.reconnectNetwork(true);
  Firebase.begin(&config, &auth);
  Firebase.setDoubleDigits(5);

  // Light sensor
  if (!veml.begin(&I2C_VEML)) {
    Serial.println("VEML7700 not found");
    while (1);
  }

  veml.setGain(VEML7700_GAIN_1);
  veml.setIntegrationTime(VEML7700_IT_100MS);
}

void loop() {

  if (Firebase.ready() && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
  }

  // LED
  int ledState;
  if (Firebase.RTDB.getInt(&fbdo, "/led/state", &ledState)) {
    digitalWrite(ledPin, ledState);
  }

  // AHT10 Reading
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  Serial.printf("Temperature: %.2f °C, Humidity: %.2f %%\n",
                temp.temperature, humidity.relative_humidity);

  Firebase.RTDB.setFloat(&fbdo, "/sensor/temperature", temp.temperature);
  Firebase.RTDB.setFloat(&fbdo, "/sensor/humidity", humidity.relative_humidity);

  // Soil moisture
  int raw = analogRead(soilPin);
  int moisture = map(raw, airValue, waterValue, 0, 100);
  moisture = constrain(moisture, 0, 100);

  Serial.print("Moisture: ");
  Serial.println(moisture);

  // Send soil data to Firebase
  Firebase.RTDB.setInt(&fbdo, "/sensor/capacitiveSoilMoisture", moisture);

  // Light sensor
  unsigned long now = millis();
  if (now - lastSampleMillis >= SAMPLE_INTERVAL_SEC * 1000UL) {
    lastSampleMillis = now;

    delay(120);

    uint16_t als_raw = veml.readALS();
    float lux = veml.readLux();

    Firebase.RTDB.setInt(&fbdo, "/sensor/light/lux", round(lux));
    Firebase.RTDB.setInt(&fbdo, "/sensor/light/raw", als_raw);
  }

  // PIR
  int pirState = digitalRead(PIR_PIN);
  Firebase.RTDB.setBool(&fbdo, "/sensor/motion", pirState);

  delay(5000);
}