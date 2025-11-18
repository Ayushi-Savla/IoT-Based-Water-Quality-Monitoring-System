#include <PubSubClient.h>

#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>

#include <DTH_Turbidity.h>
#include <ph4502c_sensor.h>

const char* ssid = "";
const char* password = "";
const char* mqttServer = "mqtt.thingsboard.cloud";  // or your ThingsBoard IP
const int mqttPort = 1883;
const char* accessToken = "";

WiFiClient espClient;
PubSubClient client(espClient);

#define PH_PIN     25   // PH4502C Po
#define TEMP_PIN   26   // PH4502C To
#define TURB_PIN   34   // SEN0189 A0
#define EC_PIN     14   // EC/TDS analog pin

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Connect to ThingsBoard MQTT
  client.setServer(mqttServer, mqttPort);
  connectMQTT();
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard MQTT...");
    if (client.connect("ESP32_Device", accessToken, NULL)) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();
  
  // Read sensor values
  int rawPH = analogRead(PH_PIN);
  int rawTemp = analogRead(TEMP_PIN);
  int rawTurb = analogRead(TURB_PIN);
  int rawEC = analogRead(EC_PIN);

  // Convert to voltage (0–3.3V)
  float voltagePH = rawPH * (3.3 / 4095.0);
  float voltageTemp = rawTemp * (3.3 / 4095.0);
  float voltageTurb = rawTurb * (3.3 / 4095.0);
  float voltageTDS = rawEC * (3.3 / 4095.0);

  // pH sensor conversion
  float pHValue = 7 + ((2.5 - voltagePH) / 0.8);

  // Temperature (approximation)
  float temperature = (voltageTemp * 14.8);

  // Turbidity (NTU)
  float turbidityNTU = -1120.4 * voltageTurb * voltageTurb 
                        + 5742.3 * voltageTurb 
                        - 4352.9;

  // EC and TDS calculations
  float ecValue = (133.42 * voltageTDS * voltageTDS * voltageTDS)
                - (255.86 * voltageTDS * voltageTDS)
                + (857.39 * voltageTDS);  // µS/cm

  float tempCoeff = 1.0 + 0.02 * (temperature - 25.0);
  float ecComp = ecValue / tempCoeff;
  float tdsValue = ecComp / 2;
  float salinity = ecComp * 0.00064;

  // Turbidity level description
  String turbidityLevel;
  if (voltageTurb >= 2.96) {
    turbidityLevel = "Level 1: Clear Water";
  } else if (voltageTurb >= 2.64) {
    turbidityLevel = "Level 2: Slightly Turbid";
  } else if (voltageTurb >= 1.84) {
    turbidityLevel = "Level 3: Medium Turbidity";
  } else {
    turbidityLevel = "Level 4: Highly Turbid";
  }

  // Subindex (Qn) calculations (0–100)
  float q_temp = max(0.0f, 100.0f - abs(temperature - 25) * 3.0f);
  float q_pH = max(0.0f, 100.0f - abs(pHValue - 7) * 15.0f);
  float q_turb = max(0.0f, 100.0f - (turbidityNTU / 10.0f));
  float q_tds = max(0.0f, 100.0f - (tdsValue / 10.0f));
  float q_ec = max(0.0f, 100.0f - (ecComp / 20));
  float q_sal = max(0.0f, 100.0f - (salinity * 100));

  // Weightage per parameter
  float W_temp = 0.08;
  float W_pH = 0.13;
  float W_turb = 0.10;
  float W_tds = 0.18;
  float W_ec = 0.25;
  float W_sal = 0.26;

  // WQI calculation
  float numerator = (q_temp * W_temp) + (q_pH * W_pH) + (q_turb * W_turb)
                  + (q_tds * W_tds) + (q_ec * W_ec) + (q_sal * W_sal);
  float denominator = W_temp + W_pH + W_turb + W_tds + W_ec + W_sal;
  float WQI = numerator / denominator;

  String wqiStatus;
  if (WQI >= 90) wqiStatus = "Excellent";
  else if (WQI >= 70) wqiStatus = "Good";
  else if (WQI >= 50) wqiStatus = "Medium";
  else if (WQI >= 25) wqiStatus = "Poor";
  else wqiStatus = "Very Poor";

  // Print readings to Serial Monitor
  Serial.println("WATER QUALITY READINGS");
  Serial.print("pH: "); Serial.println(pHValue, 2);
  Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print("Turbidity: "); Serial.print(turbidityNTU, 1); Serial.println(" NTU");
  Serial.println(turbidityLevel);
  Serial.print("EC: "); Serial.print(ecComp, 1); Serial.println(" µS/cm");
  Serial.print("TDS: "); Serial.print(tdsValue, 1); Serial.println(" ppm");
  Serial.print("Salinity: "); Serial.print(salinity, 3); Serial.println(" ppt");

  Serial.println("\nWATER QUALITY INDEX");
  Serial.print("WQI: "); Serial.print(WQI, 2);
  Serial.print(" -> "); Serial.println(wqiStatus);
  Serial.println("\n");

  // ----- Send data to ThingsBoard -----
  String payload = "{";
  payload += "\"pH\":" + String(pHValue, 2) + ",";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"turbidity\":" + String(turbidityNTU, 1) + ",";
  payload += "\"tds\":" + String(tdsValue, 1) + ",";
  payload += "\"salinity\":" + String(salinity, 3) + ",";
  payload += "\"ec\":" + String(ecComp, 1) + ",";
  payload += "\"WQI\":" + String(WQI, 2) + ",";
  payload += "\"status\":\"" + wqiStatus + "\"";
  payload += "}";

  client.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.println("Data sent to ThingsBoard!");

  delay(10000);
}
