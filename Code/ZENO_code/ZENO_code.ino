/*
  ================================================================
                  ZENO SELF-BALANCING ROBOT
  ================================================================

  Controller : ESP32 DevKit V1
  IMU        : MPU9250
  Driver     : TB6612FNG
  Motors     : 2x N20 3V 500RPM Encoder Motors

  ---------------------- PIN CONNECTIONS ---------------------------

  MPU9250
  SDA  -> GPIO 22
  SCL  -> GPIO 23

  TB6612FNG
  PWMA -> GPIO 25
  AIN1 -> GPIO 26
  AIN2 -> GPIO 27
  STBY -> GPIO 32

  BIN1 -> GPIO 14
  BIN2 -> GPIO 13
  PWMB -> GPIO 33

  Encoder currently configured
  Encoder A -> GPIO 19
  Encoder B -> GPIO 18

  ---------------------- SERIAL COMMANDS ---------------------------

  C  -> Calibrate IMU
  B  -> Enable balancing
  S  -> Stop balancing
  F  -> Move forward
  R  -> Move backward
  L  -> Turn left
  T  -> Turn right
  X  -> Stop movement
  P  -> Print PID values
  I  -> Increase Kp
  i  -> Decrease Kp
  O  -> Increase Ki
  o  -> Decrease Ki
  D  -> Increase Kd
  d  -> Decrease Kd

  ================================================================
*/

#include <Wire.h>

// ================================================================
// MPU9250
// ================================================================

#define MPU_ADDR 0x68

#define MPU_PWR_MGMT_1   0x6B
#define MPU_SMPLRT_DIV   0x19
#define MPU_CONFIG       0x1A
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_ACCEL_XOUT_H 0x3B

// I2C
#define SDA_PIN 22
#define SCL_PIN 23


// ================================================================
// TB6612FNG
// ================================================================

// Motor A
#define PWMA 25
#define AIN1 26
#define AIN2 27

// Motor B
#define PWMB 33
#define BIN1 14
#define BIN2 13

#define STBY 32


// ================================================================
// ENCODER
// ================================================================

#define ENC_A 19
#define ENC_B 18

volatile long encoderCount = 0;


// ================================================================
// IMU VARIABLES
// ================================================================

float accX = 0;
float accY = 0;
float accZ = 0;

float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

float gyroOffsetX = 0;
float gyroOffsetY = 0;
float gyroOffsetZ = 0;

float angleOffset = 0;

float pitchAngle = 0;
float accelAngle = 0;


// ================================================================
// BALANCING PID
// ================================================================

// STARTING VALUES ONLY.
// These MUST be tuned for your actual ZENO mechanical design.

float Kp = 22.0;
float Ki = 0.0;
float Kd = 0.85;

float targetAngle = 0.0;

float error = 0;
float previousError = 0;
float integral = 0;
float derivative = 0;

float pidOutput = 0;


// ================================================================
// CONTROL VARIABLES
// ================================================================

bool calibrated = false;
bool balanceEnabled = false;

int moveCommand = 0;
int turnCommand = 0;

const int MAX_PWM = 255;

float forwardOffset = 1.5;
float turnPWM = 35;


// ================================================================
// SAFETY
// ================================================================

// If the robot falls beyond this angle motors stop.

const float FALL_ANGLE = 40.0;


// ================================================================
// FILTER
// ================================================================

float complementaryAlpha = 0.98;


// ================================================================
// TIMING
// ================================================================

unsigned long previousMicros = 0;
unsigned long serialTimer = 0;

float dt = 0;


// ================================================================
// MPU9250 FUNCTIONS
// ================================================================

void writeMPU(byte reg, byte data)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}


// ----------------------------------------------------------------

bool testMPU()
{
  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() == 0)
  {
    return true;
  }

  return false;
}


// ----------------------------------------------------------------

