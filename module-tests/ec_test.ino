#define EC_PIN 34  // EC/TDS sensor analog output pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting EC, TDS, and Salinity sensor test...");
}

void loop() {
  int rawEC = analogRead(EC_PIN);
  float voltage = rawEC * (3.3 / 4095.0);  // Convert ADC reading to voltage

  // --- EC Calculation (µS/cm) ---
  float ecValue = (133.42 * voltage * voltage * voltage)
                - (255.86 * voltage * voltage)
                + (857.39 * voltage);  // µS/cm

  // --- TDS and Salinity ---
  float tdsValue = ecValue / 2.0;      // ppm
  float salinity = ecValue * 0.00064;  // ppt

  if (ecValue < 0) ecValue = 0;
  if (tdsValue < 0) tdsValue = 0;
  if (salinity < 0) salinity = 0;

  Serial.print("Raw ADC: ");
  Serial.print(rawEC);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | EC: ");
  Serial.print(ecValue, 1);
  Serial.print(" µS/cm | TDS: ");
  Serial.print(tdsValue, 1);
  Serial.print(" ppm | Salinity: ");
  Serial.print(salinity, 3);
  Serial.println(" ppt");

  delay(3000);
}
