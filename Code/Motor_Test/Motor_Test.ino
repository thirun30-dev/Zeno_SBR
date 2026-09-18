/*
  ZENO - ESP32 + TB6612FNG Motor Test

  Serial Monitor:
    Baud rate: 115200
    Line ending: Newline or Both NL & CR

  Commands:
    F or FORWARD  -> Both wheels forward
    B or BACKWARD -> Both wheels backward
    S or STOP     -> Stop motors
    H or HELP     -> Show commands

  Motors automatically stop after 2 seconds.
  No additional libraries required.
*/

#include <Arduino.h>

constexpr uint8_t AIN1 = 25;
constexpr uint8_t AIN2 = 26;
constexpr uint8_t PWMA = 27;

constexpr uint8_t BIN1 = 32;
constexpr uint8_t BIN2 = 33;
constexpr uint8_t PWMB = 14;

constexpr uint8_t STBY = 13;

// Mirrored motors often require opposite electrical directions.
// Change the corresponding value if a wheel rotates the wrong way.
constexpr bool LEFT_REVERSED  = false;
constexpr bool RIGHT_REVERSED = true;

constexpr unsigned long RUN_TIME_MS = 2000;

bool motorsRunning = false;
unsigned long motorStartTime = 0;

char commandBuffer[32];
size_t commandLength = 0;
bool commandOverflow = false;

void stopMotors() {
  digitalWrite(STBY, LOW);

  digitalWrite(PWMA, LOW);
  digitalWrite(PWMB, LOW);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);

  motorsRunning = false;
}

void setDirection(uint8_t in1, uint8_t in2,
                  bool forward, bool reversed) {
  bool direction = forward != reversed;

  digitalWrite(in1, direction ? HIGH : LOW);
  digitalWrite(in2, direction ? LOW : HIGH);
}

void runMotors(bool forward) {
  // Disable the driver before changing direction.
  stopMotors();
  delay(20);

  setDirection(AIN1, AIN2, forward, LEFT_REVERSED);
  setDirection(BIN1, BIN2, forward, RIGHT_REVERSED);

  // Constant HIGH enables full output; this test does not use PWM.
  digitalWrite(PWMA, HIGH);
  digitalWrite(PWMB, HIGH);
  digitalWrite(STBY, HIGH);

  motorStartTime = millis();
  motorsRunning = true;

  Serial.println(forward
    ? "FORWARD: running for 2 seconds."
    : "BACKWARD: running for 2 seconds.");
}

void printHelp() {
  Serial.println();
  Serial.println("=== ZENO MOTOR TEST ===");
  Serial.println("F / FORWARD  : Move forward");
  Serial.println("B / BACKWARD : Move backward");
  Serial.println("S / STOP     : Stop");
  Serial.println("H / HELP     : Show commands");
  Serial.println("Automatic stop: 2 seconds");
  Serial.println("Use Newline or Both NL & CR.");
  Serial.println();
}

void processCommand() {
  commandBuffer[commandLength] = '\0';

  String command(commandBuffer);
  command.trim();
  command.toUpperCase();

  if (command == "F" || command == "FORWARD") {
    runMotors(true);
  } else if (command == "B" || command == "BACKWARD") {
    runMotors(false);
  } else if (command == "S" || command == "STOP") {
    stopMotors();
    Serial.println("STOPPED.");
  } else if (command == "H" || command == "HELP") {
    printHelp();
  } else if (command.length() > 0) {
    stopMotors();
    Serial.println("Unknown command. Motors stopped. Enter F, B or S.");
  }
}

void setup() {
  // Keep the driver disabled during initialization.
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);

  stopMotors();

  Serial.begin(115200);
  printHelp();
}

void loop() {
  if (motorsRunning &&
      millis() - motorStartTime >= RUN_TIME_MS) {
    stopMotors();
    Serial.println("AUTO STOP: 2-second test completed.");
  }

  // Read without blocking the automatic stop timer.
  if (Serial.available() > 0) {
    char received = Serial.read();

    if (received == '\n' || received == '\r') {
      if (commandOverflow) {
        Serial.println("Command too long. Motors stopped.");
      } else if (commandLength > 0) {
        processCommand();
      }

      commandLength = 0;
      commandOverflow = false;
    } else if (!commandOverflow) {
      if (commandLength < sizeof(commandBuffer) - 1) {
        commandBuffer[commandLength++] = received;
      } else {
        stopMotors();
        commandOverflow = true;
      }
    }
  }
}