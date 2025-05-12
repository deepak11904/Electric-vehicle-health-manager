#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include "ACS712.h"

// WiFi credentials - replace with your network details
const char* ssid = "vivo Y200 5G";
const char* password = "123456789";

// Web server on port 80
WebServer server(80);

// Define pins for serial communication with Arduino
#define RX_PIN 16  // Connect to Arduino TX
#define TX_PIN 17  // Connect to Arduino RX

// Create a hardware serial instance
HardwareSerial ArduinoSerial(1); // Using UART1

// ACS712 sensor on A0 (ADC1 channel 0) with 5V reference, 10-bit ADC, 66 mV/A sensitivity
ACS712 ACS(34, 3.3, 4095, 66);

// Variables to store sensor data
float temperature = 0.0;
float humidity = 0.0;
float current = 0.0;
String pumpStatus = "None";
String lastPumpAction = "None";

// Data history for graphs
#define HISTORY_SIZE 60
float temperatureHistory[HISTORY_SIZE] = {0};
float humidityHistory[HISTORY_SIZE] = {0};
float currentHistory[HISTORY_SIZE] = {0};
int historyIndex = 0;

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 5000; // Update interval in milliseconds

void setup() {
  // Initialize Serial communication for debugging

  // Connect to WiFi

  Serial.begin(115200);
  Serial.println("ESP32 Sensor Monitoring Web Server");
  Serial.println();
  Serial.println("ESP32 Wi-Fi Connection Test");

  // Set Wi-Fi mode to station (client)
  WiFi.mode(WIFI_STA);
  Serial.println("Wi-Fi mode set to Station.");

  // Attempt to connect to the Wi-Fi network
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // Wait for the connection to establish or timeout
  int connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 30) { // Check for 30 seconds
   delay(1000);
   Serial.print(".");
   connectionAttempts++;
  }

  // Check the connection status
  if (WiFi.status() == WL_CONNECTED) {
   Serial.println();
   Serial.println("Wi-Fi connected successfully!");
   Serial.print("IP address: ");
   Serial.println(WiFi.localIP());
   Serial.print("MAC address: ");
   Serial.println(WiFi.macAddress());
   Serial.print("RSSI (Signal Strength): ");
   Serial.print(WiFi.RSSI());
   Serial.println(" dBm");
  } else {
   Serial.println();
   Serial.println("Failed to connect to Wi-Fi!");
   Serial.print("Wi-Fi status code: ");
   Serial.println(WiFi.status());
   /*
    Wi-Fi Status Codes:
    0: WL_IDLE_STATUS
    1: WL_SCAN_COMPLETED
    2: WL_CONNECT_FAILED
    3: WL_CONNECTION_LOST
    4: WL_CONNECTED
    5: WL_DISCONNECTED
   */
   if (WiFi.status() == WL_CONNECT_FAILED) {
    Serial.println("Possible reasons for connection failure:");
    Serial.println("- Incorrect Wi-Fi SSID or password.");
    Serial.println("- Wi-Fi network is out of range or not available.");
    Serial.println("- Router is not allowing new connections.");
    Serial.println("- Firewall or other security settings on the router.");
   } else if (WiFi.status() == WL_DISCONNECTED) {
    Serial.println("ESP32 disconnected from Wi-Fi.");
   } else if (WiFi.status() == WL_CONNECTION_LOST) {
    Serial.println("Wi-Fi connection was lost.");
   }
  }

  // Initialize ACS712 sensor
  Serial.print("ACS712_LIB_VERSION: ");
  Serial.println(ACS712_LIB_VERSION);
  ACS.autoMidPoint();

  // Initialize serial communication with Arduino
  ArduinoSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/styles.css", handleCSS);
  server.on("/scripts.js", handleJS);

  // Start server only if Wi-Fi is connected
  if (WiFi.status() == WL_CONNECTED) {
   server.begin();
   Serial.println("HTTP server started");
  } else {
   Serial.println("HTTP server not started due to Wi-Fi connection failure.");
  }
 }

