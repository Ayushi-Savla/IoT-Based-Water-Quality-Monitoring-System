#define PH_PIN 25  // PH4502C output pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting pH sensor test...");
}

void loop() {
  int rawPH = analogRead(PH_PIN);
  float voltage = rawPH * (3.3 / 4095.0);        // Convert ADC reading to voltage
  float pHValue = 7 + ((2.5 - voltage) / 0.8);   // Convert voltage to pH value

  Serial.print("Raw ADC: ");
  Serial.print(rawPH);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | pH: ");
  Serial.println(pHValue, 2);

  delay(3000);  // Read every second
}
