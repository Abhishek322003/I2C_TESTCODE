#include <Wire.h>
#include <Arduino.h>

#define STM8_SLAVE_ADDR 0x27   // Must match STM8 I2C address
#define SDA_PIN 9
#define SCL_PIN 8
#define I2C_READ_SIZE 64       // Bytes to request from STM8

String userCommand = "";  // Store user input

void sendCommand(const char *cmd) {
  Serial.print("\n[ESP32 → STM8] Sending: ");
  Serial.println(cmd);

  // Send command to STM8
  Wire.beginTransmission(STM8_SLAVE_ADDR);
  while (*cmd) {
    Wire.write((uint8_t)*cmd++);
  }
  uint8_t error = Wire.endTransmission();

  if (error != 0) {
    Serial.print("[I2C ERROR] Code: ");
    Serial.println(error);
    return;
  }

  delay(20); // Allow STM8 to process

  // Read STM8 response
  Wire.requestFrom(STM8_SLAVE_ADDR, I2C_READ_SIZE);
  String response = "";
  while (Wire.available()) {
    uint8_t c = Wire.read();
    if (c == 0x00) break;
    response += (char)c;
  }

  Serial.print("[STM8 → ESP32] Response: ");
  Serial.println(response);

  // ------------------------------
  // Parse RECTIFIER STATUS
  // ------------------------------
  int rectPos = response.indexOf("R:");
  if (rectPos != -1) {
    // Extract only the rectifier values after "R:"
    String rectStatus = response.substring(rectPos + 2);
    int endPos = rectStatus.indexOf('|');
    if (endPos != -1) rectStatus = rectStatus.substring(0, endPos);
    rectStatus.trim();

    // Add space after every 4 bits
    String formattedRect = "";
    for (int i = 0; i < rectStatus.length(); i++) {
      formattedRect += rectStatus[i];
      if ((i + 1) % 4 == 0 && i != rectStatus.length() - 1)
        formattedRect += ' ';
    }

    Serial.print("[RECTIFIER STATUS] ");
    Serial.println(formattedRect);
  } else {
    Serial.println("[RECTIFIER STATUS] Not found in response");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println("\n===============================");
  Serial.println(" ESP32 I2C Master Ready");
  Serial.println(" Type commands like:");
  Serial.println("   relaycon");
  Serial.println("   relaydoff");
  Serial.println("   rgb1on");
  Serial.println("   allon");
  Serial.println("   alloff");
  Serial.println("   readrect");
  Serial.println("===============================");
}

void loop() {
  if (Serial.available()) {
    userCommand = Serial.readStringUntil('\n');
    userCommand.trim();
    userCommand.toLowerCase();
    userCommand.replace(" ", "");

    if (userCommand.length() > 0) {
      sendCommand(userCommand.c_str());
    }

    Serial.println("\nEnter next command:");
  }
}
