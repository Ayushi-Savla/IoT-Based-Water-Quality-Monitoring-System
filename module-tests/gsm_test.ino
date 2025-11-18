#include <HardwareSerial.h>

HardwareSerial SIM(2); // UART2 for SIM900
const int RX_PIN = 16;
const int TX_PIN = 17;

const char* simPIN = "5065";                // <-- replace with your SIM PIN
const char* recipientNumber = "+254715497740"; // <-- replace with destination number
const char* messageBody = "Hello from ESP32 + SIM900!";

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n==== SIM900 SMS Sender ====");

  SIM.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(200);

  // Basic AT check
  sendCommandAndPrint("AT", 2000);

  // Enter SIM PIN if needed
  sendCommandAndPrint(String("AT+CPIN=\"") + simPIN + "\"", 2000);

  // Wait for SIM ready and network registration
  if (!waitForSIMReady(60000)) {
    Serial.println("SIM not ready or not registered. Aborting.");
    while (1) { delay(1000); } // stop execution
  }

  // Set SMS to text mode
  sendCommandAndPrint("AT+CMGF=1", 1000);

  // Send SMS
  sendSMS(recipientNumber, messageBody);
}

void loop() {
  // Print any further responses from modem
  while (SIM.available()) Serial.write(SIM.read());
  while (Serial.available()) SIM.write(Serial.read());
  delay(10);
}

// ---------- helper functions ----------
void sendCommandAndPrint(String cmd, unsigned long timeoutMs) {
  Serial.print("> "); Serial.println(cmd);
  SIM.println(cmd);
  readResponse(timeoutMs);
}

void readResponse(unsigned long timeoutMs) {
  unsigned long start = millis();
  String resp = "";
  while (millis() - start < timeoutMs) {
    while (SIM.available()) {
      char c = (char)SIM.read();
      resp += c;
    }
  }
  if (resp.length()) Serial.print(resp);
}

bool waitForSIMReady(unsigned long overallTimeoutMs) {
  unsigned long start = millis();
  Serial.println("Waiting for SIM ready and network registration...");
  while (millis() - start < overallTimeoutMs) {
    // Check CPIN
    SIM.println("AT+CPIN?");
    delay(800);
    String r = readAvailableString(800);
    if (r.indexOf("READY") >= 0) {
      // Check registration
      SIM.println("AT+CREG?");
      delay(800);
      String r2 = readAvailableString(800);
      if ((r2.indexOf("0,1") >= 0) || (r2.indexOf("0,5") >= 0)) {
        Serial.println("SIM ready and registered.");
        return true;
      } else {
        Serial.print("CREG not ready yet: "); Serial.println(r2.length()? r2 : "<no reply>");
      }
    } else {
      Serial.print("CPIN not ready yet: "); Serial.println(r.length()? r : "<no reply>");
    }
    delay(2000);
  }
  return false;
}

String readAvailableString(unsigned long timeoutMs) {
  unsigned long start = millis();
  String s = "";
  while (millis() - start < timeoutMs) {
    while (SIM.available()) s += (char)SIM.read();
  }
  return s;
}

void sendSMS(const char* number, const char* text) {
  Serial.print("Sending SMS to "); Serial.println(number);
  // Ensure text mode
  sendCommandAndPrint("AT+CMGF=1", 1000);

  // Start message
  String cmd = String("AT+CMGS=\"") + number + "\"";
  SIM.println(cmd);

  // Wait for '>' prompt
  unsigned long t0 = millis();
  bool prompt = false;
  String resp = "";
  while (millis() - t0 < 5000) {
    while (SIM.available()) {
      char c = (char)SIM.read();
      resp += c;
      if (c == '>') { prompt = true; break; }
    }
    if (prompt) break;
  }
  Serial.print(resp);
  if (!prompt) {
    Serial.println("No prompt '>' received — aborting SMS.");
    return;
  }

  // Send message text
  SIM.print(text);
  delay(300);
  // Send Ctrl+Z
  SIM.write(26);
  Serial.println("\nMessage body sent, waiting for modem response...");

  // Wait for final response (+CMGS and OK)
  readResponse(10000);
  Serial.println("SMS send attempt finished.");
}
