#include <Wire.h>
#include <MPU9250_asukiaaa.h>

// =====================================================
//                    ZENO
//     ESP32 SELF BALANCING ROBOT
//
// ESP32 + MPU9250 + TB6612FNG
// 2 x N20 Encoder Motors
// =====================================================


// =====================================================
// MPU9250
// =====================================================

#define MPU_SDA 22
#define MPU_SCL 23

MPU9250_asukiaaa mpu;


// =====================================================
// TB6612FNG MOTOR DRIVER
// =====================================================

// LEFT MOTOR
#define PWMA 25
#define AIN1 26
#define AIN2 27

// RIGHT MOTOR
#define PWMB 33
#define BIN1 14
#define BIN2 13

#define STBY 32


// =====================================================
// ENCODERS
// =====================================================

// LEFT
#define LEFT_ENC_A 34
#define LEFT_ENC_B 35

// RIGHT
#define RIGHT_ENC_A 19
#define RIGHT_ENC_B 18


volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;


// =====================================================
// PWM SETTINGS
// =====================================================

#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 8

#define MAX_PWM 255

/*
  N20 motors often require some minimum PWM
  before they start moving.

  Start low during initial testing.
*/
int MIN_PWM = 35;


// =====================================================
// BALANCE PARAMETERS
// =====================================================

/*
   IMPORTANT:

   These are STARTING values only.

   PID MUST be tuned on your actual robot.
*/

float Kp = 18.0;
float Ki = 0.0;
float Kd = 0.65;


// Desired upright angle

float balanceSetpoint = 0.0;


// PID variables

float error = 0;
float previousError = 0;

float integral = 0;
float derivative = 0;

float pidOutput = 0;


// =====================================================
// IMU VARIABLES
// =====================================================

float ax = 0;
float ay = 0;
float az = 0;

float gx = 0;
float gy = 0;
float gz = 0;


// Gyroscope bias

float gyroOffsetX = 0;
float gyroOffsetY = 0;
float gyroOffsetZ = 0;


// Angle

float accelAngle = 0;
float balanceAngle = 0;


// =====================================================
// COMPLEMENTARY FILTER
// =====================================================

const float FILTER_ALPHA = 0.98;


// =====================================================
// LOOP TIMING
// =====================================================

// 5 ms = 200 Hz

const unsigned long CONTROL_PERIOD_US = 5000;

unsigned long previousControlTime = 0;


// =====================================================
// SAFETY
// =====================================================

const float FALL_ANGLE = 35.0;

// robot will only arm when reasonably upright

const float ARM_ANGLE = 8.0;

bool robotArmed = false;


// =====================================================
// MOTOR ORIENTATION
// =====================================================

/*
   Because the two motors are physically mounted
   opposite each other, one motor may need reversing.

   Change these if necessary.
*/

const int LEFT_MOTOR_DIRECTION = 1;
const int RIGHT_MOTOR_DIRECTION = 1;


// =====================================================
// ENCODER ISR
// =====================================================

void IRAM_ATTR leftEncoderISR()
{
  bool A = digitalRead(LEFT_ENC_A);
  bool B = digitalRead(LEFT_ENC_B);

  if (A == B)
  {
    leftEncoderCount++;
  }
  else
  {
    leftEncoderCount--;
  }
}


void IRAM_ATTR rightEncoderISR()
{
  bool A = digitalRead(RIGHT_ENC_A);
  bool B = digitalRead(RIGHT_ENC_B);

  if (A == B)
  {
    rightEncoderCount++;
  }
  else
  {
    rightEncoderCount--;
  }
}


// =====================================================
// LEFT MOTOR
// =====================================================

void setLeftMotor(int speed)
{
  speed *= LEFT_MOTOR_DIRECTION;

  speed = constrain(speed, -255, 255);


  if (speed > 0)
  {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

    ledcWrite(PWMA, speed);
  }

  else if (speed < 0)
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

    ledcWrite(PWMA, abs(speed));
  }

  else
  {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

    ledcWrite(PWMA, 0);
  }
}