void initializeMPU()
{
  Serial.println();
  Serial.println("Initializing MPU9250...");

  // Wake MPU9250
  writeMPU(MPU_PWR_MGMT_1, 0x00);
  delay(100);

  // Clock source
  writeMPU(MPU_PWR_MGMT_1, 0x01);
  delay(10);

  // Sample rate
  writeMPU(MPU_SMPLRT_DIV, 0x04);

  // Low pass filter
  writeMPU(MPU_CONFIG, 0x03);

  // Gyroscope ±250 deg/sec
  writeMPU(MPU_GYRO_CONFIG, 0x00);

  // Accelerometer ±2g
  writeMPU(MPU_ACCEL_CONFIG, 0x00);

  delay(100);

  Serial.println("MPU9250 configuration complete.");
}


// ----------------------------------------------------------------

bool readMPU()
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(MPU_ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }

  int received = Wire.requestFrom(
    (uint8_t)MPU_ADDR,
    (uint8_t)14,
    true
  );

  if (received != 14)
  {
    return false;
  }

  int16_t rawAX =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawAY =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawAZ =
      ((int16_t)Wire.read() << 8) | Wire.read();


  // Temperature bytes
  Wire.read();
  Wire.read();


  int16_t rawGX =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawGY =
      ((int16_t)Wire.read() << 8) | Wire.read();

  int16_t rawGZ =
      ((int16_t)Wire.read() << 8) | Wire.read();


  // Accelerometer ±2g
  accX = rawAX / 16384.0;
  accY = rawAY / 16384.0;
  accZ = rawAZ / 16384.0;


  // Gyroscope ±250 dps
  gyroX = rawGX / 131.0;
  gyroY = rawGY / 131.0;
  gyroZ = rawGZ / 131.0;


  gyroX -= gyroOffsetX;
  gyroY -= gyroOffsetY;
  gyroZ -= gyroOffsetZ;

  return true;
}


// ================================================================
// IMU CALIBRATION
// ================================================================

void calibrateIMU()
{
  Serial.println();
  Serial.println("================================");
  Serial.println("       ZENO CALIBRATION");
  Serial.println("================================");

  Serial.println("Keep ZENO perfectly stationary.");
  Serial.println("Keep robot in balancing position.");
  Serial.println();

  stopMotors();

  balanceEnabled = false;
  calibrated = false;

  gyroOffsetX = 0;
  gyroOffsetY = 0;
  gyroOffsetZ = 0;

  angleOffset = 0;

  delay(2000);

  const int samples = 1500;

  double sumGX = 0;
  double sumGY = 0;
  double sumGZ = 0;

  double sumAngle = 0;

  Serial.println("Calibrating...");


  for (int i = 0; i < samples; i++)
  {
    // Read RAW values without previously calculated offsets

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(MPU_ACCEL_XOUT_H);
    Wire.endTransmission(false);

    Wire.requestFrom(
      (uint8_t)MPU_ADDR,
      (uint8_t)14,
      true
    );

    int16_t rawAX =
        ((int16_t)Wire.read() << 8) | Wire.read();

    int16_t rawAY =
        ((int16_t)Wire.read() << 8) | Wire.read();

    int16_t rawAZ =
        ((int16_t)Wire.read() << 8) | Wire.read();

    Wire.read();
    Wire.read();

    int16_t rawGX =
        ((int16_t)Wire.read() << 8) | Wire.read();

    int16_t rawGY =
        ((int16_t)Wire.read() << 8) | Wire.read();

    int16_t rawGZ =
        ((int16_t)Wire.read() << 8) | Wire.read();


    float ax = rawAX / 16384.0;
    float ay = rawAY / 16384.0;
    float az = rawAZ / 16384.0;

    float gx = rawGX / 131.0;
    float gy = rawGY / 131.0;
    float gz = rawGZ / 131.0;


    /*
       ZENO pitch calculation.

       If your MPU9250 orientation produces the wrong axis,
       we can change this calculation according to your
       physical sensor mounting.
    */

    float angle =
        atan2(
          ax,
          sqrt((ay * ay) + (az * az))
        ) * 180.0 / PI;


    sumGX += gx;
    sumGY += gy;
    sumGZ += gz;

    sumAngle += angle;

    if (i % 150 == 0)
    {
      Serial.print(".");
    }

    delay(2);
  }


  gyroOffsetX = sumGX / samples;
  gyroOffsetY = sumGY / samples;
  gyroOffsetZ = sumGZ / samples;

  angleOffset = sumAngle / samples;


  pitchAngle = 0;


  Serial.println();
  Serial.println();
  Serial.println("Calibration complete.");

  Serial.print("Gyro X Offset: ");
  Serial.println(gyroOffsetX, 4);

  Serial.print("Gyro Y Offset: ");
  Serial.println(gyroOffsetY, 4);

  Serial.print("Gyro Z Offset: ");
  Serial.println(gyroOffsetZ, 4);

  Serial.print("Balance Angle Offset: ");
  Serial.println(angleOffset, 4);

  Serial.println();

  calibrated = true;

  integral = 0;
  previousError = 0;

  previousMicros = micros();

  Serial.println("Press B to start balancing.");
  Serial.println("================================");
}


