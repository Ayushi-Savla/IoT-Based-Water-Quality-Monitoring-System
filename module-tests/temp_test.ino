#define TEMP_PIN 34  // Temperature sensor analog output pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting temperature sensor test...");
}

void loop() {
  int rawTemp = analogRead(TEMP_PIN);
  float voltage = rawTemp * (3.3 / 4095.0);   // Convert ADC reading to voltage
  float temperature = (voltage * 14.8);         // Approximate °C conversion (adjust as needed)

  Serial.print("Raw ADC: ");
  Serial.print(rawTemp);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" °C");

  delay(3000);  // Read every 3 seconds
}
