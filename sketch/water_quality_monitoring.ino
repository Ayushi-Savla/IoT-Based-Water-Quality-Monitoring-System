#include <ph4502c_sensor.h>

#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Yoshi";
const char* password = "ayushi123789";
const char* mqttServer = "mqtt.thingsboard.cloud";
const int mqttPort = 1883;
const char* accessToken = "giav4pp5utqbnubdqvp1";

// SIM PIN
String simPIN = "5065";

WiFiClient espClient;
PubSubClient client(espClient);

// --- Sensor Pins ---
//#define PH4502C_PH_PIN     33
//#define PH4502C_TEMPERATURE_PIN   32
#define TURB_PIN   34
#define EC_PIN     35
#define BUZZER_PIN 27

#define PH4502C_TEMPERATURE_PIN 32
#define PH4502C_PH_PIN 33
#define PH4502C_PH_TRIGGER_PIN 18 
#define PH4502C_CALIBRATION 14.8f
#define PH4502C_READING_INTERVAL 100
#define PH4502C_READING_COUNT 10
#define ADC_RESOLUTION 4096.0f

PH4502C_Sensor ph4502c(
  PH4502C_PH_PIN,
  PH4502C_TEMPERATURE_PIN,
  PH4502C_CALIBRATION,
  PH4502C_READING_INTERVAL,
  PH4502C_READING_COUNT,
  ADC_RESOLUTION
);

// Edge-triggered alert flag
bool alertActive = false;

HardwareSerial SIM900(1);
const int SIM_RX = 16;
const int SIM_TX = 17;

bool sendAT(String cmd, String expectedResponse, int timeout = 5000) {
  Serial.print("-> Sending: "); Serial.println(cmd);

  // Flush any leftover serial data
  while (SIM900.available()) SIM900.read();

  SIM900.println(cmd);

  long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    while (SIM900.available()) {
      char c = SIM900.read();
      response += c;
    }

    // Check if expected response is in the buffer
    if (response.indexOf(expectedResponse) != -1) {
      Serial.print("<- Received: "); Serial.println(response);
      return true;
    }

    delay(10); // small delay to allow serial data to arrive
    yield();   // ESP32 friendly
  }

  Serial.print("<- Timeout/Error. Final Response: ");
  Serial.println(response);
  return false;
}

void unlockSIM() {
  Serial.println("Checking SIM PIN status...");
  // Check if SIM is ready. Expects: +CPIN: READY or +CPIN: SIM PIN
  if (sendAT("AT+CPIN?", "READY", 3000)) {
    Serial.println("SIM already READY/unlocked.");
    return;
  }
  
  // If not READY, try to unlock with PIN
  if (sendAT("AT+CPIN?", "SIM PIN", 3000)) {
    Serial.println("SIM is locked. Sending PIN...");
    String pinCmd = "AT+CPIN=\"" + simPIN + "\"";
    if (sendAT(pinCmd, "OK", 5000)) {
        Serial.println("PIN accepted.");
    } else {
        Serial.println("PIN failed or timeout.");
    }
  } else {
    Serial.println("SIM status unknown or busy.");
  }
}

void sendSMS(String number, String message) {
  Serial.println("Sending SMS to " + number + "...");

  if (!sendAT("AT+CMGF=1", "OK", 3000)) return;

  String cmd = "AT+CMGS=\"" + number + "\"";
  if (!sendAT(cmd, ">", 5000)) return;

  SIM900.print(message);
  SIM900.write(26);

  waitForSMSConfirmation();
}


void beepAlert() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

void waitForSMSConfirmation() {
  long timeout = millis() + 10000;
  String resp = "";
  while (millis() < timeout) {
    while (SIM900.available()) {
      resp += char(SIM900.read());
      if (resp.indexOf("OK") != -1 || resp.indexOf("+CMGS") != -1) {
        Serial.println("SMS sent successfully!");
        return;
      }
    }
  }
  Serial.println("SMS send timeout.");
}