// =====================================================
// RIGHT MOTOR
// =====================================================

void setRightMotor(int speed)
{
  speed *= RIGHT_MOTOR_DIRECTION;

  speed = constrain(speed, -255, 255);


  if (speed > 0)
  {
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);

    ledcWrite(PWMB, speed);
  }

  else if (speed < 0)
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);

    ledcWrite(PWMB, abs(speed));
  }

  else
  {
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);

    ledcWrite(PWMB, 0);
  }
}


// =====================================================
// STOP MOTORS
// =====================================================

void stopMotors()
{
  setLeftMotor(0);
  setRightMotor(0);
}


// =====================================================
// APPLY BALANCE MOTOR COMMAND
// =====================================================

void driveBalance(int motorCommand)
{
  motorCommand = constrain(
      motorCommand,
      -MAX_PWM,
      MAX_PWM
  );


  // Dead zone compensation

  if (motorCommand > 0 &&
      motorCommand < MIN_PWM)
  {
    motorCommand = MIN_PWM;
  }

  else if (motorCommand < 0 &&
           motorCommand > -MIN_PWM)
  {
    motorCommand = -MIN_PWM;
  }


  /*
     Both motors receive the same logical command.

     If one wheel physically spins backwards because
     of mounting orientation, change its
     *_MOTOR_DIRECTION constant above.
  */

  setLeftMotor(motorCommand);
  setRightMotor(motorCommand);
}


// =====================================================
// MPU READ
// =====================================================

bool readMPU()
{
  if (mpu.accelUpdate() != 0)
  {
    return false;
  }

  if (mpu.gyroUpdate() != 0)
  {
    return false;
  }


  ax = mpu.accelX();
  ay = mpu.accelY();
  az = mpu.accelZ();


  gx = mpu.gyroX() - gyroOffsetX;
  gy = mpu.gyroY() - gyroOffsetY;
  gz = mpu.gyroZ() - gyroOffsetZ;


  return true;
}


// =====================================================
// GYRO CALIBRATION
// =====================================================

void calibrateGyroscope()
{
  Serial.println();
  Serial.println("======================================");
  Serial.println("       GYROSCOPE CALIBRATION");
  Serial.println("======================================");

  Serial.println("KEEP ROBOT COMPLETELY STILL!");
  Serial.println();

  delay(3000);


  const int samples = 1500;

  double sumX = 0;
  double sumY = 0;
  double sumZ = 0;

  int validSamples = 0;


  for (int i = 0; i < samples; i++)
  {
    if (mpu.gyroUpdate() == 0)
    {
      sumX += mpu.gyroX();
      sumY += mpu.gyroY();
      sumZ += mpu.gyroZ();

      validSamples++;
    }

    delay(2);
  }


  if (validSamples > 0)
  {
    gyroOffsetX =
        sumX / validSamples;

    gyroOffsetY =
        sumY / validSamples;

    gyroOffsetZ =
        sumZ / validSamples;
  }


  Serial.println("Calibration complete");

  Serial.print("GX offset = ");
  Serial.println(gyroOffsetX, 4);

  Serial.print("GY offset = ");
  Serial.println(gyroOffsetY, 4);

  Serial.print("GZ offset = ");
  Serial.println(gyroOffsetZ, 4);

  Serial.println();
}


// =====================================================
// INITIAL ANGLE
// =====================================================

void initializeAngle()
{
  if (mpu.accelUpdate() == 0)
  {
    ax = mpu.accelX();
    ay = mpu.accelY();
    az = mpu.accelZ();


    /*
       Assumption:
       Forward/backward robot tilt corresponds
       to rotation around MPU Y axis.

       Therefore:
       accelerometer angle uses X and Z
       gyro rate uses GY.
    */

    accelAngle =
      atan2(
        ax,
        sqrt((ay * ay) + (az * az))
      ) * 180.0 / PI;


    balanceAngle = accelAngle;
  }


  Serial.print("Initial angle = ");
  Serial.println(balanceAngle);
}