// ================================================================
// ANGLE CALCULATION
// ================================================================

void calculateAngle()
{
  unsigned long currentMicros = micros();

  dt = (currentMicros - previousMicros) / 1000000.0;

  previousMicros = currentMicros;


  if (dt <= 0 || dt > 0.1)
  {
    dt = 0.005;
  }


  accelAngle =
      atan2(
        accX,
        sqrt((accY * accY) + (accZ * accZ))
      ) * 180.0 / PI;


  // Remove calibration position

  accelAngle -= angleOffset;


  /*
     IMPORTANT

     Current code assumes Gyro Y is the rotation axis.

     If the angle runs in the opposite direction,
     change:

        gyroY

     to:

        -gyroY
  */

  pitchAngle =
      complementaryAlpha *
      (pitchAngle + gyroY * dt)
      +
      (1.0 - complementaryAlpha) *
      accelAngle;
}


// ================================================================
// PID
// ================================================================

void calculatePID()
{
  float effectiveTarget = targetAngle;


  // Forward/backward movement

  if (moveCommand == 1)
  {
    effectiveTarget += forwardOffset;
  }
  else if (moveCommand == -1)
  {
    effectiveTarget -= forwardOffset;
  }


  error = effectiveTarget - pitchAngle;


  integral += error * dt;


  // Anti-windup

  integral = constrain(
    integral,
    -50.0,
    50.0
  );


  derivative =
      (error - previousError) / dt;


  pidOutput =
      (Kp * error) +
      (Ki * integral) +
      (Kd * derivative);


  previousError = error;


  pidOutput = constrain(
    pidOutput,
    -MAX_PWM,
    MAX_PWM
  );
}


// ================================================================
// MOTOR CONTROL
// ================================================================

void motorA(int speedValue)
{
  speedValue = constrain(
    speedValue,
    -255,
    255
  );


  if (speedValue > 0)
  {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    analogWrite(
      PWMA,
      speedValue
    );
  }

  else if (speedValue < 0)
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

    analogWrite(
      PWMA,
      -speedValue
    );
  }

  else
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

    analogWrite(
      PWMA,
      0
    );
  }
}


// ----------------------------------------------------------------

void motorB(int speedValue)
{
  speedValue = constrain(
    speedValue,
    -255,
    255
  );


  if (speedValue > 0)
  {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);

    analogWrite(
      PWMB,
      speedValue
    );
  }

  else if (speedValue < 0)
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);

    analogWrite(
      PWMB,
      -speedValue
    );
  }

  else
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);

    analogWrite(
      PWMB,
      0
    );
  }
}


// ----------------------------------------------------------------

void driveMotors(float balancePWM)
{
  int leftPWM = balancePWM;
  int rightPWM = balancePWM;


  // Turning

  leftPWM += turnCommand;
  rightPWM -= turnCommand;


  leftPWM = constrain(
    leftPWM,
    -255,
    255
  );

  rightPWM = constrain(
    rightPWM,
    -255,
    255
  );


  /*
     Motors are mounted opposite each other mechanically.

     Therefore one motor normally requires reverse polarity.

     Motor A = normal
     Motor B = reversed
  */

  motorA(leftPWM);

  motorB(-rightPWM);
}


// ----------------------------------------------------------------

void stopMotors()
{
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
}


// ================================================================
// ENCODER
// ================================================================

void IRAM_ATTR encoderISR()
{
  if (digitalRead(ENC_A) == digitalRead(ENC_B))
  {
    encoderCount++;
  }
  else
  {
    encoderCount--;
  }
}


