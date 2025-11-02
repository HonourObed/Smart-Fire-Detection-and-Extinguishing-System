#include <WiFi.h>
#include <WebServer.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>

#define FLAME_SENSOR_PIN    15  // Digital pin for flame sensor
#define SMOKE_SENSOR_DIGITAL 4 // Digital pin for smoke sensor DO
#define RED_LED_PIN         5   // Red LED for fire alert
#define YELLOW_LED_PIN      18  // Yellow LED for smoke alert
#define BUZZER_PIN          19  // Active buzzer pin
#define RELAY_PIN           21  // Relay control pin for pump
#define SERVO_PIN           22  // Servo motor pin

// WiFi credentials (Station mode)
const char* ssid = "Honour's Device";
const char* password = "quark-gluon-plasma-eon";

// Access Point credentials (Fallback)
const char* apSSID = "FlameGuard-AP";
const char* apPassword = "flame123";

// Telegram credentials
#define BOT_TOKEN "8133365908:AAGWG0F4UMJ9ejaByOUDxoxN4uMyWqQBrDU"
#define CHAT_ID "5484017015"

// NTP setup for WAT (UTC+1)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // UTC+1 for WAT

WebServer server(80);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

bool manualOverride = false;
bool manualRelayState = false;
bool buzzerEnabled = true;
unsigned long lastAlertTime = 0;
const unsigned long alertCooldown = 5000; // Reduced to 5 seconds for testing
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000; // Check WiFi every 10 seconds
unsigned long lastTimeBotRan = 0;
const int botRequestDelay = 1000; // Poll Telegram every 1 second
unsigned long startTime = 0; // For uptime tracking
bool isAPMode = false; // Track if in AP mode
// Debouncing variables
const int debounceDelay = 50; // 50ms debounce for sensors
bool lastFlameState = false;
bool lastSmokeState = false;
unsigned long lastFlameChange = 0;
unsigned long lastSmokeChange = 0;
// Confirmation for stable detection
const int detectionConfirmationDelay = 500; // 500ms to confirm stable detection
unsigned long detectionStartTime = 0;
bool detectionConfirmed = false;
// Servo variables
Servo myServo;
int currentPos = 0;
int direction = 1;
unsigned long lastServoMove = 0;
const unsigned long servoDelay = 10; // ms per degree step for faster rotation
unsigned long sweepStartTime = 0; // For measuring sweep duration
bool servoTestActive = false; // For Telegram servo test
// Buzzer test variables
unsigned long buzzerTestStartTime = 0;
bool buzzerTestActive = false;

// Status log for web interface (last 5 events)
String statusLog = "";