// =====================================================
// BALANCE CONTROLLER
// =====================================================

void balanceController(float dt)
{
  if (!readMPU())
  {
    stopMotors();
    return;
  }


  // ---------------------------------------------------
  // ACCELEROMETER ANGLE
  // ---------------------------------------------------

  accelAngle =
    atan2(
      ax,
      sqrt((ay * ay) + (az * az))
    ) * 180.0 / PI;


  // ---------------------------------------------------
  // GYROSCOPE RATE
  // ---------------------------------------------------

  /*
     Pitch rotation around MPU Y axis
  */

  float gyroRate = gy;


  // ---------------------------------------------------
  // COMPLEMENTARY FILTER
  // ---------------------------------------------------

  balanceAngle =
      FILTER_ALPHA *
      (balanceAngle + gyroRate * dt)

      +

      (1.0 - FILTER_ALPHA) *
      accelAngle;


  // ---------------------------------------------------
  // FALL PROTECTION
  // ---------------------------------------------------

  if (abs(balanceAngle) > FALL_ANGLE)
  {
    robotArmed = false;

    integral = 0;
    previousError = 0;

    stopMotors();

    return;
  }


  // ---------------------------------------------------
  // AUTOMATIC ARM
  // ---------------------------------------------------

  if (!robotArmed)
  {
    stopMotors();

    if (abs(balanceAngle - balanceSetpoint)
        < ARM_ANGLE)
    {
      robotArmed = true;

      integral = 0;
      previousError = 0;

      Serial.println("BALANCE ENABLED");
    }

    return;
  }


  // ---------------------------------------------------
  // PID
  // ---------------------------------------------------

  error =
      balanceSetpoint -
      balanceAngle;


  // Integral

  integral += error * dt;


  // Integral anti-windup

  integral = constrain(
      integral,
      -50.0,
      50.0
  );


  // Derivative

  derivative =
      (error - previousError) / dt;


  // PID output

  pidOutput =
      (Kp * error)
      +
      (Ki * integral)
      +
      (Kd * derivative);


  previousError = error;


  // Limit output

  pidOutput =
      constrain(
          pidOutput,
          -MAX_PWM,
          MAX_PWM
      );


  driveBalance((int)pidOutput);
}


// =====================================================
// SERIAL COMMANDS
// =====================================================

void serialCommands()
{
  if (!Serial.available())
    return;


  String command =
      Serial.readStringUntil('\n');

  command.trim();


  // ---------------------------------------
  // Kp
  // Example:
  // P18.5
  // ---------------------------------------

  if (command.startsWith("P"))
  {
    Kp = command.substring(1).toFloat();

    Serial.print("Kp = ");
    Serial.println(Kp);
  }


  // ---------------------------------------
  // Ki
  // ---------------------------------------

  else if (command.startsWith("I"))
  {
    Ki = command.substring(1).toFloat();

    Serial.print("Ki = ");
    Serial.println(Ki);
  }


  // ---------------------------------------
  // Kd
  // ---------------------------------------

  else if (command.startsWith("D"))
  {
    Kd = command.substring(1).toFloat();

    Serial.print("Kd = ");
    Serial.println(Kd);
  }


  // ---------------------------------------
  // Setpoint
  //
  // Example:
  // A1.5
  // ---------------------------------------

  else if (command.startsWith("A"))
  {
    balanceSetpoint =
        command.substring(1).toFloat();

    Serial.print("Balance setpoint = ");
    Serial.println(balanceSetpoint);
  }


  // ---------------------------------------
  // Emergency stop
  // ---------------------------------------

  else if (command == "STOP")
  {
    robotArmed = false;

    stopMotors();

    Serial.println("ROBOT STOPPED");
  }


  // ---------------------------------------
  // Reset encoders
  // ---------------------------------------

  else if (command == "ZERO")
  {
    noInterrupts();

    leftEncoderCount = 0;
    rightEncoderCount = 0;

    interrupts();

    Serial.println("Encoder counters reset");
  }
}