// ================================================================
// SERIAL COMMANDS
// ================================================================

void processSerial()
{
  if (!Serial.available())
  {
    return;
  }


  char command = Serial.read();


  switch (command)
  {

    // ------------------------------------------------------------
    // CALIBRATION
    // ------------------------------------------------------------

    case 'C':
    case 'c':

      calibrateIMU();

      break;


    // ------------------------------------------------------------
    // START BALANCE
    // ------------------------------------------------------------

    case 'B':
    case 'b':

      if (calibrated)
      {
        balanceEnabled = true;

        integral = 0;
        previousError = 0;

        Serial.println("BALANCING ENABLED");
      }
      else
      {
        Serial.println(
          "Calibrate first using C."
        );
      }

      break;


    // ------------------------------------------------------------
    // STOP BALANCE
    // ------------------------------------------------------------

    case 'S':
    case 's':

      balanceEnabled = false;

      moveCommand = 0;
      turnCommand = 0;

      stopMotors();

      Serial.println(
        "BALANCING STOPPED"
      );

      break;


    // ------------------------------------------------------------
    // FORWARD
    // ------------------------------------------------------------

    case 'F':
    case 'f':

      moveCommand = 1;

      Serial.println("FORWARD");

      break;


    // ------------------------------------------------------------
    // REVERSE
    // ------------------------------------------------------------

    case 'R':
    case 'r':

      moveCommand = -1;

      Serial.println("REVERSE");

      break;


    // ------------------------------------------------------------
    // LEFT
    // ------------------------------------------------------------

    case 'L':
    case 'l':

      turnCommand = turnPWM;

      Serial.println("LEFT");

      break;


    // ------------------------------------------------------------
    // RIGHT
    // ------------------------------------------------------------

    case 'T':
    case 't':

      turnCommand = -turnPWM;

      Serial.println("RIGHT");

      break;


    // ------------------------------------------------------------
    // STOP MOVEMENT
    // ------------------------------------------------------------

    case 'X':
    case 'x':

      moveCommand = 0;
      turnCommand = 0;

      Serial.println(
        "MOVEMENT COMMAND CLEARED"
      );

      break;


    // ------------------------------------------------------------
    // PID SETTINGS
    // ------------------------------------------------------------

    case 'I':

      Kp += 0.5;

      printPID();

      break;


    case 'i':

      Kp -= 0.5;

      printPID();

      break;


    case 'O':

      Ki += 0.01;

      printPID();

      break;


    case 'o':

      Ki -= 0.01;

      if (Ki < 0)
        Ki = 0;

      printPID();

      break;


    case 'D':

      Kd += 0.05;

      printPID();

      break;


    case 'd':

      Kd -= 0.05;

      if (Kd < 0)
        Kd = 0;

      printPID();

      break;


    case 'P':
    case 'p':

      printPID();

      break;
  }
}


// ================================================================
// PRINT PID
// ================================================================

void printPID()
{
  Serial.println();
  Serial.println("----- PID VALUES -----");

  Serial.print("Kp = ");
  Serial.println(Kp, 3);

  Serial.print("Ki = ");
  Serial.println(Ki, 3);

  Serial.print("Kd = ");
  Serial.println(Kd, 3);

  Serial.println("----------------------");
}


// ================================================================
// DEBUG INFORMATION
// ================================================================

void printDebug()
{
  if (millis() - serialTimer < 100)
  {
    return;
  }

  serialTimer = millis();


  Serial.print("Angle: ");

  Serial.print(
    pitchAngle,
    2
  );


  Serial.print(" | AccAngle: ");

  Serial.print(
    accelAngle,
    2
  );


  Serial.print(" | PID: ");

  Serial.print(
    pidOutput,
    1
  );


  Serial.print(" | Enc: ");

  Serial.print(
    encoderCount
  );


  Serial.print(" | AX: ");

  Serial.print(
    accX,
    3
  );


  Serial.print(" AY: ");

  Serial.print(
    accY,
    3
  );


  Serial.print(" AZ: ");

  Serial.print(
    accZ,
    3
  );


  Serial.print(" | GY: ");

  Serial.print(
    gyroY,
    2
  );


  Serial.print(" | Balance: ");

  if (balanceEnabled)
  {
    Serial.println("ON");
  }
  else
  {
    Serial.println("OFF");
  }
}


