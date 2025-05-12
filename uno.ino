#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <SoftwareSerial.h>

#define DHTPIN 2           // DHT sensor connected to pin 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Initialize I2C LCD at address 0x27, 16 columns, 2 rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Motor driver pins
#define PUMP1_IN1 7
#define PUMP1_IN2 8
#define PUMP2_IN1 10
#define PUMP2_IN2 9

// Software Serial for ESP32 communication
#define RX_PIN 5  // Connect to ESP32 TX
#define TX_PIN 6  // Connect to ESP32 RX
SoftwareSerial espSerial(RX_PIN, TX_PIN);

bool usePump1 = true;
unsigned long lastDataSendTime = 0;
const unsigned long DATA_SEND_INTERVAL = 2000; // Send data every 2 seconds

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);  // Initialize software serial for ESP32 communication
  
  dht.begin();
  lcd.begin();
  lcd.backlight();
  
  // Pump pins as outputs
  pinMode(PUMP1_IN1, OUTPUT);
  pinMode(PUMP1_IN2, OUTPUT);
  pinMode(PUMP2_IN1, OUTPUT);
  pinMode(PUMP2_IN2, OUTPUT);
  
  // Stop both pumps initially
  digitalWrite(PUMP1_IN1, LOW);
  digitalWrite(PUMP1_IN2, LOW);
  digitalWrite(PUMP2_IN1, LOW);
  digitalWrite(PUMP2_IN2, LOW);
  
  Serial.println("Arduino system initialized");
  espSerial.println("AT+BEGIN"); // Send initialization message to ESP32
}

void loop() {
  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor");
    return;
  }

  // Display on Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(humidity);
  lcd.print("%");
  
  // Send data to ESP32 every DATA_SEND_INTERVAL milliseconds
  if (millis() - lastDataSendTime >= DATA_SEND_INTERVAL) {
    // Format: "DATA,temperature,humidity,pumpStatus"
    espSerial.print("DATA,");
    espSerial.print(temperature);
    espSerial.print(",");
    espSerial.print(humidity);
    espSerial.print(",");
    espSerial.println(usePump1 ? "Pump1" : "Pump2");
    
    lastDataSendTime = millis();
  }
  
  // Pump control based on temperature
  if (temperature >= 32.0) {
    Serial.println("Temperature is high, activating pump...");
    
    // Send pump activation message to ESP32
    if (usePump1) {
      Serial.println("Activating Pump 1...");
      espSerial.println("PUMP,1,ON");
      digitalWrite(PUMP1_IN1, HIGH);
      digitalWrite(PUMP1_IN2, LOW);
      delay(10000);
      digitalWrite(PUMP1_IN1, LOW);
      espSerial.println("PUMP,1,OFF");
    } else {
      Serial.println("Activating Pump 2...");
      espSerial.println("PUMP,2,ON");
      digitalWrite(PUMP2_IN1, HIGH);
      digitalWrite(PUMP2_IN2, LOW);
      delay(10000);
      digitalWrite(PUMP2_IN1, LOW);
      espSerial.println("PUMP,2,OFF");
    }
    
    usePump1 = !usePump1;
    delay(5000);  // Pause before checking again
  }
  
  delay(2000);  // Delay between loops
}