// =====================================================
// DEBUG OUTPUT
// =====================================================

void printDebug()
{
  static unsigned long previousPrint = 0;


  if (millis() - previousPrint >= 100)
  {
    long leftCount;
    long rightCount;


    noInterrupts();

    leftCount = leftEncoderCount;
    rightCount = rightEncoderCount;

    interrupts();


    Serial.print("Angle: ");
    Serial.print(balanceAngle, 2);


    Serial.print(" | Acc: ");
    Serial.print(accelAngle, 2);


    Serial.print(" | PID: ");
    Serial.print(pidOutput, 1);


    Serial.print(" | L: ");
    Serial.print(leftCount);


    Serial.print(" | R: ");
    Serial.print(rightCount);


    Serial.print(" | ");

    if (robotArmed)
      Serial.println("ACTIVE");
    else
      Serial.println("SAFE");


    previousPrint = millis();
  }
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("               ZENO");
  Serial.println("       SELF BALANCING ROBOT");
  Serial.println("======================================");


  // ===================================================
  // MOTOR PINS
  // ===================================================

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);


  digitalWrite(STBY, HIGH);


  // PWM

  ledcAttach(
      PWMA,
      PWM_FREQUENCY,
      PWM_RESOLUTION
  );

  ledcAttach(
      PWMB,
      PWM_FREQUENCY,
      PWM_RESOLUTION
  );


  stopMotors();


  // ===================================================
  // ENCODERS
  // ===================================================

  pinMode(LEFT_ENC_A, INPUT);
  pinMode(LEFT_ENC_B, INPUT);

  pinMode(RIGHT_ENC_A, INPUT);
  pinMode(RIGHT_ENC_B, INPUT);


  attachInterrupt(
      digitalPinToInterrupt(LEFT_ENC_A),
      leftEncoderISR,
      CHANGE
  );


  attachInterrupt(
      digitalPinToInterrupt(RIGHT_ENC_A),
      rightEncoderISR,
      CHANGE
  );


  // ===================================================
  // MPU9250
  // ===================================================

  Wire.begin(
      MPU_SDA,
      MPU_SCL
  );

  Wire.setClock(400000);


  mpu.setWire(&Wire);


  uint8_t accelStatus =
      mpu.beginAccel();

  uint8_t gyroStatus =
      mpu.beginGyro();


  Serial.print("Accel status: ");
  Serial.println(accelStatus);

  Serial.print("Gyro status: ");
  Serial.println(gyroStatus);


  if (accelStatus != 0 ||
      gyroStatus != 0)
  {
    Serial.println();
    Serial.println(
      "ERROR: MPU9250 initialization failed!"
    );

    while (1)
    {
      stopMotors();

      delay(1000);
    }
  }


  // ===================================================
  // CALIBRATE
  // ===================================================

  calibrateGyroscope();


  // ===================================================
  // INITIAL ANGLE
  // ===================================================

  initializeAngle();


  previousControlTime = micros();


  Serial.println();
  Serial.println("======================================");
  Serial.println("SYSTEM READY");
  Serial.println("======================================");

  Serial.println();
  Serial.println("PID tuning commands:");

  Serial.println("P20    -> set Kp");
  Serial.println("I0.1   -> set Ki");
  Serial.println("D0.8   -> set Kd");
  Serial.println("A1.2   -> balance angle");
  Serial.println("STOP   -> emergency stop");
  Serial.println("ZERO   -> reset encoders");

  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  serialCommands();


  unsigned long currentTime =
      micros();


  if (currentTime - previousControlTime
      >= CONTROL_PERIOD_US)
  {
    float dt =
      (currentTime -
       previousControlTime)
      / 1000000.0;


    previousControlTime =
        currentTime;


    balanceController(dt);
  }


  printDebug();
}