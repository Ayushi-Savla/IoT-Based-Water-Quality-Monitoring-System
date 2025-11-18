#define TURB_PIN 34  // Turbidity sensor analog output pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting turbidity sensor test...");
}

void loop() {
  int rawTurb = analogRead(TURB_PIN);
  float voltage = rawTurb * (3.3 / 4095.0);  // Convert ADC reading to voltage

  // Convert voltage to NTU (approximation from DFRobot calibration curve)
  float turbidityNTU = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;

//  // Prevent negative readings
//  if (turbidityNTU < 0) turbidityNTU = 0;

  Serial.print("Raw ADC: ");
  Serial.print(rawTurb);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | Turbidity: ");
  Serial.print(turbidityNTU, 1);
  Serial.println(" NTU");

  delay(3000);  // Read every 3 seconds
}
