#include <DTH_Turbidity.h>
#include <ph4502c_sensor.h>

#define PH_PIN     25   // PH4502C Po
#define TEMP_PIN   26   // PH4502C To
#define TURB_PIN   34   // SEN0189 A0
#define EC_PIN     14   // EC/TDS analog pin

void setup() {
  Serial.begin(115200);
}

void loop() {
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

  // pH sensor
  float pHValue = 7 + ((2.5 - voltagePH) / 0.6);

  // Temperature
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

  // Turbidity level
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

  // Subindex (Qn) calculations (0-100)
  float q_temp = max(0, 100 - abs(temperature - 25) * 3);
  float q_pH = max(0, 100 - abs(pHValue - 7) * 15);
  float q_turb = max(0, 100 - (turbidityNTU / 10));
  float q_tds = max(0, 100 - (tdsValue / 10));
  float q_ec = max(0, 100 - (ecComp / 20));
  float q_sal = max(0, 100 - (salinity * 100));

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

  // Print readings
  Serial.println("WATER QUALITY READINGS");
  Serial.print("pH: "); Serial.println(pHValue, 2);
  Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print("Turbidity: "); Serial.print(turbidityNTU, 1); Serial.println(" NTU");
  Serial.println(turbidityLevel);
  Serial.print("EC: "); Serial.print(ecComp, 1); Serial.println(" µS/cm");
  Serial.print("TDS: "); Serial.print(tdsValue, 1); Serial.println(" ppm");
  Serial.print("Salinity: "); Serial.print(salinity, 3); Serial.println(" ppt");

  Serial.println("");
  Serial.println("SUBINDICES");
  Serial.print("Temp: "); Serial.println(q_temp, 1);
  Serial.print("pH: "); Serial.println(q_pH, 1);
  Serial.print("Turbidity: "); Serial.println(q_turb, 1);
  Serial.print("TDS: "); Serial.println(q_tds, 1);
  Serial.print("EC: "); Serial.println(q_ec, 1);
  Serial.print("Salinity: "); Serial.println(q_sal, 1);

  Serial.println("");
  Serial.println("WATER QUALITY INDEX");
  Serial.print("WQI: "); Serial.print(WQI, 2);
  Serial.print(" -> "); Serial.println(wqiStatus);

  delay(3000);
}
