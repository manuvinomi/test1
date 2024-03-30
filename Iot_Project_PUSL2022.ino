#include <DHT.h>
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "Dialog 4G 039"
#define WIFI_PASSWORD "C42d52c3"
#define API_KEY "AIzaSyBCCdgnu0hmUQITgJ6QYtcrOnJFWNp1rls"
#define DATABASE_URL "https://iot-module-887e1-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define DHTPIN 4      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11

const int waterlvlsensorPin = 32;  //Water Level Sensor
const int soilPin = 2;  // Analog pin connected to the soil moisture sensor
const int pumpPin = 14;  // Digital pin connected to the transistor/relay controlling the pump
const int threshold = 1700;  // Set your desired moisture threshold

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

int counter = 0;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print("."); delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if(Firebase.signUp(&config, &auth, "", "")){
    Serial.println("signUp OK");
    signupOK = true;
  }else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("DHTxx test!");
  dht.begin();
  pinMode(soilPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(waterlvlsensorPin, INPUT);

  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;
  // 0x27 0x3F
  Wire.begin();
  for (byte i = 8; i < 120; i++)
  {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0)
      {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1);
      }
  }
  Serial.println ("Done.");
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");

  lcd.init();                       // Initialize the LCD
  lcd.backlight();                  // Turn on the backlight
  lcd.clear();                      // Clear the LCD screen

  lcd.setCursor(0, 0);
  lcd.print("H-");
  lcd.setCursor(4, 0);
  lcd.print("%");
  lcd.setCursor(9, 0);
  lcd.print("T-");
  lcd.setCursor(0, 1);
  lcd.print("M-");
  lcd.setCursor(7, 1);
  lcd.print("L-");


}

void loop() {

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  int humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  // Wait a few seconds between measurements.
  delay(1000);

  // Display the temperature and humidity values
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print("%\t");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println("°C");

  //Display the soil moisture level
  int moisture = analogRead(soilPin);  // Read soil moisture value
  if (moisture > threshold) {
    // If soil is dry, turn on the pump
    digitalWrite(pumpPin, LOW);
    delay(3000);  // Run the pump for 5 seconds (adjust as needed)
    digitalWrite(pumpPin, HIGH);
  }
  Serial.print("Soil Moisture: ");
  Serial.println(moisture);
  delay(1000);

  float volts = analogRead(34) * 5.0 / 1024.0;
  float amps = volts / 10000.0; // across 10,000 Ohms
  float microamps = amps * 1000000;
  float lux = microamps * 2.0;
  Serial.print("Illuminence: ");
  Serial.print(lux);
  Serial.print(" lux");
  delay(1000);

  int watersensorValue = digitalRead(waterlvlsensorPin);
  if (watersensorValue == HIGH) {
    Serial.println("Water level is low.");
  } else {
    Serial.println("Water level is high.");
  }

  lcd.setCursor(2, 0);        // Set the cursor to the first column and first row
  lcd.print(humidity);       // Print humidity on LCD
  lcd.setCursor(11, 0);      // Set the cursor to the current position
  lcd.print(temperature);    // Print temperature on LCD
  lcd.setCursor(2, 1);      // Set the cursor to the current position
  lcd.print(moisture);    // Print Soil moisture on LCD
  lcd.setCursor(9, 1);      // Set the cursor to the current position
  lcd.print(lux);          // Print illuminence on LCD

  if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    //Store Sensors data to a RTDB

    if(Firebase.RTDB.setInt(&fbdo, "DHT11_Hu_&_Temp/humidity", humidity)){
      Serial.println("--Humidity has successfully saved to: "+ fbdo.dataPath());
    }else{
      Serial.println("--Humidity has failed: " + fbdo.errorReason());
    }

    if(Firebase.RTDB.setFloat(&fbdo, "DHT11_Hu_&_Temp/temperature", temperature)){
      Serial.println("--Temperature has successfully saved to: "+ fbdo.dataPath());
    }else{
      Serial.println("--Temperature has failed: " + fbdo.errorReason());
    }

    lux = analogRead(34);
    if(Firebase.RTDB.setFloat(&fbdo, "Light_Intensity", lux)){
      Serial.println("--Illuminence has successfully saved to: "+ fbdo.dataPath());
    }else{
      Serial.println("--Illuminence has failed: " + fbdo.errorReason());
    }

    moisture = analogRead(soilPin);
    Firebase.RTDB.setInt(&fbdo, "Soil Moisture", moisture);
  }

}
