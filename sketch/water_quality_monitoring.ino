#include <DTH_Turbidity.h>
#include <ph4502c_sensor.h>

#define PH_PIN     25   // Po from PH4502C
#define TEMP_PIN   26   // To from PH4502C
#define TURB_PIN   34   // Analog pin connected to SEN0189 A0

void setup() {
  Serial.begin(115200);
}

void loop() {
  // Read raw values
  int rawPH   = analogRead(PH_PIN);
  int rawTemp = analogRead(TEMP_PIN);
  int rawTurb = analogRead(TURB_PIN);

  // Convert to voltages (ESP32 ADC: 0–4095 = 0–3.3V)
  float voltagePH   = rawPH   * (3.3 / 4095.0);
  float voltageTemp = rawTemp * (3.3 / 4095.0);
  float voltageTurb = rawTurb * (3.3 / 4095.0);

  // pH calculation
  float pHValue = 7 + ((2.5 - voltagePH) / 0.6);

  // Temperature calculation
  float temperature = (voltageTemp * 14.8); // example scaling

  // Turbidity calculation (simple model, calibration required)
  // Clear water ≈ 4.2V, muddy water ≈ < 2.5V
  float turbidityNTU = -1120.4 * voltageTurb * voltageTurb 
                        + 5742.3 * voltageTurb 
                        - 4352.9;

  // Print values
  Serial.println("WATER QUALITY READINGS");
  
  Serial.print("Raw pH ADC: "); Serial.println(rawPH);
  Serial.print("pH Voltage: "); Serial.print(voltagePH, 2); Serial.println(" V");
  Serial.print("Estimated pH: "); Serial.println(pHValue, 2);

  Serial.print("Temp Voltage: "); Serial.print(voltageTemp, 2); Serial.println(" V");
  Serial.print("Estimated Temp: "); Serial.print(temperature, 1); Serial.println(" °C");

  Serial.print("Raw Turbidity ADC: "); Serial.println(rawTurb);
  Serial.print("Turbidity Voltage: "); Serial.print(voltageTurb, 2); Serial.println(" V");
  Serial.print("Estimated Turbidity: "); Serial.print(turbidityNTU, 1); Serial.println(" NTU");

  // Optional Warnings
  if (temperature <= 20) {
    Serial.println("Water temperature too low");
  } else if (temperature >= 35) {
    Serial.println("Water temperature too high");
  }

  if (turbidityNTU > 50) {
    Serial.println("Water turbidity too high (not clean)");
  }

  Serial.println();
  delay(2000);
}