// ================================================================
// COMMAND HELP
// ================================================================

void printCommands()
{
  Serial.println();
  Serial.println(
    "=============================================="
  );

  Serial.println(
    "          ZENO SERIAL COMMANDS"
  );

  Serial.println(
    "=============================================="
  );

  Serial.println("C = Calibrate");
  Serial.println("B = Enable balancing");
  Serial.println("S = Disable balancing");

  Serial.println();

  Serial.println("F = Forward");
  Serial.println("R = Reverse");
  Serial.println("L = Left");
  Serial.println("T = Right");
  Serial.println("X = Stop movement");

  Serial.println();

  Serial.println("I/i = Kp +/-");
  Serial.println("O/o = Ki +/-");
  Serial.println("D/d = Kd +/-");

  Serial.println("P = Print PID");

  Serial.println(
    "=============================================="
  );

  Serial.println();
}


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);

  delay(1500);


  Serial.println();
  Serial.println();
  Serial.println(
    "=============================================="
  );

  Serial.println(
    "             ZENO ROBOT"
  );

  Serial.println(
    "       SELF BALANCING CONTROLLER"
  );

  Serial.println(
    "=============================================="
  );


  // --------------------------------------------------------------
  // MOTOR PINS
  // --------------------------------------------------------------

  pinMode(PWMA, OUTPUT);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);


  pinMode(PWMB, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);


  pinMode(STBY, OUTPUT);


  digitalWrite(
    STBY,
    HIGH
  );


  stopMotors();


  // --------------------------------------------------------------
  // ENCODER
  // --------------------------------------------------------------

  pinMode(
    ENC_A,
    INPUT_PULLUP
  );

  pinMode(
    ENC_B,
    INPUT_PULLUP
  );


  attachInterrupt(
    digitalPinToInterrupt(ENC_A),
    encoderISR,
    CHANGE
  );


  // --------------------------------------------------------------
  // I2C
  // --------------------------------------------------------------

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  Wire.setClock(
    400000
  );


  delay(100);


  // --------------------------------------------------------------
  // MPU9250 CHECK
  // --------------------------------------------------------------

  if (!testMPU())
  {
    Serial.println();
    Serial.println(
      "ERROR: MPU9250 NOT FOUND!"
    );

    Serial.println(
      "Check:"
    );

    Serial.println(
      "VCC -> ESP32 3.3V"
    );

    Serial.println(
      "GND -> GND"
    );

    Serial.println(
      "SDA -> GPIO22"
    );

    Serial.println(
      "SCL -> GPIO23"
    );

    Serial.println();

    while (true)
    {
      stopMotors();

      delay(1000);
    }
  }


  Serial.println(
    "MPU9250 detected at 0x68."
  );


  initializeMPU();


  previousMicros = micros();


  printCommands();


  Serial.println(
    "ZENO READY."
  );

  Serial.println(
    "Place robot vertically and press C."
  );
}


// ================================================================
// MAIN LOOP
// ================================================================

void loop()
{
  // Serial commands

  processSerial();


  // Read MPU

  if (!readMPU())
  {
    stopMotors();

    return;
  }


  // Calculate angle

  calculateAngle();


  // --------------------------------------------------------------
  // BALANCING
  // --------------------------------------------------------------

  if (
    calibrated &&
    balanceEnabled
  )
  {

    // ------------------------------------------------------------
    // FALL PROTECTION
    // ------------------------------------------------------------

    if (
      abs(pitchAngle) >
      FALL_ANGLE
    )
    {
      stopMotors();

      balanceEnabled = false;

      integral = 0;

      Serial.println();
      Serial.println(
        "FALL DETECTED - MOTORS STOPPED"
      );

      Serial.println(
        "Return ZENO upright and press B."
      );

      delay(500);
    }

    else
    {
      calculatePID();

      driveMotors(
        pidOutput
      );
    }
  }

  else
  {
    stopMotors();
  }


  printDebug();


  // Approx. 200 Hz control loop

  delay(4);
}