void loop() {
  // Handle client requests
  server.handleClient();
  
  // Read data from Arduino if available
  if (ArduinoSerial.available() > 0) {
    String message = ArduinoSerial.readStringUntil('\n');
    message.trim();
    
    // Process the message
    if (message.startsWith("DATA,")) {
      // Format: "DATA,temperature,humidity,pumpStatus"
      int firstComma = message.indexOf(',');
      int secondComma = message.indexOf(',', firstComma + 1);
      int thirdComma = message.indexOf(',', secondComma + 1);
      
      if (firstComma > 0 && secondComma > 0 && thirdComma > 0) {
        temperature = message.substring(firstComma + 1, secondComma).toFloat();
        humidity = message.substring(secondComma + 1, thirdComma).toFloat();
        pumpStatus = message.substring(thirdComma + 1);
        
        // Print parsed data for debugging
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
        
        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
        
        Serial.print("Pump Status: ");
        Serial.println(pumpStatus);
      }
    } else if (message.startsWith("PUMP,")) {
      // Format: "PUMP,pumpNumber,action"
      int firstComma = message.indexOf(',');
      int secondComma = message.indexOf(',', firstComma + 1);
      
      if (firstComma > 0 && secondComma > 0) {
        String pumpNumber = message.substring(firstComma + 1, secondComma);
        String action = message.substring(secondComma + 1);
        
        lastPumpAction = "Pump " + pumpNumber + " " + action;
        Serial.print("Pump Action: ");
        Serial.println(lastPumpAction);
      }
    }
  }

  // Read current from ACS712
  current = ACS.mA_DC();

  Serial.print("Current: ");
  Serial.print(current / 1000.0); // Convert mA to A for display
  Serial.println(" A");
  // Update history arrays at regular intervals
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentTime;
    
    // Update history arrays
    temperatureHistory[historyIndex] = temperature;
    humidityHistory[historyIndex] = humidity;
    currentHistory[historyIndex] = current / 1000.0; // Convert to Amps
    
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  }
  delay(1000);
}