void addToLog(String message) {
  String timestamp = timeClient.getFormattedTime();
  statusLog = "[" + timestamp + " WAT] " + message + "\n" + statusLog;
  int lines = 0;
  for (int i = 0; i < statusLog.length(); i++) {
    if (statusLog[i] == '\n') lines++;
    if (lines > 5) {
      statusLog = statusLog.substring(0, i);
      break;
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", getPage());
}

void handlePumpOn() {
  manualOverride = true;
  manualRelayState = true;
  addToLog("Pump turned ON manually");
  server.send(200, "text/html", getPage());
}

void handlePumpOff() {
  manualOverride = true;
  manualRelayState = false;
  addToLog("Pump turned OFF manually");
  server.send(200, "text/html", getPage());
}

void handleAuto() {
  manualOverride = false;
  addToLog("Switched to Auto mode");
  server.send(200, "text/html", getPage());
}

void handleBuzzerToggle() {
  buzzerEnabled = !buzzerEnabled;
  addToLog("Buzzer " + String(buzzerEnabled ? "enabled" : "disabled") + " via web");
  server.send(200, "text/html", getPage());
}

void handleTestAlert() {
  bool flame = !digitalRead(FLAME_SENSOR_PIN); // Read current sensor state
  bool smoke = !digitalRead(SMOKE_SENSOR_DIGITAL);
  String message = "🚨🔥 Web Test Alert!\n";
  message += "Flame: " + String(flame ? "Detected" : "Not Detected") + "\n";
  message += "Smoke: " + String(smoke ? "Detected" : "Not Detected") + "\n";
  message += "Pump: " + String(digitalRead(RELAY_PIN) ? "ON" : "OFF") + "\n";
  message += "Buzzer: " + String(buzzerEnabled ? "Enabled" : "Disabled") + "\n";
  message += "Servo Position: " + String(currentPos) + " degrees\n";
  message += "Time: " + timeClient.getFormattedTime() + " WAT";
  if (!isAPMode) {
    bot.sendMessage(CHAT_ID, message, "");
  }
  addToLog("Web test alert sent");
  server.send(200, "text/html", getPage());
}

String getPage() {
  bool flame = !digitalRead(FLAME_SENSOR_PIN);
  bool smoke = !digitalRead(SMOKE_SENSOR_DIGITAL);
  bool relayOn = digitalRead(RELAY_PIN);

  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>FlameGuard Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta charset="UTF-8">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
      color: #fff;
      position: relative;
      overflow-x: hidden;
    }
    
    .background-pattern {
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: 
        radial-gradient(circle at 20% 80%, rgba(120, 119, 198, 0.3) 0%, transparent 50%),
        radial-gradient(circle at 80% 20%, rgba(255, 119, 48, 0.3) 0%, transparent 50%),
        radial-gradient(circle at 40% 40%, rgba(120, 119, 198, 0.2) 0%, transparent 50%);
      z-index: -1;
    }
    
    .container {
      max-width: 1000px;
      margin: 0 auto;
      background: rgba(255, 255, 255, 0.95);
      backdrop-filter: blur(20px);
      border-radius: 24px;
      box-shadow: 0 20px 60px rgba(0, 0, 0, 0.15);
      color: #1a202c;
      border: 1px solid rgba(255, 255, 255, 0.2);
      overflow: hidden;
    }
    
    .header {
      background: linear-gradient(135deg, #ff6b6b, #ee5a24);
      padding: 30px;
      text-align: center;
      position: relative;
    }
    
    .header::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><defs><pattern id="flame" patternUnits="userSpaceOnUse" width="20" height="20"><circle cx="10" cy="10" r="1" fill="rgba(255,255,255,0.1)"/></pattern></defs><rect width="100" height="100" fill="url(%23flame)"/></svg>');
      opacity: 0.3;
    }
    
    h1 {
      font-size: 2.8em;
      font-weight: 700;
      color: #fff;
      text-shadow: 0 4px 8px rgba(0, 0, 0, 0.3);
      margin-bottom: 10px;
      position: relative;
      z-index: 1;
    }
    
    #clock {
      font-size: 1.2em;
      font-weight: 500;
      color: rgba(255, 255, 255, 0.9);
      position: relative;
      z-index: 1;
    }
    
    .content {
      padding: 30px;
    }
    
    .alert-banner {
      display: none;
      background: linear-gradient(135deg, #ff3838, #ff1744);
      color: #fff;
      padding: 16px;
      margin-bottom: 24px;
      border-radius: 12px;
      font-weight: 600;
      text-align: center;
      animation: alertPulse 2s infinite;
      box-shadow: 0 8px 32px rgba(255, 56, 56, 0.3);
    }
    
    .alert-banner.show {
      display: block;
    }
    
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    
    .status-card {
      background: linear-gradient(135deg, #f8fafc, #e2e8f0);
      border: 1px solid #e2e8f0;
      border-radius: 16px;
      padding: 20px;
      transition: all 0.3s ease;
      position: relative;
      overflow: hidden;
    }
    
    .status-card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      width: 4px;
      height: 100%;
      background: var(--accent-color, #3b82f6);
      border-radius: 0 4px 4px 0;
    }
    
    .status-card:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 25px rgba(0, 0, 0, 0.1);
    }
    
    .status-card.wifi { --accent-color: #10b981; }
    .status-card.flame { --accent-color: #ef4444; }
    .status-card.smoke { --accent-color: #f59e0b; }
    .status-card.pump { --accent-color: #3b82f6; }
    .status-card.mode { --accent-color: #8b5cf6; }
    .status-card.buzzer { --accent-color: #06b6d4; }
    
    .status-label {
      font-size: 0.875rem;
      font-weight: 500;
      color: #64748b;
      margin-bottom: 8px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    
    .status-value {
      font-size: 1.125rem;
      font-weight: 600;
      color: #1e293b;
    }
    
    .status-icon {
      position: absolute;
      top: 16px;
      right: 16px;
      font-size: 1.5rem;
      opacity: 0.3;
    }
    
    .button-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
      gap: 16px;
      margin-bottom: 30px;
    }
    
    .btn {
      padding: 16px 24px;
      font-size: 1rem;
      font-weight: 600;
      border: none;
      border-radius: 12px;
      cursor: pointer;
      transition: all 0.3s ease;
      text-decoration: none;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      position: relative;
      overflow: hidden;
    }
    
    .btn::before {
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 100%;
      background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
      transition: left 0.5s;
    }
    
    .btn:hover::before {
      left: 100%;
    }
    
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 8px 25px rgba(0, 0, 0, 0.15);
    }
    
    .btn:active {
      transform: translateY(0);
    }
    
    .btn-success {
      background: linear-gradient(135deg, #10b981, #059669);
      color: white;
    }
    
    .btn-danger {
      background: linear-gradient(135deg, #ef4444, #dc2626);
      color: white;
    }
    
    .btn-warning {
      background: linear-gradient(135deg, #f59e0b, #d97706);
      color: white;
    }
    
    .btn-info {
      background: linear-gradient(135deg, #06b6d4, #0891b2);
      color: white;
    }
    
    .btn-secondary {
      background: linear-gradient(135deg, #8b5cf6, #7c3aed);
      color: white;
    }
    
    .btn-primary {
      background: linear-gradient(135deg, #3b82f6, #2563eb);
      color: white;
    }
    
    .log-section {
      background: linear-gradient(135deg, #1e293b, #334155);
      border-radius: 16px;
      padding: 20px;
      color: #e2e8f0;
    }
    
    .log-header {
      font-size: 1.125rem;
      font-weight: 600;
      margin-bottom: 16px;
      color: #f1f5f9;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    
    .log-content {
      font-family: 'Courier New', monospace;
      font-size: 0.875rem;
      line-height: 1.6;
      background: rgba(0, 0, 0, 0.3);
      padding: 16px;
      border-radius: 8px;
      white-space: pre-wrap;
      max-height: 200px;
      overflow-y: auto;
      border: 1px solid rgba(255, 255, 255, 0.1);
    }
    
    .log-content::-webkit-scrollbar {
      width: 6px;
    }
    
    .log-content::-webkit-scrollbar-track {
      background: rgba(255, 255, 255, 0.1);
      border-radius: 3px;
    }
    
    .log-content::-webkit-scrollbar-thumb {
      background: rgba(255, 255, 255, 0.3);
      border-radius: 3px;
    }
    
    @keyframes alertPulse {
      0%, 100% { 
        box-shadow: 0 8px 32px rgba(255, 56, 56, 0.3);
        transform: scale(1);
      }
      50% { 
        box-shadow: 0 12px 40px rgba(255, 56, 56, 0.5);
        transform: scale(1.02);
      }
    }
    
    @media (max-width: 768px) {
      .container {
        margin: 10px;
        border-radius: 16px;
      }
      
      .header {
        padding: 20px;
      }
      
      h1 {
        font-size: 2.2em;
      }
      
      .content {
        padding: 20px;
      }
      
      .status-grid {
        grid-template-columns: 1fr;
        gap: 16px;
      }
      
      .button-grid {
        grid-template-columns: repeat(2, 1fr);
        gap: 12px;
      }
      
      .btn {
        padding: 12px 16px;
        font-size: 0.875rem;
      }
    }
    
    @media (max-width: 480px) {
      body {
        padding: 10px;
      }
      
      h1 {
        font-size: 1.8em;
      }
      
      .button-grid {
        grid-template-columns: 1fr;
      }
    }
  </style>
</head>
<body>
  <div class="background-pattern"></div>
  <div class="container">
    <div class="header">
      <h1>🔥 FlameGuard Control</h1>
      <div id="clock"></div>
    </div>
    
    <div class="content">
      <div class="alert-banner" id="alertBanner">
        🚨 Critical Alert: Flame and Smoke Detected! 🚨
      </div>
      
      <div class="status-grid">
        <div class="status-card wifi">
          <div class="status-icon">📶</div>
          <div class="status-label">WiFi Status</div>
          <div class="status-value">)rawliteral";
  page += isAPMode ? "Access Point (FlameGuard-AP)" : (WiFi.status() == WL_CONNECTED ? "Connected to " + String(ssid) : "Disconnected");
  page += R"rawliteral(</div>
        </div>
        
        <div class="status-card flame">
          <div class="status-icon">🔥</div>
          <div class="status-label">Flame Detection</div>
          <div class="status-value">)rawliteral";
  page += flame ? "Detected" : "Not Detected";
  page += R"rawliteral(</div>
        </div>
        
        <div class="status-card smoke">
          <div class="status-icon">💨</div>
          <div class="status-label">Smoke Detection</div>
          <div class="status-value">)rawliteral";
  page += smoke ? "Detected" : "Not Detected";
  page += R"rawliteral(</div>
        </div>
        
        <div class="status-card pump">
          <div class="status-icon">💧</div>
          <div class="status-label">Water Pump</div>
          <div class="status-value">)rawliteral";
  page += relayOn ? "ON" : "OFF";
  page += R"rawliteral(</div>
        </div>
        
        <div class="status-card mode">
          <div class="status-icon">⚙️</div>
          <div class="status-label">Operation Mode</div>
          <div class="status-value">)rawliteral";
  page += manualOverride ? "Manual" : "Auto";
  page += R"rawliteral(</div>
        </div>
        
        <div class="status-card buzzer">
          <div class="status-icon">🔊</div>
          <div class="status-label">Buzzer Status</div>
          <div class="status-value">)rawliteral";
  page += buzzerEnabled ? "Enabled" : "Disabled";
  page += R"rawliteral(</div>
        </div>
      </div>
      
      <div class="button-grid">
        <button class="btn btn-success" onclick="window.location.href='/pump_on'">
          💧 Turn Pump ON
        </button>
        <button class="btn btn-danger" onclick="window.location.href='/pump_off'">
          ⏹️ Turn Pump OFF
        </button>
        <button class="btn btn-warning" onclick="window.location.href='/auto'">
          🤖 Auto Mode
        </button>
        <button class="btn btn-info" onclick="window.location.href='/buzzer_toggle'">
          🔔 Toggle Buzzer
        </button>
        <button class="btn btn-secondary" onclick="window.location.href='/test_alert'">
          🧪 Test Alert
        </button>
        <button class="btn btn-primary" onclick="window.location.reload()">
          🔄 Refresh
        </button>
      </div>
      
      <div class="log-section">
        <div class="log-header">
          📋 System Status Log
        </div>
        <div class="log-content">)rawliteral";
  page += statusLog;
  page += R"rawliteral(</div>
      </div>
    </div>
  </div>
  <script>
    function updateClock() {
      let now = new Date();
      let options = { timeZone: 'Africa/Lagos', year: 'numeric', month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false };
      document.getElementById('clock').innerText = now.toLocaleString('en-US', options) + ' WAT';
    }
    setInterval(updateClock, 1000);
    updateClock();
    let alertBanner = document.getElementById('alertBanner');
    if ()rawliteral";
  page += (flame && smoke) ? "true" : "false";
  page += R"rawliteral() {
      alertBanner.classList.add('show');
    } else {
      alertBanner.classList.remove('show');
    }
  </script>
</body>
</html>
)rawliteral";
  return page;
}

String getUptime() {
  unsigned long seconds = (millis() - startTime) / 1000;
  unsigned long hours = seconds / 3600;
  seconds %= 3600;
  unsigned long minutes = seconds / 60;
  seconds %= 60;
  return String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
}

void handleTelegramCommands() {
  if (isAPMode) return; // Skip Telegram in AP mode
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    if (chat_id != CHAT_ID) continue;

    String response = "🔥 FlameGuard Response:\n";
    if (text == "/status") {
      bool flame = !digitalRead(FLAME_SENSOR_PIN);
      bool smoke = !digitalRead(SMOKE_SENSOR_DIGITAL);
      response += "Flame: " + String(flame ? "Detected" : "Not Detected") + "\n";
      response += "Smoke: " + String(smoke ? "Detected" : "Not Detected") + "\n";
      response += "Pump: " + String(digitalRead(RELAY_PIN) ? "ON" : "OFF") + "\n";
      response += "Mode: " + String(manualOverride ? "Manual" : "Auto") + "\n";
      response += "Buzzer: " + String(buzzerEnabled ? "Enabled" : "Disabled") + "\n";
      response += "Servo Position: " + String(currentPos) + " degrees\n";
      response += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
      response += "Time: " + timeClient.getFormattedTime() + " WAT";
    } else if (text == "/pump_on") {
      manualOverride = true;
      manualRelayState = true;
      addToLog("Pump turned ON via Telegram");
      response += "Pump turned ON manually.";
    } else if (text == "/pump_off") {
      manualOverride = true;
      manualRelayState = false;
      addToLog("Pump turned OFF via Telegram");
      response += "Pump turned OFF manually.";
    } else if (text == "/auto") {
      manualOverride = false;
      addToLog("Switched to Auto mode via Telegram");
      response += "Switched to Auto mode.";
    } else if (text == "/buzzer_toggle") {
      buzzerEnabled = !buzzerEnabled;
      digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off after toggle
      addToLog("Buzzer " + String(buzzerEnabled ? "enabled" : "disabled") + " via Telegram");
      response += "Buzzer " + String(buzzerEnabled ? "enabled" : "disabled") + ".";
    } else if (text == "/buzzer_test") {
      buzzerTestActive = true;
      buzzerTestStartTime = millis();
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("Buzzer test started");
      addToLog("Buzzer test started via Telegram");
      response += "Buzzer test started: ON for 2 seconds.";
    } else if (text == "/test_alert") {
      bool flame = !digitalRead(FLAME_SENSOR_PIN); // Read current sensor state
      bool smoke = !digitalRead(SMOKE_SENSOR_DIGITAL);
      response += "🚨🔥 Test Alert!\n";
      response += "Flame: " + String(flame ? "Detected" : "Not Detected") + "\n";
      response += "Smoke: " + String(smoke ? "Detected" : "Not Detected") + "\n";
      response += "Pump: " + String(digitalRead(RELAY_PIN) ? "ON" : "OFF") + "\n";
      response += "Buzzer: " + String(buzzerEnabled ? "Enabled" : "Disabled") + "\n";
      response += "Servo Position: " + String(currentPos) + " degrees\n";
      response += "Time: " + timeClient.getFormattedTime() + " WAT";
      addToLog("Test alert sent via Telegram");
    } else if (text == "/sensitivity") {
      int flameRaw = digitalRead(FLAME_SENSOR_PIN);
      int smokeRaw = digitalRead(SMOKE_SENSOR_DIGITAL);
      response += "Sensor Sensitivity:\n";
      response += "Flame Sensor (GPIO15): " + String(flameRaw) + " (" + String(flameRaw == LOW ? "LOW" : "HIGH") + ")\n";
      response += "Smoke Sensor (GPIO16): " + String(smokeRaw) + " (" + String(smokeRaw == LOW ? "LOW" : "HIGH") + ")\n";
      response += "Time: " + timeClient.getFormattedTime() + " WAT";
      addToLog("Sensitivity check via Telegram");
    } else if (text == "/servo_test") {
      servoTestActive = true;
      sweepStartTime = millis();
      currentPos = 0;
      direction = 1;
      myServo.write(0);
      addToLog("Servo test started via Telegram");
      response += "Starting servo test: Sweeping from 0 to 180 degrees and back.";
    } else if (text == "/restart") {
      response += "🔄 Restarting FlameGuard system...";
      addToLog("System restart via Telegram");
      bot.sendMessage(CHAT_ID, response, "");
      delay(1000); // Allow message to send
      ESP.restart();
    } else if (text == "/uptime") {
      response += "System Uptime: " + getUptime() + "\n";
      response += "Time: " + timeClient.getFormattedTime() + " WAT";
      addToLog("Uptime check via Telegram");
    } else {
      response += "Unknown command. Use /status, /pump_on, /pump_off, /auto, /buzzer_toggle, /buzzer_test, /test_alert, /sensitivity, /servo_test, /restart, or /uptime.";
    }
    bot.sendMessage(CHAT_ID, response, "");
  }
}

void reconnectWiFi() {
  if (isAPMode) return; // Skip WiFi reconnect in AP mode
  if (WiFi.status() != WL_CONNECTED && millis() - lastWiFiCheck >= wifiCheckInterval) {
    Serial.println("WiFi disconnected. Reconnecting...");
    addToLog("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconnected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength (RSSI): ");
      Serial.println(WiFi.RSSI());
      addToLog("WiFi reconnected");
      timeClient.begin();
      timeClient.update();
      bot.sendMessage(CHAT_ID, "🔄 WiFi reconnected: " + WiFi.localIP().toString(), "");
    } else {
      Serial.println("\nWiFi reconnection failed");
      Serial.print("WiFi status code: ");
      Serial.println(WiFi.status());
      addToLog("WiFi reconnection failed");
    }
    lastWiFiCheck = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Flame and Smoke Sensor Reading Starting...");
  Serial.println("Board: ESP32 Dev Module");
  Serial.println("Libraries: WiFi, WebServer, UniversalTelegramBot, ArduinoJson, NTPClient, ESP32Servo");
  Serial.print("WiFi SSID: ");
  Serial.println(ssid);
  Serial.print("WiFi Password: ");
  Serial.println(password);
  startTime = millis(); // Initialize uptime tracking

  pinMode(FLAME_SENSOR_PIN, INPUT);
  pinMode(SMOKE_SENSOR_DIGITAL, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(0); // Set initial position to 0 degrees
  Serial.println("Servo initialized on GPIO22");
  addToLog("Servo initialized");

  // Ensure Auto mode at startup
  manualOverride = false;
  addToLog("System started in Auto mode");

  // Test sensor readings at startup
  int initialFlameVal = digitalRead(FLAME_SENSOR_PIN);
  int initialSmokeVal = digitalRead(SMOKE_SENSOR_DIGITAL);
  Serial.print("Initial Flame Sensor Reading (GPIO15): ");
  Serial.println(initialFlameVal == LOW ? "LOW (Fire Detected)" : "HIGH (No Fire)");
  Serial.print("Initial Smoke Sensor Reading (GPIO16): ");
  Serial.println(initialSmokeVal == LOW ? "LOW (Smoke Detected)" : "HIGH (No Smoke)");

  // Test buzzer at startup
  Serial.println("Testing buzzer on GPIO19...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500); // Buzz for 500ms
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Buzzer test complete");
  addToLog("Buzzer startup test complete");

  // Connect to WiFi (Station Mode)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 60) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.println(WiFi.RSSI());
    addToLog("WiFi connected to " + String(ssid));
  } else {
    Serial.println("\nWiFi connection failed");
    Serial.print("WiFi status code: ");
    Serial.println(WiFi.status());
    addToLog("WiFi connection failed. Starting AP mode...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID, apPassword);
    isAPMode = true;
    Serial.println("Started Access Point");
    Serial.print("AP SSID: ");
    Serial.println(apSSID);
    Serial.print("AP Password: ");
    Serial.println(apPassword);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    addToLog("Started AP: " + String(apSSID));
  }

  // Initialize NTP client
  timeClient.begin();
  timeClient.update();

  // Configure Telegram client (only in STA mode)
  if (!isAPMode) {
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
    String startupMessage = "🔥 FlameGuard system started! Ready to monitor flame and smoke at ";
    startupMessage += timeClient.getFormattedTime() + " WAT";
    startupMessage += "\nWiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected to " + String(ssid) : "Disconnected");
    if (bot.sendMessage(CHAT_ID, startupMessage, "")) {
      Serial.println("Telegram startup message sent");
      addToLog("Telegram startup message sent");
    } else {
      Serial.println("Telegram connection failed");
      addToLog("Telegram connection failed");
    }
  } else {
    Serial.println("Telegram disabled in AP mode");
    addToLog("Telegram disabled in AP mode");
  }

  server.on("/", handleRoot);
  server.on("/pump_on", handlePumpOn);
  server.on("/pump_off", handlePumpOff);
  server.on("/auto", handleAuto);
  server.on("/buzzer_toggle", handleBuzzerToggle);
  server.on("/test_alert", handleTestAlert);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  timeClient.update();
  if (!isAPMode) {
    reconnectWiFi();
    if (millis() - lastTimeBotRan >= botRequestDelay) {
      handleTelegramCommands();
      lastTimeBotRan = millis();
    }
  }

  // Handle buzzer test
  if (buzzerTestActive && millis() - buzzerTestStartTime >= 2000) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerTestActive = false;
    Serial.println("Buzzer test ended");
    addToLog("Buzzer test ended");
    if (!isAPMode) {
      bot.sendMessage(CHAT_ID, "Buzzer test completed: Buzzer turned OFF", "");
    }
  }

  // Debounce sensor readings
  bool rawFlame = digitalRead(FLAME_SENSOR_PIN) == LOW;
  bool rawSmoke = digitalRead(SMOKE_SENSOR_DIGITAL) == LOW;
  bool flameState = lastFlameState;
  bool smokeDetected = lastSmokeState;

  // Update flame state with debouncing
  if (rawFlame != lastFlameState && millis() - lastFlameChange >= debounceDelay) {
    flameState = rawFlame;
    lastFlameState = rawFlame;
    lastFlameChange = millis();
    Serial.println(rawFlame ? "Flame Sensor: LOW (Fire Detected)" : "Flame Sensor: HIGH (No Fire)");
    addToLog(rawFlame ? "Flame detected" : "Flame cleared");
  }

  // Update smoke state with debouncing
  if (rawSmoke != lastSmokeState && millis() - lastSmokeChange >= debounceDelay) {
    smokeDetected = rawSmoke;
    lastSmokeState = rawSmoke;
    lastSmokeChange = millis();
    Serial.println(rawSmoke ? "Smoke Sensor: LOW (Smoke Detected)" : "Smoke Sensor: HIGH (No Smoke)");
    addToLog(rawSmoke ? "Smoke detected" : "Smoke cleared");
  }

  // Debug sensor values
  Serial.print("Flame Sensor Raw (GPIO15): ");
  Serial.println(digitalRead(FLAME_SENSOR_PIN) == LOW ? "LOW" : "HIGH");
  Serial.print("Smoke Sensor Raw (GPIO16): ");
  Serial.println(digitalRead(SMOKE_SENSOR_DIGITAL) == LOW ? "LOW" : "HIGH");
  Serial.print("Flame State (Debounced): ");
  Serial.println(flameState ? "Detected" : "Not Detected");
  Serial.print("Smoke State (Debounced): ");
  Serial.println(smokeDetected ? "Detected" : "Not Detected");

  if (flameState) {
    digitalWrite(RED_LED_PIN, HIGH);
    Serial.println("Fire Detected - Red LED ON");
  } else {
    digitalWrite(RED_LED_PIN, LOW);
  }

  if (smokeDetected) {
    digitalWrite(YELLOW_LED_PIN, HIGH);
    Serial.println("Smoke Detected - Yellow LED ON");
  } else {
    digitalWrite(YELLOW_LED_PIN, LOW);
  }

  // Confirm stable detection before sending alert
  if (flameState && smokeDetected) {
    if (!detectionConfirmed) {
      if (detectionStartTime == 0) {
        detectionStartTime = millis();
        Serial.println("Stable detection started for flame and smoke");
        addToLog("Stable detection started");
      } else if (millis() - detectionStartTime >= detectionConfirmationDelay) {
        detectionConfirmed = true;
        Serial.println("Stable detection confirmed for flame and smoke");
        addToLog("Stable detection confirmed");
      }
    }
    if (detectionConfirmed && !manualOverride && (millis() - lastAlertTime >= alertCooldown) && !isAPMode) {
      String message = "🚨🔥 Critical Alert!\n";
      message += "Flame: Detected\n";
      message += "Smoke: Detected\n";
      message += "Pump: ON\n";
      message += "Buzzer: " + String(buzzerEnabled ? "Enabled" : "Disabled") + "\n";
      message += "Time: " + timeClient.getFormattedTime() + " WAT";
      if (bot.sendMessage(CHAT_ID, message, "")) {
        Serial.println("Telegram critical alert sent successfully");
        addToLog("Critical alert sent: Flame and Smoke detected");
      } else {
        Serial.println("Failed to send Telegram critical alert");
        addToLog("Failed to send critical alert");
      }
      lastAlertTime = millis();
    } else if (manualOverride) {
      Serial.println("Critical alert not sent: System in Manual mode");
      addToLog("Critical alert blocked: System in Manual mode");
    } else if (isAPMode) {
      Serial.println("Critical alert not sent: System in AP mode");
      addToLog("Critical alert blocked: System in AP mode");
    } else if (millis() - lastAlertTime < alertCooldown) {
      Serial.println("Critical alert not sent: Cooldown active");
      addToLog("Critical alert blocked: Cooldown active");
    } else if (!detectionConfirmed) {
      Serial.println("Critical alert not sent: Awaiting stable detection");
      addToLog("Critical alert blocked: Awaiting stable detection");
    }
    // Activate buzzer if enabled and not in test mode
    if (buzzerEnabled && !buzzerTestActive) {
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("Buzzer ON: Flame and Smoke detected");
      addToLog("Buzzer ON: Flame and Smoke detected");
    } else if (!buzzerEnabled && !buzzerTestActive) {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Buzzer OFF: Disabled");
      addToLog("Buzzer OFF: Disabled");
    }
  } else {
    if (!buzzerTestActive) {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Buzzer OFF: No flame or smoke");
      addToLog("Buzzer OFF: No flame or smoke");
    }
    detectionStartTime = 0;
    detectionConfirmed = false;
  }

  if (manualOverride) {
    digitalWrite(RELAY_PIN, manualRelayState ? HIGH : LOW);
  } else {
    digitalWrite(RELAY_PIN, (flameState && smokeDetected) ? HIGH : LOW);
  }

  // Servo control: Rotate 0 to 180 degrees when pump is ON and flame/smoke detected, or during servo test
  if ((flameState && smokeDetected && digitalRead(RELAY_PIN)) || servoTestActive) {
    if (millis() - lastServoMove >= servoDelay) {
      if (sweepStartTime == 0) {
        sweepStartTime = millis();
        Serial.println("Servo sweep started");
      }
      currentPos += direction;
      if (currentPos >= 180) {
        currentPos = 180;
        direction = -1; // Reverse direction
        if (servoTestActive) {
          unsigned long sweepTime = millis() - sweepStartTime;
          Serial.print("Servo sweep to 180 degrees took: ");
          Serial.print(sweepTime);
          Serial.println(" ms");
          addToLog("Servo test sweep to 180 degrees took " + String(sweepTime) + " ms");
        }
      } else if (currentPos <= 0) {
        currentPos = 0;
        direction = 1; // Forward direction
        if (servoTestActive) {
          unsigned long sweepTime = millis() - sweepStartTime;
          Serial.print("Servo full sweep (0-180-0) took: ");
          Serial.print(sweepTime);
          Serial.println(" ms");
          addToLog("Servo test full sweep took " + String(sweepTime) + " ms");
          bot.sendMessage(CHAT_ID, "Servo test completed: Full sweep took " + String(sweepTime) + " ms", "");
          servoTestActive = false;
          sweepStartTime = 0;
        }
      }
      myServo.write(currentPos);
      Serial.print("Servo Position: ");
      Serial.println(currentPos);
      lastServoMove = millis();
    }
  } else {
    if (currentPos != 0) {
      currentPos = 0;
      myServo.write(0); // Return to 0 degrees when pump is off
      Serial.println("Servo returned to 0 degrees");
      addToLog("Servo returned to 0 degrees");
      if (sweepStartTime != 0) {
        sweepStartTime = 0; // Reset sweep timer
      }
    }
    direction = 1; // Reset direction for next activation
    servoTestActive = false; // Cancel test if interrupted
  }

  Serial.println("--- Sensor Readings ---");
  Serial.print("WiFi Status: ");
  Serial.println(isAPMode ? "Access Point (FlameGuard-AP)" : (WiFi.status() == WL_CONNECTED ? "Connected to " + String(ssid) : "Disconnected"));
  Serial.print("IP Address: ");
  Serial.println(isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
  Serial.print("Signal Strength (RSSI): ");
  Serial.println(isAPMode ? "N/A (AP Mode)" : String(WiFi.RSSI()));
  Serial.print("Flame Sensor (GPIO15): ");
  Serial.println(flameState ? "LOW (Fire Detected)" : "HIGH (No Fire)");
  Serial.print("Smoke Sensor (GPIO16): ");
  Serial.println(smokeDetected ? "LOW (Smoke Detected)" : "HIGH (No Smoke)");
  Serial.print("Mode: ");
  Serial.println(manualOverride ? "Manual" : "Auto");
  Serial.print("Pump: ");
  Serial.println(digitalRead(RELAY_PIN) ? "ON" : "OFF");
  Serial.print("Buzzer: ");
  Serial.println(digitalRead(BUZZER_PIN) ? "ON" : "OFF");
  Serial.print("Detection Confirmed: ");
  Serial.println(detectionConfirmed ? "Yes" : "No");
  Serial.print("Servo Position: ");
  Serial.println(currentPos);
  Serial.print("Time: ");
  Serial.println(timeClient.getFormattedTime() + " WAT");
  Serial.print("Uptime: ");
  Serial.println(getUptime());
  Serial.println("----------------------");
  delay(100); // Reduced from 250ms to minimize loop overhead
}