void initGSM() {
  Serial.println("Initializing GSM...");

  SIM900.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(2000);

  if (!sendAT("AT", "OK")) {
    Serial.println("SIM900 not responding! Check wiring or power.");
    return;
  }

  sendAT("ATE0", "OK"); // turn off echo
  sendAT("AT+CSQ", "OK"); 
  unlockSIM();

  Serial.println("Waiting for network registration...");

  const int maxAttempts = 10;
  int attempt = 0;
  while (attempt < maxAttempts) {
    if (sendAT("AT+CREG?", "+CREG: 0,1") || sendAT("AT+CREG?", "+CREG: 0,5")) {
      Serial.println("Registered to network!");
      break;
    }
    attempt++;
    Serial.print(".");
    delay(2000);
  }

  if (attempt == maxAttempts) {
    Serial.println("Network registration failed!");
  }

  sendAT("AT+CMGF=1", "OK"); // SMS text mode
  sendAT("AT+CCID", "OK");    // SIM ID
}


void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  initGSM();  // <--- call the new GSM setup

  // --- WiFi & MQTT setup ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected!");
  client.setServer(mqttServer, mqttPort);
  ph4502c.init();
}


void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32Client", accessToken, NULL)) {
      Serial.println("Connected!");
    } 
    else {
      Serial.print("Failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  // ---- Read Sensors ----
  //int rawPH = analogRead(PH_PIN);
  //int rawTemp = analogRead(TEMP_PIN);
  int rawTurb = analogRead(TURB_PIN);
  int rawEC = analogRead(EC_PIN);

  Serial.println("Temperature reading:"
        + String(ph4502c.read_temp()));
  // Read the pH level by average
    Serial.println("pH Level Reading: "
        + String(ph4502c.read_ph_level()));

  // Read a single pH level value
    Serial.println("pH Level Single Reading: "
      + String(ph4502c.read_ph_level_single()));

    // --- pH correction using voltage divider ---
int rawPH_fix = analogRead(PH4502C_PH_PIN);  // read ADC from divider
float voltagePH_fix = (rawPH_fix * (3.3 / 4095.0)) * 1.3;  // multiply by 2 because divider halves voltage
float ph_corrected = 7 + ((2.5 - voltagePH_fix) / 0.14);  // apply calibration

// --- Temperature correction using ADC reading ---
int rawTemp_fix = ph4502c.read_temp();  // read raw ADC
float voltageTemp_fix = rawTemp_fix * (3.3 / 4095.0);  // convert to voltage

// Apply linear mapping / calibration (adjust 14.6 if needed for your sensor)
float temperature_corrected = voltageTemp_fix * 14.6;  

Serial.print("Corrected Temperature (°C): ");
Serial.println(temperature_corrected, 1);

Serial.print("Corrected pH reading: ");
Serial.println(ph_corrected, 2);

  //float voltagePH = rawPH * (3.3 / 4095.0);
  //float voltageTemp = rawTemp * (3.3 / 4095.0);
  float voltageTurb = rawTurb * (3.3 / 4095.0);
  float voltageTDS = rawEC * (3.3 / 4095.0);

  //float pHValue = 7 + ((2.5 - voltagePH) / 0.6);
  //float temperature = voltageTemp * 14.6;
  float turbidityNTU = -1120.4 * voltageTurb * voltageTurb + 5742.3 * voltageTurb - 4352.9;

  float ecValue = 133.42 * voltageTDS * voltageTDS * voltageTDS
                - 255.86 * voltageTDS * voltageTDS
                + 857.39 * voltageTDS;

  float tempCoeff = 1.0 + 0.02*((ph4502c.read_temp()) - 25.0);
  float ecComp = ecValue / tempCoeff;
  float tdsValue = ecComp / 2;
  float salinity = ecComp * 0.00064;

  // Turbidity Level
  String turbidityLevel;
  if(turbidityNTU <= 2000) turbidityLevel = "Level 1: Clear";
  else if(turbidityNTU <= 2600) turbidityLevel = "Level 2: Slightly Turbid";
  else if(turbidityNTU <= 3400) turbidityLevel = "Level 3: Medium";
  else turbidityLevel = "Level 4: Highly Turbid";

  // ---- WQI ----
  float q_temp = max(0.0f, 100.0f - abs(temperature_corrected - 25) * 3.0f);
  float q_pH   = max(0.0f, 100.0f - abs(ph_corrected - 7) * 15.0f);
  float q_turb = max(0.0f, 100.0f - (turbidityNTU / 10.0f));
  float q_tds  = max(0.0f, 100.0f - (tdsValue / 10.0f));
  float q_ec   = max(0.0f, 100.0f - (ecComp / 20));
  float q_sal  = max(0.0f, 100.0f - (salinity * 100));

  float W_temp = 0.08, W_pH = 0.13, W_turb = 0.10;
  float W_tds = 0.18, W_ec = 0.25, W_sal = 0.26;

  float WQI = (q_temp*W_temp + q_pH*W_pH +  q_turb*W_turb +
               q_tds*W_tds + q_ec*W_ec + q_sal*W_sal) /
              (W_temp + W_pH +  W_turb + W_tds + W_ec + W_sal);

  String wqiStatus;
  if(WQI >= 90) wqiStatus = "Excellent";
  else if(WQI >= 70) wqiStatus = "Good";
  else if(WQI >= 50) wqiStatus = "Medium";
  else if(WQI >= 25) wqiStatus = "Poor";
  else wqiStatus = "Very Poor";

  // ---- Print Sensor Values ----
  Serial.println("--SENSOR READINGS--");
  Serial.print("pH: "); Serial.println(ph_corrected);
  Serial.print("Temperature (°C): "); Serial.println(temperature_corrected);
  Serial.print("Turbidity (NTU): "); Serial.println(turbidityNTU);
  Serial.print("Salinity (ppm): "); Serial.println(salinity);
  Serial.print("WQI: "); Serial.println(WQI);
  Serial.println("---------------------------\n");
  delay(2000);


  // -------------------------------------------------------------------
  // ✅ EDGE-TRIGGERED ALERT SECTION
  // -------------------------------------------------------------------
  String alertMsg = "";

  if(turbidityNTU > 1460) alertMsg += "Turbidity is HIGH; ";
  if(WQI < 25) alertMsg += "Water quality is VERY POOR; ";
  else if(WQI < 50) alertMsg += "Water quality is POOR; ";
  if(temperature_corrected < 18.0) alertMsg += "Temperature is BELOW 18°C; ";
  else if(temperature_corrected > 32.0) alertMsg += "Temperature is ABOVE 32°C; ";
  if(ecComp > 3000) alertMsg += "Electrical Conductivity is HIGH; ";
  if(tdsValue > 1800) alertMsg += "Total Dissolved Solids concentration is HIGH; ";
  if(salinity > 2.0) alertMsg += "Salinity is HIGH; ";
  if(ph_corrected < 6.0) alertMsg += "pH is LOW - acidic water; ";
  else if(ph_corrected > 8.5) alertMsg += "pH is HIGH - basic water; ";

  bool alertNow = (alertMsg.length() > 0);

  if (alertNow && !alertActive) {
      alertActive = true;
      beepAlert();
      sendSMS("+254715497740", alertMsg);
  }

  if (!alertNow && alertActive) {
      alertActive = false;  // reset when normal
  }


  // ---- Telemetry JSON ----
  String payload = "{";
  payload += "\"pH\":" + String(ph_corrected,2) + ",";
  payload += "\"temperature\":" + String(temperature_corrected,1) + ",";
  payload += "\"turbidity\":" + String(turbidityNTU,1) + ",";
  payload += "\"total dissolved solids\":" + String(tdsValue,1) + ",";
  payload += "\"salinity\":" + String(salinity,3) + ",";
  payload += "\"electrical conductivity\":" + String(ecComp,1) + ",";
  payload += "\"WQI\":" + String(WQI,2) + ",";
  payload += "\"status\":\"" + wqiStatus + "\",";
  payload += "\"alerts\":\"" + alertMsg + "\",";
  payload += "\"alarm\":" + String(alertNow ? "true" : "false"); // new field
  payload += "}";

  client.publish("v1/devices/me/telemetry", payload.c_str());

  delay(2000);
}
