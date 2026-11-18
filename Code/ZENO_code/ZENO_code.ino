/*
  ================================================================
                  ZENO SELF-BALANCING ROBOT  (v2)
  ================================================================

  Controller : ESP32 DevKit V1   (Arduino-ESP32 core 2.x or 3.x)
  IMU        : MPU9250 / MPU6500 / MPU6050 (raw I2C, no library)
  Driver     : TB6612FNG
  Motors     : 2x N20 3V 500RPM encoder motors

  Control structure (cascade):

      wheel encoders ──► speed + position loop ──► target lean angle
                                                         │
      IMU ──► complementary filter ──► pitch ──► angle PD loop ──► PWM

  The inner loop keeps ZENO upright; the outer loop stops it from
  running away (without it, any trim error makes the robot drive off
  until it falls).

  ---------------------- FIRST-TIME SETUP --------------------------
  Serial Monitor: 115200 baud, line ending "Newline" (or NL & CR).

   1. Hold ZENO with wheels OFF the ground, type:  motor
      Both wheels should spin toward ZENO's FRONT. If one spins
      backwards type flipl / flipr and run "motor" again.
      (This also measures motor deadband and fixes encoder signs.)
   2. Type:  axis   and follow the prompts (hold upright, then tilt
      toward the FRONT). This finds the IMU axes and signs.
   3. Hold ZENO at its balance point, type:  c
   4. Type:  save      (stores everything in flash)
   5. Type:  b   and let go gently. Tune with kp / kd / vkp / xkp.

  Type "help" for all commands.
  ================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

// ================================================================
// PIN MAP  (matches the previous ZENO_code.ino / README / MPU9250.ino)
// NOTE: Code/Motor_Test/Motor_Test.ino and the PCB draft use
// DIFFERENT pins. Make sure this block matches your real wiring.
// ================================================================

#define SDA_PIN 22
#define SCL_PIN 23

#define PWMA 25   // left motor
#define AIN1 26
#define AIN2 27

#define PWMB 33   // right motor
#define BIN1 14
#define BIN2 13

#define STBY 32

#define ENC_L_A 34   // GPIO34/35 are input-only: NO internal pull-ups.
#define ENC_L_B 35   // Use encoder board pull-ups or external 10k to 3V3.
#define ENC_R_A 19
#define ENC_R_B 18

// ================================================================
// HARDWARE CONSTANTS  (edit to match your robot)
// ================================================================

const float WHEEL_DIAMETER_M = 0.043f;   // measure your wheel
// Encoder counts per WHEEL revolution with this firmware's x2 decoding
// (channel A, both edges) = 2 * magnet pulses * gear ratio.
// Check: type "enc", turn one wheel exactly one turn by hand, "enc" again.
const float COUNTS_PER_WHEEL_REV = 420.0f;

const uint32_t PWM_FREQ   = 20000;  // 20 kHz: silent, smooth N20 drive
const uint8_t  PWM_BITS   = 8;
const int      PWM_MAX    = 255;

const float CONTROL_HZ    = 200.0f;
const uint32_t CONTROL_US = (uint32_t)(1000000.0f / CONTROL_HZ);

const float FALL_ANGLE    = 45.0f;  // disarm beyond this tilt (deg)
const float ARM_ANGLE     = 5.0f;   // must be within this of upright to arm
const uint32_t ARM_HOLD_MS = 400;   // ...for this long
const float MAX_LEAN      = 7.0f;   // outer loop lean-angle limit (deg)

const float FILTER_TAU    = 0.5f;   // complementary filter time constant (s)

#define MPU_ADDR 0x68

// ================================================================
// SETTINGS (persisted to flash with "save")
// ================================================================

struct Settings {
  uint32_t magic;
  // angle loop: PWM per degree, PWM per (deg/s), PWM per (deg*s)
  float kp, ki, kd;
  // speed/position loop: degrees lean per (m/s) and per m
  float vkp, xkp;
  float trim;          // balance-point offset (deg)
  float driveSpeed;    // m/s for f / r
  float turnPWM;       // differential PWM for l / t
  // IMU axis mapping (from "axis" wizard)
  int8_t upAxis, upSign;
  int8_t fwdAxis, fwdSign;
  int8_t gyroAxis, gyroSign;
  // motors
  int8_t motorDirL, motorDirR;
  int8_t encDirL, encDirR;
  int16_t minPwmL, minPwmR;
  bool calibrated;
};

const uint32_t SETTINGS_MAGIC = 0x5A454E32;  // "ZEN2"

Settings cfg;

void loadDefaults() {
  cfg.magic = SETTINGS_MAGIC;
  cfg.kp = 25.0f;
  cfg.ki = 0.0f;
  cfg.kd = 0.6f;
  cfg.vkp = 12.0f;
  cfg.xkp = 5.0f;      // set 0 while tuning the angle loop alone
  cfg.trim = 0.0f;
  cfg.driveSpeed = 0.15f;
  cfg.turnPWM = 35.0f;
  // Defaults for an IMU lying flat, X toward the front, Z up.
  // Right-hand rule: tilting forward = +rotation about Y, which makes
  // accel X go NEGATIVE. (The old firmware used +X with +gyroY, so the
  // gyro and accelerometer fought each other in the filter.)
  cfg.upAxis = 2;   cfg.upSign = 1;
  cfg.fwdAxis = 0;  cfg.fwdSign = -1;
  cfg.gyroAxis = 1; cfg.gyroSign = 1;
  cfg.motorDirL = 1;  cfg.motorDirR = -1;  // mirrored mounting
  cfg.encDirL = 1;    cfg.encDirR = 1;
  cfg.minPwmL = 25;   cfg.minPwmR = 25;
  cfg.calibrated = false;
}

Preferences prefs;

bool loadSettings() {
  prefs.begin("zeno", true);
  size_t n = prefs.getBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();
  if (n != sizeof(cfg) || cfg.magic != SETTINGS_MAGIC) {
    loadDefaults();
    return false;
  }
  return true;
}

void saveSettings() {
  prefs.begin("zeno", false);
  prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();
  Serial.println("Settings saved to flash.");
}

// ================================================================
// STATE
// ================================================================

float acc[3], gyr[3];          // g, deg/s (gyro bias removed)
float gyroBias[3] = {0, 0, 0};

float accPitch = 0;            // deg, from accelerometer
float pitch = 0;               // deg, fused (positive = leaning forward)
float pitchRate = 0;           // deg/s

volatile int32_t encL = 0, encR = 0;
int32_t lastEncL = 0, lastEncR = 0;
float wheelSpeed = 0;          // m/s, filtered, positive = forward
float wheelPos = 0;            // m
float posRef = 0;

float speedCmd = 0, speedCmdRamped = 0;
float turnCmd = 0, turnCmdRamped = 0;

float targetPitch = 0;
float angleIntegral = 0;
float u = 0;                   // balance PWM
int outL = 0, outR = 0;

bool balanceEnabled = false;   // user wants balancing
bool armed = false;            // motors actually driving
uint32_t uprightSince = 0;

bool telemetry = true;
uint32_t lastTelemetry = 0;
uint32_t lastControl = 0;
uint32_t maxLoopUs = 0;
int imuFailCount = 0;

// ================================================================
// IMU
// ================================================================

void imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

int imuRead8(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

bool imuInit() {
  int who = imuRead8(0x75);
  if (who < 0) return false;
  Serial.printf("IMU WHO_AM_I = 0x%02X (%s)\n", who,
                who == 0x71 ? "MPU9250" : who == 0x73 ? "MPU9255" :
                who == 0x70 ? "MPU6500" : who == 0x68 ? "MPU6050" :
                "unknown/clone");
  imuWrite(0x6B, 0x80);  delay(100);  // reset
  imuWrite(0x6B, 0x01);  delay(10);   // PLL clock
  imuWrite(0x19, 0x00);               // sample rate = 1 kHz
  imuWrite(0x1A, 0x03);               // DLPF ~41 Hz
  imuWrite(0x1B, 0x08);               // gyro  ±500 dps (±250 can clip in a fall)
  imuWrite(0x1C, 0x00);               // accel ±2 g
  imuWrite(0x1D, 0x03);               // accel DLPF ~41 Hz (MPU6500/9250)
  delay(50);
  return true;
}

// Reads raw sensor values (gyro bias removed). Returns false on I2C error.
bool imuReadRaw() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; i++) b[i] = Wire.read();
  for (int i = 0; i < 3; i++) {
    int16_t a = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
    int16_t g = (int16_t)((b[8 + 2 * i] << 8) | b[9 + 2 * i]);
    acc[i] = a / 16384.0f;
    gyr[i] = g / 65.5f - gyroBias[i];
  }
  return true;
}

float computeAccPitch() {
  float f = cfg.fwdSign * acc[cfg.fwdAxis];
  float up = cfg.upSign * acc[cfg.upAxis];
  return atan2f(f, up) * RAD_TO_DEG;
}

// Averages the sensor for `ms` while still. Returns accel/gyro means
// and the gyro standard deviation (to detect motion).
bool imuAverage(uint32_t ms, float accMean[3], float gyroMean[3], float &gyroStd) {
  double sa[3] = {0}, sg[3] = {0}, sg2 = 0;
  int n = 0;
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (imuReadRaw()) {
      for (int i = 0; i < 3; i++) {
        float graw = gyr[i] + gyroBias[i];
        sa[i] += acc[i];
        sg[i] += graw;
        sg2 += (double)graw * graw;
      }
      n++;
    }
    delay(2);
  }
  if (n < 20) return false;
  double var = 0;
  for (int i = 0; i < 3; i++) {
    accMean[i] = sa[i] / n;
    gyroMean[i] = sg[i] / n;
    var += (double)gyroMean[i] * gyroMean[i];
  }
  var = sg2 / n - var;
  gyroStd = sqrt(var > 0 ? var : 0);
  return true;
}

bool calibrateGyro(bool verbose) {
  float a[3], g[3], sd;
  if (!imuAverage(1000, a, g, sd)) return false;
  if (sd > 2.0f) {
    if (verbose) Serial.printf("Robot moved during gyro calibration (noise %.2f dps). Hold still and retry.\n", sd);
    return false;
  }
  for (int i = 0; i < 3; i++) gyroBias[i] = g[i];
  return true;
}

// ================================================================
// MOTORS (LEDC 20 kHz)
// ================================================================

void pwmSetup() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PWMA, PWM_FREQ, PWM_BITS);
  ledcAttach(PWMB, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(0, PWM_FREQ, PWM_BITS);
  ledcSetup(1, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PWMA, 0);
  ledcAttachPin(PWMB, 1);
#endif
}

void pwmWrite(int pin, int duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(pin == PWMA ? 0 : 1, duty);
#endif
}

// pwm: -255..255, positive = drive ZENO forward
void setMotorRaw(int in1, int in2, int pwmPin, int pwm) {
  pwm = constrain(pwm, -PWM_MAX, PWM_MAX);
  if (pwm > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (pwm < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
  else              { digitalWrite(in1, LOW);  digitalWrite(in2, LOW); }
  pwmWrite(pwmPin, abs(pwm));
}

void setLeft(int pwm)  { setMotorRaw(AIN1, AIN2, PWMA, pwm * cfg.motorDirL); }
void setRight(int pwm) { setMotorRaw(BIN1, BIN2, PWMB, pwm * cfg.motorDirR); }

void motorsOff() {
  setLeft(0);
  setRight(0);
  digitalWrite(STBY, LOW);
  outL = outR = 0;
}

// Adds the deadband so tiny corrections actually move the wheels.
int withDeadband(float cmd, int minPwm) {
  if (fabsf(cmd) < 0.5f) return 0;
  float mag = minPwm + fabsf(cmd) * (PWM_MAX - minPwm) / (float)PWM_MAX;
  mag = min(mag, (float)PWM_MAX);
  return (int)(cmd > 0 ? mag : -mag);
}

// ================================================================
// ENCODERS (x2 decoding on channel A)
// ================================================================

void IRAM_ATTR isrLeft() {
  if (digitalRead(ENC_L_A) == digitalRead(ENC_L_B)) encL = encL - 1; else encL = encL + 1;
}
void IRAM_ATTR isrRight() {
  if (digitalRead(ENC_R_A) == digitalRead(ENC_R_B)) encR = encR - 1; else encR = encR + 1;
}

inline int32_t readEncL() { return encL * cfg.encDirL; }
inline int32_t readEncR() { return encR * cfg.encDirR; }

// ================================================================
// CONTROL LOOP (runs at CONTROL_HZ)
// ================================================================

void resetControllers() {
  angleIntegral = 0;
  lastEncL = readEncL();
  lastEncR = readEncR();
  wheelSpeed = 0;
  wheelPos = 0;
  posRef = 0;
  speedCmdRamped = 0;
  turnCmdRamped = 0;
  targetPitch = cfg.trim;
}

void controlStep(float dt) {
  // ---------- IMU ----------
  if (!imuReadRaw()) {
    if (++imuFailCount > 5) {
      if (armed) Serial.println("IMU READ FAILED - motors off");
      armed = false;
      motorsOff();
      if (imuFailCount % 200 == 0) imuInit();
    }
    return;
  }
  imuFailCount = 0;

  accPitch = computeAccPitch();
  pitchRate = cfg.gyroSign * gyr[cfg.gyroAxis];
  float alpha = FILTER_TAU / (FILTER_TAU + dt);
  pitch = alpha * (pitch + pitchRate * dt) + (1.0f - alpha) * accPitch;

  // ---------- wheels ----------
  int32_t eL = readEncL(), eR = readEncR();
  int32_t dL = eL - lastEncL, dR = eR - lastEncR;
  lastEncL = eL; lastEncR = eR;
  const float mPerCount = PI * WHEEL_DIAMETER_M / COUNTS_PER_WHEEL_REV;
  float dPos = 0.5f * (dL + dR) * mPerCount;
  wheelPos += dPos;
  float rawSpeed = dPos / dt;
  wheelSpeed += 0.1f * (rawSpeed - wheelSpeed);   // ~8 Hz low-pass

  // ---------- arming / safety ----------
  if (!balanceEnabled || !cfg.calibrated) {
    armed = false;
    motorsOff();
    return;
  }

  if (armed && fabsf(pitch) > FALL_ANGLE) {
    armed = false;
    motorsOff();
    Serial.println("FALL DETECTED - motors off. Stand ZENO upright to re-arm.");
    uprightSince = 0;
    return;
  }

  if (!armed) {
    motorsOff();
    if (fabsf(pitch - cfg.trim) < ARM_ANGLE && fabsf(pitchRate) < 30.0f) {
      if (uprightSince == 0) uprightSince = millis();
      if (millis() - uprightSince > ARM_HOLD_MS) {
        resetControllers();
        armed = true;
        digitalWrite(STBY, HIGH);
        Serial.println("ARMED - balancing");
      }
    } else {
      uprightSince = 0;
    }
    return;
  }

  // ---------- outer loop: speed / position -> lean angle ----------
  const float accelLimit = 0.5f;   // m/s^2 command ramp
  float step = accelLimit * dt;
  speedCmdRamped += constrain(speedCmd - speedCmdRamped, -step, step);
  turnCmdRamped  += constrain(turnCmd - turnCmdRamped, -200.0f * dt, 200.0f * dt);

  if (fabsf(speedCmdRamped) > 0.001f) posRef = wheelPos;   // driving: follow
  float lean = cfg.vkp * (wheelSpeed - speedCmdRamped)
             + cfg.xkp * (wheelPos - posRef);
  // Moving forward too fast -> lean BACK to slow down.
  targetPitch = cfg.trim - constrain(lean, -MAX_LEAN, MAX_LEAN);

  // ---------- inner loop: angle PID (D from gyro: no noise/kick) ----------
  float err = pitch - targetPitch;          // + = leaning too far forward
  float uUnsat = cfg.kp * err + cfg.ki * angleIntegral + cfg.kd * pitchRate;
  if (fabsf(uUnsat) < PWM_MAX || (uUnsat > 0) != (err > 0))
    angleIntegral = constrain(angleIntegral + err * dt, -20.0f, 20.0f);
  u = constrain(uUnsat, -(float)PWM_MAX, (float)PWM_MAX);

  // ---------- mix + drive ----------
  outL = withDeadband(constrain(u + turnCmdRamped, -255.0f, 255.0f), cfg.minPwmL);
  outR = withDeadband(constrain(u - turnCmdRamped, -255.0f, 255.0f), cfg.minPwmR);
  setLeft(outL);
  setRight(outR);
}

// ================================================================
// WIZARDS (motors disabled while running)
// ================================================================

void motorWizard() {
  balanceEnabled = false; armed = false; motorsOff();
  Serial.println("\n=== MOTOR TEST: keep wheels OFF the ground ===");
  delay(1500);
  digitalWrite(STBY, HIGH);

  for (int m = 0; m < 2; m++) {
    const char *name = m == 0 ? "LEFT" : "RIGHT";
    volatile int32_t *enc = m == 0 ? &encL : &encR;

    // 1) find deadband: ramp until encoder moves
    int32_t start = *enc;
    int found = -1;
    for (int p = 0; p <= 200; p++) {
      m == 0 ? setLeft(p) : setRight(p);
      delay(12);
      if (abs(*enc - start) >= 4) { found = p; break; }
    }
    m == 0 ? setLeft(0) : setRight(0);
    delay(300);

    // 2) direction: run forward, check encoder sign
    start = *enc;
    m == 0 ? setLeft(140) : setRight(140);
    delay(700);
    m == 0 ? setLeft(0) : setRight(0);
    int32_t delta = *enc - start;
    delay(300);

    if (found < 0 || abs(delta) < 10) {
      Serial.printf("%s: encoder did not count (delta=%ld). Check encoder wiring/power/pull-ups.\n",
                    name, (long)delta);
      continue;
    }
    int minPwm = max(10, (int)(found * 0.9f));
    if (m == 0) { cfg.minPwmL = minPwm; cfg.encDirL = delta > 0 ? 1 : -1; }
    else        { cfg.minPwmR = minPwm; cfg.encDirR = delta > 0 ? 1 : -1; }
    Serial.printf("%s: start PWM=%d -> deadband %d, %ld counts in 0.7 s, encoder sign %+d\n",
                  name, found, minPwm, (long)labs(delta), delta > 0 ? 1 : -1);
  }
  motorsOff();
  Serial.println("Did BOTH wheels spin toward ZENO's FRONT?");
  Serial.println("If not: flipl / flipr for the wrong wheel, then run 'motor' again.");
  Serial.println("Then run 'axis', then 'c', then 'save'.");
}

int argmaxAbs(const float v[3], int skip) {
  int best = -1;
  for (int i = 0; i < 3; i++) {
    if (i == skip) continue;
    if (best < 0 || fabsf(v[i]) > fabsf(v[best])) best = i;
  }
  return best;
}

void axisWizard() {
  balanceEnabled = false; armed = false; motorsOff();
  const char *AX = "XYZ";
  Serial.println("\n=== IMU AXIS WIZARD ===");
  Serial.println("Step 1: hold ZENO UPRIGHT and STILL...");
  delay(2000);
  if (!calibrateGyro(true)) return;
  float a0[3], g[3], sd;
  imuAverage(500, a0, g, sd);
  int up = argmaxAbs(a0, -1);
  int upSign = a0[up] > 0 ? 1 : -1;
  Serial.printf("  Up axis: %c%c (%.2f g)\n", upSign > 0 ? '+' : '-', AX[up], a0[up]);

  Serial.println("Step 2: within 4 s, slowly tilt ZENO ~25 deg toward its FRONT and hold it there.");
  float gInt[3] = {0, 0, 0};
  uint32_t t0 = micros(), last = t0;
  while (micros() - t0 < 4000000UL) {
    if (imuReadRaw()) {
      uint32_t now = micros();
      float dt = (now - last) * 1e-6f;
      last = now;
      for (int i = 0; i < 3; i++) gInt[i] += gyr[i] * dt;
    }
    delay(3);
  }
  float a1[3];
  imuAverage(300, a1, g, sd);
  float dA[3];
  for (int i = 0; i < 3; i++) dA[i] = a1[i] - a0[i];
  int fwd = argmaxAbs(dA, up);
  int gax = argmaxAbs(gInt, -1);
  float accAngle = atan2f(fabsf(a1[fwd]), fabsf(a1[up])) * RAD_TO_DEG;

  Serial.printf("  Gyro integrated: X %.1f  Y %.1f  Z %.1f deg\n", gInt[0], gInt[1], gInt[2]);
  Serial.printf("  Accel tilt: %.1f deg on %c\n", accAngle, AX[fwd]);

  if (fabsf(gInt[gax]) < 10.0f || accAngle < 10.0f) {
    Serial.println("  Not enough tilt detected. Run 'axis' again and tilt further (~25 deg).");
    return;
  }
  float ratio = fabsf(gInt[gax]) / accAngle;
  if (ratio < 0.6f || ratio > 1.6f) {
    Serial.println("  WARNING: gyro and accel angles disagree; did ZENO move/shake? Consider re-running.");
  }
  cfg.upAxis = up;    cfg.upSign = upSign;
  cfg.fwdAxis = fwd;  cfg.fwdSign = dA[fwd] > 0 ? 1 : -1;
  cfg.gyroAxis = gax; cfg.gyroSign = gInt[gax] > 0 ? 1 : -1;
  cfg.calibrated = false;  // balance point must be re-taken
  Serial.printf("  Result: up=%c%c  forward=%c%c  pitch gyro=%c%c\n",
                cfg.upSign > 0 ? '+' : '-', AX[cfg.upAxis],
                cfg.fwdSign > 0 ? '+' : '-', AX[cfg.fwdAxis],
                cfg.gyroSign > 0 ? '+' : '-', AX[cfg.gyroAxis]);
  Serial.println("Now hold ZENO at its balance point and type 'c', then 'save'.");
}

void calibrateBalancePoint() {
  balanceEnabled = false; armed = false; motorsOff();
  Serial.println("\nCALIBRATE: hold ZENO at its balance point, still...");
  delay(2000);
  if (!calibrateGyro(true)) return;
  float a[3], g[3], sd;
  imuAverage(800, a, g, sd);
  for (int i = 0; i < 3; i++) acc[i] = a[i];
  cfg.trim = computeAccPitch();
  pitch = cfg.trim;
  cfg.calibrated = true;
  Serial.printf("Balance point = %.2f deg. Gyro bias = %.2f %.2f %.2f dps\n",
                cfg.trim, gyroBias[0], gyroBias[1], gyroBias[2]);
  Serial.println("Type 'save' to keep it, 'b' to balance.");
}

// ================================================================
// SERIAL COMMANDS
// ================================================================

void printSettings() {
  const char *AX = "XYZ";
  Serial.println("\n----- ZENO SETTINGS -----");
  Serial.printf("kp %.3f  ki %.3f  kd %.3f\n", cfg.kp, cfg.ki, cfg.kd);
  Serial.printf("vkp %.3f  xkp %.3f  trim %.2f deg\n", cfg.vkp, cfg.xkp, cfg.trim);
  Serial.printf("speed %.2f m/s  turn %.0f PWM\n", cfg.driveSpeed, cfg.turnPWM);
  Serial.printf("IMU: up %c%c  fwd %c%c  gyro %c%c\n",
                cfg.upSign > 0 ? '+' : '-', AX[cfg.upAxis],
                cfg.fwdSign > 0 ? '+' : '-', AX[cfg.fwdAxis],
                cfg.gyroSign > 0 ? '+' : '-', AX[cfg.gyroAxis]);
  Serial.printf("motor dir L %+d R %+d  enc dir L %+d R %+d  minPWM L %d R %d\n",
                cfg.motorDirL, cfg.motorDirR, cfg.encDirL, cfg.encDirR, cfg.minPwmL, cfg.minPwmR);
  Serial.printf("calibrated: %s   max loop time: %lu us (budget %lu)\n",
                cfg.calibrated ? "yes" : "NO", (unsigned long)maxLoopUs, (unsigned long)CONTROL_US);
  Serial.println("-------------------------");
}

void printHelp() {
  Serial.println(
    "\n================ ZENO COMMANDS ================\n"
    " Setup:   motor | axis | c (calibrate) | save | load | defaults\n"
    " Run:     b (balance on) | s (stop)\n"
    " Drive:   f fwd | r rev | l left | t right | x stop moving\n"
    " Tune:    kp <v> | ki <v> | kd <v> | vkp <v> | xkp <v> | trim <deg>\n"
    "          speed <m/s> | turn <pwm> | minpwm <pwm>\n"
    "          I/i Kp+-0.5  O/o Ki+-0.01  D/d Kd+-0.05\n"
    " Motors:  flipl | flipr (reverse a wheel)\n"
    " Info:    p (settings) | tel (telemetry on/off) | enc | help\n"
    "===============================================");
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  // Single-character, case-sensitive tuning keys (legacy)
  if (line.length() == 1) {
    switch (line[0]) {
      case 'I': cfg.kp += 0.5f;  printSettings(); return;
      case 'i': cfg.kp = max(0.0f, cfg.kp - 0.5f); printSettings(); return;
      case 'O': cfg.ki += 0.01f; printSettings(); return;
      case 'o': cfg.ki = max(0.0f, cfg.ki - 0.01f); printSettings(); return;
      case 'D': cfg.kd += 0.05f; printSettings(); return;
      case 'd': cfg.kd = max(0.0f, cfg.kd - 0.05f); printSettings(); return;
    }
  }

  int sp = line.indexOf(' ');
  String cmd = sp < 0 ? line : line.substring(0, sp);
  String arg = sp < 0 ? "" : line.substring(sp + 1);
  cmd.toLowerCase();
  bool hasArg = arg.length() > 0;
  float v = arg.toFloat();

  if (cmd == "c" || cmd == "cal")          calibrateBalancePoint();
  else if (cmd == "motor")                 motorWizard();
  else if (cmd == "axis")                  axisWizard();
  else if (cmd == "b" || cmd == "start") {
    if (!cfg.calibrated) { Serial.println("Calibrate first: 'c'."); return; }
    balanceEnabled = true; uprightSince = 0;
    Serial.println("BALANCE ENABLED - hold ZENO upright, it arms automatically.");
  }
  else if (cmd == "s" || cmd == "stop") {
    balanceEnabled = false; armed = false; speedCmd = 0; turnCmd = 0; motorsOff();
    Serial.println("STOPPED");
  }
  else if (cmd == "f") { speedCmd =  cfg.driveSpeed; Serial.println("FORWARD"); }
  else if (cmd == "r") { speedCmd = -cfg.driveSpeed; Serial.println("REVERSE"); }
  else if (cmd == "l") { turnCmd = -cfg.turnPWM;     Serial.println("LEFT"); }
  else if (cmd == "t") { turnCmd =  cfg.turnPWM;     Serial.println("RIGHT"); }
  else if (cmd == "x") { speedCmd = 0; turnCmd = 0;  Serial.println("HOLD POSITION"); }
  else if (cmd == "kp"  && hasArg) { cfg.kp = v;  printSettings(); }
  else if (cmd == "ki"  && hasArg) { cfg.ki = v;  printSettings(); }
  else if (cmd == "kd"  && hasArg) { cfg.kd = v;  printSettings(); }
  else if (cmd == "vkp" && hasArg) { cfg.vkp = v; printSettings(); }
  else if (cmd == "xkp" && hasArg) { cfg.xkp = v; printSettings(); }
  else if (cmd == "trim" && hasArg) { cfg.trim = v; printSettings(); }
  else if (cmd == "speed" && hasArg) { cfg.driveSpeed = v; printSettings(); }
  else if (cmd == "turn" && hasArg) { cfg.turnPWM = v; printSettings(); }
  else if (cmd == "minpwm" && hasArg) { cfg.minPwmL = cfg.minPwmR = (int)v; printSettings(); }
  else if (cmd == "flipl") { cfg.motorDirL = -cfg.motorDirL; Serial.println("Left motor reversed. Re-run 'motor'."); }
  else if (cmd == "flipr") { cfg.motorDirR = -cfg.motorDirR; Serial.println("Right motor reversed. Re-run 'motor'."); }
  else if (cmd == "save")     saveSettings();
  else if (cmd == "load")     { Serial.println(loadSettings() ? "Loaded." : "No saved settings; defaults."); printSettings(); }
  else if (cmd == "defaults") { loadDefaults(); Serial.println("Defaults loaded (not saved)."); printSettings(); }
  else if (cmd == "p" || cmd == "show") printSettings();
  else if (cmd == "tel")      { telemetry = !telemetry; Serial.printf("Telemetry %s\n", telemetry ? "ON" : "OFF"); }
  else if (cmd == "enc")      Serial.printf("enc L %ld  R %ld\n", (long)readEncL(), (long)readEncR());
  else if (cmd == "help" || cmd == "h" || cmd == "?") printHelp();
  else Serial.printf("Unknown command '%s' (type help)\n", line.c_str());
}

void pollSerial() {
  static String buf;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (buf.length()) handleCommand(buf);
      buf = "";
      lastControl = micros();   // wizards may have blocked; don't "catch up"
    } else if (buf.length() < 40) {
      buf += ch;
    }
  }
}

void printTelemetry() {
  if (!telemetry || millis() - lastTelemetry < 50) return;
  lastTelemetry = millis();
  char line[160];
  int n = snprintf(line, sizeof(line),
                   "pitch:%.2f target:%.2f acc:%.2f rate:%.1f u:%.0f L:%d R:%d v:%.3f x:%.3f armed:%d\n",
                   pitch, targetPitch, accPitch, pitchRate, u, outL, outR,
                   wheelSpeed, wheelPos, armed ? 1 : 0);
  // never block the control loop on a full serial buffer
  if (Serial.availableForWrite() >= n) Serial.write(line, n);
}

// ================================================================
// SETUP / LOOP
// ================================================================

void setup() {
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);          // driver disabled during boot
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pwmSetup();

  Serial.begin(115200);
  delay(500);
  Serial.println("\n===== ZENO SELF-BALANCING ROBOT v2 =====");

  bool loaded = loadSettings();
  motorsOff();

  pinMode(ENC_L_A, INPUT); pinMode(ENC_L_B, INPUT);              // 34/35: no pull-ups
  pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_L_A), isrLeft, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrRight, CHANGE);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  Wire.setTimeOut(5);

  while (!imuInit()) {
    Serial.println("ERROR: IMU not found at 0x68. Check VCC=3V3, GND, SDA=GPIO22, SCL=GPIO23, AD0=GND.");
    delay(1000);
  }

  Serial.println("Hold still: measuring gyro bias...");
  if (!calibrateGyro(true)) Serial.println("Gyro bias not set - run 'c' before balancing.");
  if (imuReadRaw()) pitch = computeAccPitch();

  Serial.println(loaded ? "Saved settings loaded." : "No saved settings - run setup: motor, axis, c, save.");
  printSettings();
  printHelp();
  lastControl = micros();
}

void loop() {
  pollSerial();

  uint32_t now = micros();
  uint32_t elapsed = now - lastControl;
  if (elapsed >= CONTROL_US) {
    float dt = elapsed * 1e-6f;
    if (dt > 0.02f) dt = 1.0f / CONTROL_HZ;   // after a stall, don't integrate garbage
    lastControl = now;
    controlStep(dt);
    uint32_t used = micros() - now;
    if (used > maxLoopUs) maxLoopUs = used;
    printTelemetry();
  }
}