void handleRoot() {
  String html = "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>EV_GUARDIAN - Monitoring System</title>"
                "<link rel='stylesheet' type='text/css' href='styles.css'>"
                "<script src='https://cdnjs.cloudflare.com/ajax/libs/gauge.js/1.3.7/gauge.min.js'></script>"
                "<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js'></script>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>EV_GUARDIAN</h1>"
                "<h3>Smart EV Monitoring System</h3>"
                
                "<div class='card'>"
                "<h2>Current Status</h2>"
                "<div class='status-panel'>"
                "<div class='status-item'>"
                "<div class='status-label'>WiFi:</div>"
                "<div class='status-value' id='wifi-status'>Connected</div>"
                "</div>"
                "<div class='status-item'>"
                "<div class='status-label'>Last Pump Action:</div>"
                "<div class='status-value' id='pump-action'>" + lastPumpAction + "</div>"
                "</div>"
                "<div class='status-item'>"
                "<div class='status-label'>Pump Status:</div>"
                "<div class='status-value' id='pump-status'>" + pumpStatus + "</div>"
                "</div>"
                "</div>"
                "</div>"
                
                "<div class='gauges-container'>"
                "<div class='card'>"
                "<h2>Temperature</h2>"
                "<div class='gauge-wrapper'>"
                "<canvas id='temperature-gauge'></canvas>"
                "<div class='gauge-value' id='temperature-value'>" + String(temperature) + " °C</div>"
                "</div>"
                "</div>"
                
                "<div class='card'>"
                "<h2>Humidity</h2>"
                "<div class='gauge-wrapper'>"
                "<canvas id='humidity-gauge'></canvas>"
                "<div class='gauge-value' id='humidity-value'>" + String(humidity) + " %</div>"
                "</div>"
                "</div>"
                
                "<div class='card'>"
                "<h2>Current</h2>"
                "<div class='gauge-wrapper'>"
                "<canvas id='current-gauge'></canvas>"
                "<div class='gauge-value' id='current-value'>" + String(current / 1000.0) + " A</div>"
                "</div>"
                "</div>"
                "</div>"
                
                "<div class='card'>"
                "<h2>Historical Data</h2>"
                "<div class='chart-container'>"
                "<canvas id='history-chart'></canvas>"
                "</div>"
                "</div>"
                
                "</div>"
                "<script src='scripts.js'></script>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

void handleCSS() {
  String css = "* {"
               "  box-sizing: border-box;"
               "  margin: 0;"
               "  padding: 0;"
               "  font-family: Arial, sans-serif;"
               "}"
               
               "body {"
               "  background-color: #f5f0ff;"
               "  padding: 20px;"
               "}"
               
               ".container {"
               "  max-width: 1200px;"
               "  margin: 0 auto;"
               "}"
               
               "h1 {"
               "  text-align: center;"
               "  color: #5d4a8a;"
               "  margin-bottom: 5px;"
               "  padding-top: 10px;"
               "  font-size: 36px;"
               "}"
               
               "h3 {"
               "  text-align: center;"
               "  color: #8a7aae;"
               "  margin-bottom: 30px;"
               "  padding-bottom: 15px;"
               "  border-bottom: 2px solid #9370DB;"
               "}"
               
               "h2 {"
               "  color: #5d4a8a;"
               "  margin-bottom: 15px;"
               "}"
               
               ".card {"
               "  background-color: white;"
               "  border-radius: 8px;"
               "  box-shadow: 0 4px 8px rgba(147, 112, 219, 0.2);"
               "  padding: 20px;"
               "  margin-bottom: 20px;"
               "  border-top: 3px solid #9370DB;"
               "}"
               
               ".gauges-container {"
               "  display: flex;"
               "  flex-wrap: wrap;"
               "  gap: 20px;"
               "  justify-content: space-between;"
               "}"
               
               ".gauges-container .card {"
               "  flex: 1 1 300px;"
               "}"
               
               ".gauge-wrapper {"
               "  position: relative;"
               "  height: 200px;"
               "  display: flex;"
               "  flex-direction: column;"
               "  align-items: center;"
               "}"
               
               ".gauge-value {"
               "  font-size: 24px;"
               "  font-weight: bold;"
               "  color: #5d4a8a;"
               "  margin-top: 10px;"
               "}"
               
               ".chart-container {"
               "  height: 400px;"
               "  width: 100%;"
               "}"
               
               ".status-panel {"
               "  display: flex;"
               "  flex-wrap: wrap;"
               "  gap: 15px;"
               "}"
               
               ".status-item {"
               "  display: flex;"
               "  align-items: center;"
               "  padding: 10px 15px;"
               "  background-color: #f8f5ff;"
               "  border-radius: 5px;"
               "  flex: 1 1 250px;"
               "  border-left: 3px solid #9370DB;"
               "}"
               
               ".status-label {"
               "  font-weight: bold;"
               "  margin-right: 10px;"
               "  color: #7a5fa0;"
               "}"
               
               ".status-value {"
               "  font-size: 16px;"
               "  color: #5d4a8a;"
               "}"
               
               "@media (max-width: 768px) {"
               "  .gauges-container {"
               "    flex-direction: column;"
               "  }"
               "  .gauges-container .card {"
               "    width: 100%;"
               "  }"
               "}";
  server.send(200, "text/css", css);
}

void handleJS() {
  String js = "// Initialize gauges after page load\n"
              "document.addEventListener('DOMContentLoaded', function() {\n"
              "  // Create Temperature Gauge\n"
              "  var tempGauge = new Gauge(document.getElementById('temperature-gauge'));\n"
              "  tempGauge.maxValue = 50;\n"
              "  tempGauge.setMinValue(0);\n"
              "  tempGauge.animationSpeed = 32;\n"
              "  tempGauge.set(0); // Initial value\n"
              "  tempGauge.setOptions({\n"
              "    colorStart: '#b19cd9',\n"
              "    colorStop: '#9370DB',\n"
              "    strokeColor: '#e0e0e0',\n"
              "    pointer: {\n"
              "      length: 0.6,\n"
              "      strokeWidth: 0.035\n"
              "    },\n"
              "    staticLabels: {\n"
              "      font: '12px sans-serif',\n"
              "      labels: [0, 10, 20, 30, 40, 50],\n"
              "      color: '#5d4a8a',\n"
              "      fractionDigits: 0\n"
              "    },\n"
              "    staticZones: [\n"
              "      {strokeStyle: '#c3b1e1', min: 0, max: 10},\n"
              "      {strokeStyle: '#aa8fd8', min: 10, max: 20},\n"
              "      {strokeStyle: '#9370DB', min: 20, max: 30},\n"
              "      {strokeStyle: '#7d5fb9', min: 30, max: 40},\n"
              "      {strokeStyle: '#5d4a8a', min: 40, max: 50}\n"
              "    ],\n"
              "    renderTicks: {\n"
              "      divisions: 5,\n"
              "      divWidth: 1.1,\n"
              "      divLength: 0.7,\n"
              "      divColor: '#333333',\n"
              "      subDivisions: 3,\n"
              "      subLength: 0.5,\n"
              "      subWidth: 0.6,\n"
              "      subColor: '#666666'\n"
              "    },\n"
              "    limitMax: true,\n"
              "    limitMin: true,\n"
              "    highDpiSupport: true\n"
              "  });\n"
              "\n"
              "  // Create Humidity Gauge\n"
              "  var humGauge = new Gauge(document.getElementById('humidity-gauge'));\n"
              "  humGauge.maxValue = 100;\n"
              "  humGauge.setMinValue(0);\n"
              "  humGauge.animationSpeed = 32;\n"
              "  humGauge.set(0); // Initial value\n"
              "  humGauge.setOptions({\n"
              "    colorStart: '#b19cd9',\n"
              "    colorStop: '#9370DB',\n"
              "    strokeColor: '#e0e0e0',\n"
              "    pointer: {\n"
              "      length: 0.6,\n"
              "      strokeWidth: 0.035\n"
              "    },\n"
              "    staticLabels: {\n"
              "      font: '12px sans-serif',\n"
              "      labels: [0, 20, 40, 60, 80, 100],\n"
              "      color: '#5d4a8a',\n"
              "      fractionDigits: 0\n"
              "    },\n"
              "    staticZones: [\n"
              "      {strokeStyle: '#c3b1e1', min: 0, max: 20},\n"
              "      {strokeStyle: '#aa8fd8', min: 20, max: 40},\n"
              "      {strokeStyle: '#9370DB', min: 40, max: 60},\n"
              "      {strokeStyle: '#7d5fb9', min: 60, max: 80},\n"
              "      {strokeStyle: '#5d4a8a', min: 80, max: 100}\n"
              "    ],\n"
              "    renderTicks: {\n"
              "      divisions: 5,\n"
              "      divWidth: 1.1,\n"
              "      divLength: 0.7,\n"
              "      divColor: '#333333',\n"
              "      subDivisions: 4,\n"
              "      subLength: 0.5,\n"
              "      subWidth: 0.6,\n"
              "      subColor: '#666666'\n"
              "    },\n"
              "    limitMax: true,\n"
              "    limitMin: true,\n"
              "    highDpiSupport: true\n"
              "  });\n"
              "\n"
              "  // Create Current Gauge\n"
              "  var currGauge = new Gauge(document.getElementById('current-gauge'));\n"
              "  currGauge.maxValue = 5;\n"
              "  currGauge.setMinValue(0);\n"
              "  currGauge.animationSpeed = 32;\n"
              "  currGauge.set(0); // Initial value\n"
              "  currGauge.setOptions({\n"
              "    colorStart: '#b19cd9',\n"
              "    colorStop: '#9370DB',\n"
              "    strokeColor: '#e0e0e0',\n"
              "    pointer: {\n"
              "      length: 0.6,\n"
              "      strokeWidth: 0.035\n"
              "    },\n"
              "    staticLabels: {\n"
              "      font: '12px sans-serif',\n"
              "      labels: [0, 1, 2, 3, 4, 5],\n"
              "      color: '#5d4a8a',\n"
              "      fractionDigits: 1\n"
              "    },\n"
              "    staticZones: [\n"
              "      {strokeStyle: '#c3b1e1', min: 0, max: 1},\n"
              "      {strokeStyle: '#aa8fd8', min: 1, max: 2},\n"
              "      {strokeStyle: '#9370DB', min: 2, max: 3},\n"
              "      {strokeStyle: '#7d5fb9', min: 3, max: 4},\n"
              "      {strokeStyle: '#5d4a8a', min: 4, max: 5}\n"
              "    ],\n"
              "    renderTicks: {\n"
              "      divisions: 5,\n"
              "      divWidth: 1.1,\n"
              "      divLength: 0.7,\n"
              "      divColor: '#333333',\n"
              "      subDivisions: 5,\n"
              "      subLength: 0.5,\n"
              "      subWidth: 0.6,\n"
              "      subColor: '#666666'\n"
              "    },\n"
              "    limitMax: true,\n"
              "    limitMin: true,\n"
              "    highDpiSupport: true\n"
              "  });\n"
              "\n"
              "  // Create Chart\n"
              "  var ctx = document.getElementById('history-chart').getContext('2d');\n"
              "  var historyChart = new Chart(ctx, {\n"
              "    type: 'line',\n"
              "    data: {\n"
              "      labels: Array(60).fill('').map((_, i) => `-${59-i}s`),\n"
              "      datasets: [\n"
              "        {\n"
              "          label: 'Temperature (°C)',\n"
              "          data: Array(60).fill(null),\n"
              "          borderColor: '#9370DB',\n"
              "          backgroundColor: 'rgba(147, 112, 219, 0.1)',\n"
              "          borderWidth: 2,\n"
              "          tension: 0.4\n"
              "        },\n"
              "        {\n"
              "          label: 'Humidity (%)',\n"
              "          data: Array(60).fill(null),\n"
              "          borderColor: '#b19cd9',\n"
              "          backgroundColor: 'rgba(177, 156, 217, 0.1)',\n"
              "          borderWidth: 2,\n"
              "          tension: 0.4\n"
              "        },\n"
              "        {\n"
              "          label: 'Current (A)',\n"
              "          data: Array(60).fill(null),\n"
              "          borderColor: '#7d5fb9',\n"
              "          backgroundColor: 'rgba(125, 95, 185, 0.1)',\n"
              "          borderWidth: 2,\n"
              "          tension: 0.4\n"
              "        }\n"
              "      ]\n"
              "    },\n"
              "    options: {\n"
              "      responsive: true,\n"
              "      maintainAspectRatio: false,\n"
              "      scales: {\n"
              "        y: {\n"
              "          beginAtZero: true\n"
              "        }\n"
              "      },\n"
              "      animation: {\n"
              "        duration: 500\n"
              "      }\n"
              "    }\n"
              "  });\n"
              "\n"
              "  // Store references to the gauges and chart in global variables\n"
              "  window.tempGauge = tempGauge;\n"
              "  window.humGauge = humGauge;\n"
              "  window.currGauge = currGauge;\n"
              "  window.historyChart = historyChart;\n"
              "\n"
              "  // Initial data update\n"
              "  updateData();\n"
              "\n"
              "  // Update data every 5 seconds\n"
              "  setInterval(updateData, 5000);\n"
              "});\n"
              "\n"
              "// Function to fetch and update data\n"
              "function updateData() {\n"
              "  fetch('/data')\n"
              "    .then(response => response.json())\n"
              "    .then(data => {\n"
              "      // Update gauges\n"
              "      if (window.tempGauge) {\n"
              "        window.tempGauge.set(parseFloat(data.temperature));\n"
              "        document.getElementById('temperature-value').textContent = data.temperature + ' °C';\n"
              "      }\n"
              "      \n"
              "      if (window.humGauge) {\n"
              "        window.humGauge.set(parseFloat(data.humidity));\n"
              "        document.getElementById('humidity-value').textContent = data.humidity + ' %';\n"
              "      }\n"
              "      \n"
              "      if (window.currGauge) {\n"
              "        window.currGauge.set(parseFloat(data.current));\n"
              "        document.getElementById('current-value').textContent = data.current + ' A';\n"
              "      }\n"
              "      \n"
              "      // Update status values\n"
              "      document.getElementById('pump-status').textContent = data.pumpStatus;\n"
              "      document.getElementById('pump-action').textContent = data.lastPumpAction;\n"
              "      \n"
              "      // Update chart data\n"
              "      if (window.historyChart) {\n"
              "        window.historyChart.data.datasets[0].data = data.tempHistory;\n"
              "        window.historyChart.data.datasets[1].data = data.humHistory;\n"
              "        window.historyChart.data.datasets[2].data = data.currHistory;\n"
              "        window.historyChart.update();\n"
              "      }\n"
              "    })\n"
              "    .catch(error => {\n"
              "      console.error('Error fetching data:', error);\n"
              "      document.getElementById('wifi-status').textContent = 'Disconnected';\n"
              "    });\n"
              "}\n";
  server.send(200, "application/javascript", js);
}

void handleData() {
  // Create history arrays for JSON
  String tempHistoryJSON = "[";
  String humHistoryJSON = "[";
  String currHistoryJSON = "[";
  
  for (int i = 0; i < HISTORY_SIZE; i++) {
    int idx = (historyIndex + i) % HISTORY_SIZE;
    
    tempHistoryJSON += String(temperatureHistory[idx]);
    humHistoryJSON += String(humidityHistory[idx]);
    currHistoryJSON += String(currentHistory[idx]);
    
    if (i < HISTORY_SIZE - 1) {
      tempHistoryJSON += ",";
      humHistoryJSON += ",";
      currHistoryJSON += ",";
    }
  }
  
  tempHistoryJSON += "]";
  humHistoryJSON += "]";
  currHistoryJSON += "]";

  // Create JSON response
  String jsonResponse = "{";
  jsonResponse += "\"temperature\":" + String(temperature) + ",";
  jsonResponse += "\"humidity\":" + String(humidity) + ",";
  jsonResponse += "\"current\":" + String(current / 1000.0) + ",";
  jsonResponse += "\"pumpStatus\":\"" + pumpStatus + "\",";
  jsonResponse += "\"lastPumpAction\":\"" + lastPumpAction + "\",";
  jsonResponse += "\"tempHistory\":" + tempHistoryJSON + ",";
  jsonResponse += "\"humHistory\":" + humHistoryJSON + ",";
  jsonResponse += "\"currHistory\":" + currHistoryJSON;
  jsonResponse += "}";
  
  server.send(200, "application/json", jsonResponse);
}