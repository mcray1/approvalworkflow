/**
 * ESP32_LineFollower_OneFile.ino
 * =============================================================================
 * Single-file ESP32 line-follower firmware, matched to THIS build's wiring.
 *
 *   Sensors  : QTR-8RC on GPIO 16,17,18,19,21,22,23,4   (LEDON tied to 3.3 V)
 *   Driver   : TB6612FNG
 *     LEFT  (A): AIN1=26  AIN2=27  PWMA=25
 *     RIGHT (B): BIN1=12  BIN2=13  PWMB=14
 *     STBY     : GPIO33  (software-armed; 10 kO pull-down to GND)
 *   Button   : GPIO34  (external 10 kO pull-up to 3.3 V; optional)
 *
 * Toolchain: Arduino-ESP32 core 3.x  +  Pololu QTRSensors library.
 *
 * SERIAL COMMANDS (115200):
 *   s  start      x  stop        c  recalibrate     d  diagnostics
 *   p  print PID  h  help        t  MOTOR TEST (spins wheels, no line needed)
 *   P<v> I<v> D<v>  set gains     B<v>  set base PWM
 *
 * Motors are DISARMED at boot and in every non-driving state. Nothing moves
 * until you calibrate and then start. Keep wheels off the ground for first tests.
 * =============================================================================
 */
#include <QTRSensors.h>

// ======================= CONFIG =======================

// ---- Sensors ----
static const uint8_t SENSOR_COUNT = 8;
static const uint8_t SENSOR_PINS[SENSOR_COUNT] = {16, 17, 18, 19, 21, 22, 23, 4};
// Emitter (LEDON) is tied HIGH in hardware -> not controlled here.

// ---- Motors (TB6612) ----
static const uint8_t LEFT_IN1_PIN  = 26;   // AIN1
static const uint8_t LEFT_IN2_PIN  = 27;   // AIN2
static const uint8_t LEFT_PWM_PIN  = 25;   // PWMA
static const uint8_t RIGHT_IN1_PIN = 12;   // BIN1
static const uint8_t RIGHT_IN2_PIN = 13;   // BIN2
static const uint8_t RIGHT_PWM_PIN = 14;   // PWMB
// STBY: tie the TB6612 STBY pin DIRECTLY to 3.3 V (driver always enabled) and
// leave this at -1. Motors still only move when the firmware commands them.
// (Set this to a GPIO only if you later want software standby control.)
static const int8_t  STBY_PIN      = -1;   // -1 = STBY tied to 3.3 V in hardware

// Flip these in Test 4 if a wheel runs backwards (don't rewire):
static bool LEFT_INVERT  = false;
static bool RIGHT_INVERT = false;
// Flip to -1 if it steers AWAY from the line:
static float STEER_SIGN = +1.0f;

// ---- Button ----
static const uint8_t BUTTON_PIN = 34;      // input-only, external pull-up
static const bool    BUTTON_ACTIVE_LOW = true;

// ---- PWM (LEDC, core 3.x) ----
static const uint32_t PWM_FREQ = 20000;    // 20 kHz
static const uint8_t  PWM_RES  = 8;        // 0..255
static const int      PWM_MAXV = 255;

// ---- Speeds (0..255) ----
static int BASE_PWM     = 110;   // straight-line base speed
static int MAX_PWM      = 180;   // cap
static int MIN_PWM      = 60;    // floor while moving
static int RECOVERY_PWM = 95;    // search speed

// ---- PID (normalized error, roughly -1..+1) ----
static float Kp = 25.0f;
static float Ki = 0.0f;
static float Kd = 0.30f;
static const float D_FILTER = 0.70f;
static const float CORR_MAX = 255.0f;

// ---- Timing ----
static const uint32_t CONTROL_US = 5000;   // 5 ms = 200 Hz

// ---- Calibration ----
static const uint32_t CAL_PREROLL_MS = 1500;
static const uint32_t CAL_TIME_MS    = 4500;
static const uint16_t CONTRAST_MIN   = 500;   // per-channel (max-min) to pass

// ---- Line detection ----
static const uint16_t ON_LINE_MIN   = 250;    // calibrated value "on the line"
static const uint8_t  WIDE_BLACK_MAX = 6;     // > this many dark = lifted/off-track
static const uint8_t  LOST_DEBOUNCE = 3;      // cycles before "lost"
static const uint32_t RECOVERY_TIMEOUT_MS = 1200;

// ======================= STATE =======================
enum State { BOOT, CALIBRATING, READY, RUNNING, RECOVERING, STOPPED, ERR };
static State state = BOOT;

QTRSensors qtr;
static uint16_t sensorValues[SENSOR_COUNT];

static uint32_t lastControlUs = 0;
static float prevError = 0, filtD = 0;
static bool  dPrimed = false;
static int   lastDir = 0;             // -1 left, +1 right
static uint8_t lostCount = 0;
static uint32_t recoveryStartMs = 0;
static uint32_t calStartMs = 0, calStepMs = 0;
static bool  armed = false;

// button debounce
static bool btnStable = false, btnLast = false;
static uint32_t btnMs = 0;

// serial line buffer
static char cmd[16]; static uint8_t cmdLen = 0;

// ======================= MOTORS =======================
void armMotors(bool on) {
  armed = on;
  if (STBY_PIN >= 0) digitalWrite(STBY_PIN, on ? HIGH : LOW);
  if (!on) {
    ledcWrite(LEFT_PWM_PIN, 0);
    ledcWrite(RIGHT_PWM_PIN, 0);
  }
}

void driveOne(uint8_t in1, uint8_t in2, uint8_t pwmPin, int spd, bool invert) {
  if (invert) spd = -spd;
  if (spd > PWM_MAXV) spd = PWM_MAXV;
  if (spd < -PWM_MAXV) spd = -PWM_MAXV;
  if (spd > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  ledcWrite(pwmPin, spd); }
  else if (spd < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); ledcWrite(pwmPin, -spd); }
  else              { digitalWrite(in1, LOW);  digitalWrite(in2, LOW);  ledcWrite(pwmPin, 0); }
}

void setMotors(int left, int right) {
  if (!armed) { ledcWrite(LEFT_PWM_PIN, 0); ledcWrite(RIGHT_PWM_PIN, 0); return; }
  driveOne(LEFT_IN1_PIN,  LEFT_IN2_PIN,  LEFT_PWM_PIN,  left,  LEFT_INVERT);
  driveOne(RIGHT_IN1_PIN, RIGHT_IN2_PIN, RIGHT_PWM_PIN, right, RIGHT_INVERT);
}

// ======================= SENSORS =======================
// Returns true if a usable line is seen; fills pos (0..7000) and conf (sum).
bool readLine(uint16_t &pos, uint16_t &conf, uint8_t &onCount) {
  pos = qtr.readLineBlack(sensorValues);     // calibrated read, 0..7000
  uint32_t sum = 0; onCount = 0;
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    sum += sensorValues[i];
    if (sensorValues[i] >= ON_LINE_MIN) onCount++;
  }
  conf = (sum > 65535) ? 65535 : sum;
  if (onCount == 0) return false;            // all white -> no line
  if (onCount > WIDE_BLACK_MAX) return false;// nearly all black -> lifted/off track
  return true;
}

// ======================= STATE HELPERS =======================
void setState(State s) {
  state = s;
  switch (s) {
    case CALIBRATING: armMotors(false); break;
    case READY:       armMotors(false); Serial.println(F("[READY] on the line, then 's' to start.")); break;
    case RUNNING:     break;
    case RECOVERING:  break;
    case STOPPED:     armMotors(false); setMotors(0,0); Serial.println(F("[STOPPED] 's' to restart, 'c' to recalibrate.")); break;
    case ERR:         armMotors(false); setMotors(0,0); Serial.println(F("[ERROR] 'c' to recalibrate.")); break;
    default: break;
  }
}

void startCalibration() {
  armMotors(false); setMotors(0,0);
  Serial.println(F("[CAL] Starting in 1.5 s. Sweep the bar over BLACK and WHITE for 4.5 s. Motors OFF."));
  calStartMs = millis(); calStepMs = millis();
  state = CALIBRATING;
}

bool validateCalibration() {
  bool ok = true;
  Serial.println(F("  idx   min    max  contrast  status"));
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    uint16_t mn = qtr.calibrationOn.minimum ? qtr.calibrationOn.minimum[i] : 0;
    uint16_t mx = qtr.calibrationOn.maximum ? qtr.calibrationOn.maximum[i] : 0;
    int c = (int)mx - (int)mn;
    bool good = c >= CONTRAST_MIN;
    if (!good) ok = false;
    Serial.printf("  S%u  %5u  %5u  %7d  %s\n", i+1, mn, mx, c, good ? "ok" : "LOW CONTRAST");
  }
  return ok;
}

void startRun() {
  if (state != READY) { Serial.println(F("[START] refused: not READY (calibrate first).")); return; }
  uint16_t pos, conf; uint8_t on;
  if (!readLine(pos, conf, on)) { Serial.println(F("[START] no line under the sensors — place it on the line.")); return; }
  prevError = 0; filtD = 0; dPrimed = true; lostCount = 0;
  armMotors(true);
  lastControlUs = micros();
  Serial.println(F("[RUN] started."));
  setState(RUNNING);
}

// ======================= MOTOR TEST (command 't') =======================
// Spins each wheel so you can verify wiring/direction WITHOUT a line.
void motorTest() {
  Serial.println(F("[TEST] Motor test — wheels should spin. Watch direction."));
  armMotors(true);
  Serial.println(F("[TEST] LEFT forward..."));
  setMotors(120, 0);   delay(900);
  setMotors(0, 0);     delay(400);
  Serial.println(F("[TEST] RIGHT forward..."));
  setMotors(0, 120);   delay(900);
  setMotors(0, 0);     delay(400);
  Serial.println(F("[TEST] BOTH forward..."));
  setMotors(120, 120); delay(900);
  setMotors(0, 0);
  armMotors(false);
  Serial.println(F("[TEST] done. If nothing moved: check STBY=3.3V in test, VM voltage, motor leads, common GND."));
}

// ======================= DIAGNOSTICS =======================
void printDiag() {
  uint16_t pos, conf; uint8_t on;
  bool line = readLine(pos, conf, on);
  Serial.printf("[DIAG] state=%d armed=%d pos=%u conf=%u on=%u line=%d\n",
                (int)state, armed?1:0, pos, conf, on, line?1:0);
  Serial.print(F("  cal=["));
  for (uint8_t i=0;i<SENSOR_COUNT;i++){ Serial.print(sensorValues[i]); if(i<7)Serial.print(','); }
  Serial.println(F("]"));
}
void printPID() { Serial.printf("[PID] Kp=%.3f Ki=%.3f Kd=%.3f  BASE=%d MAX=%d  STEER=%.0f\n", Kp,Ki,Kd,BASE_PWM,MAX_PWM,STEER_SIGN); }
void printHelp() {
  Serial.println(F("Commands: s start | x stop | c recalibrate | t MOTOR TEST | d diag | p pid | h help"));
  Serial.println(F("Tune: P<v> I<v> D<v> B<base>   e.g. P22.5  or  B120"));
}

// ======================= CONTROL CYCLES =======================
void runningCycle(float dt) {
  uint16_t pos, conf; uint8_t on;
  if (!readLine(pos, conf, on)) {
    if (++lostCount >= LOST_DEBOUNCE) {
      Serial.println(F("[LINE LOST] -> RECOVERING"));
      recoveryStartMs = millis();
      setState(RECOVERING);
    }
    return;
  }
  lostCount = 0;

  float error = ((float)pos - 3500.0f) / 3500.0f;    // normalized
  if (pos < 3100) lastDir = -1; else if (pos > 3900) lastDir = +1; else lastDir = 0;

  float rawD = dPrimed ? 0.0f : (error - prevError) / dt;
  dPrimed = false;
  filtD = D_FILTER*filtD + (1.0f-D_FILTER)*rawD;
  prevError = error;

  float corr = STEER_SIGN * (Kp*error + Kd*filtD);
  if (corr >  CORR_MAX) corr =  CORR_MAX;
  if (corr < -CORR_MAX) corr = -CORR_MAX;

  // simple adaptive speed: slow down as |error| grows
  int base = BASE_PWM - (int)(fabsf(error) * (BASE_PWM - MIN_PWM));
  if (base < MIN_PWM) base = MIN_PWM;
  if (base > MAX_PWM) base = MAX_PWM;

  int left  = base + (int)corr;
  int right = base - (int)corr;
  if (left  > MAX_PWM) left  = MAX_PWM;  if (left  < -MAX_PWM) left  = -MAX_PWM;
  if (right > MAX_PWM) right = MAX_PWM;  if (right < -MAX_PWM) right = -MAX_PWM;
  setMotors(left, right);
}

void recoveringCycle() {
  if (millis() - recoveryStartMs >= RECOVERY_TIMEOUT_MS) {
    Serial.println(F("[RECOVERY] timeout -> STOPPED"));
    setState(STOPPED);
    return;
  }
  uint16_t pos, conf; uint8_t on;
  if (readLine(pos, conf, on)) {
    prevError = ((float)pos-3500.0f)/3500.0f; filtD=0; dPrimed=true; lostCount=0;
    Serial.println(F("[RECOVERY] line found -> RUNNING"));
    setState(RUNNING);
    return;
  }
  int dir = (lastDir < 0) ? -1 : +1;      // search toward last side
  setMotors(dir>0 ?  RECOVERY_PWM : -RECOVERY_PWM/2,
            dir>0 ? -RECOVERY_PWM/2 :  RECOVERY_PWM);
}

// ======================= UI =======================
void handleChar(char c) {
  switch (c) {
    case 's': startRun(); break;
    case 'x': setState(STOPPED); break;
    case 'c': startCalibration(); break;
    case 't':
      if (state==RUNNING||state==RECOVERING) { Serial.println(F("[TEST] stop first (x).")); break; }
      motorTest(); break;
    case 'd': printDiag(); break;
    case 'p': printPID(); break;
    case 'h': printHelp(); break;
    default: Serial.println(F("[ERR] unknown. 'h' for help.")); break;
  }
}
void applyTune(char k, const char* s) {
  float v = atof(s);
  switch (k) {
    case 'P': if(v>=0&&v<=200){Kp=v;printPID();} break;
    case 'I': if(v>=0&&v<=50){Ki=v;printPID();} break;
    case 'D': if(v>=0&&v<=50){Kd=v;printPID();} break;
    case 'B': if(v>=0&&v<=255){BASE_PWM=(int)v;printPID();} break;
    default: break;
  }
}
void serviceSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c=='\n'||c=='\r') {
      if (cmdLen>0) { cmd[cmdLen]=0;
        if (cmd[0]=='P'||cmd[0]=='I'||cmd[0]=='D'||cmd[0]=='B') applyTune(cmd[0], cmd+1);
        else handleChar(cmd[0]);
        cmdLen=0;
      }
      continue;
    }
    if (cmdLen < sizeof(cmd)-1) cmd[cmdLen++]=c;
    if (cmdLen==1 && (c=='s'||c=='x'||c=='c'||c=='t'||c=='d'||c=='p'||c=='h')) { handleChar(c); cmdLen=0; }
  }
}
void serviceButton() {
  bool raw = digitalRead(BUTTON_PIN) == (BUTTON_ACTIVE_LOW ? LOW : HIGH);
  uint32_t now = millis();
  if (raw != btnLast) { btnLast = raw; btnMs = now; }
  else if (now - btnMs >= 40 && raw != btnStable) {
    btnStable = raw;
    if (btnStable) {                       // debounced press
      if (state==READY) startRun();
      else if (state==RUNNING||state==RECOVERING) setState(STOPPED);
      else startCalibration();
    }
  }
}

// ======================= SETUP / LOOP =======================
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println(F("\n=== ESP32 Line Follower (single-file) ==="));

  pinMode(LEFT_IN1_PIN, OUTPUT); pinMode(LEFT_IN2_PIN, OUTPUT);
  pinMode(RIGHT_IN1_PIN, OUTPUT); pinMode(RIGHT_IN2_PIN, OUTPUT);
  if (STBY_PIN >= 0) { pinMode(STBY_PIN, OUTPUT); digitalWrite(STBY_PIN, LOW); }
  ledcAttach(LEFT_PWM_PIN, PWM_FREQ, PWM_RES);
  ledcAttach(RIGHT_PWM_PIN, PWM_FREQ, PWM_RES);
  ledcWrite(LEFT_PWM_PIN, 0); ledcWrite(RIGHT_PWM_PIN, 0);
  armMotors(false);

  pinMode(BUTTON_PIN, INPUT);

  qtr.setTypeRC();
  qtr.setSensorPins(SENSOR_PINS, SENSOR_COUNT);
  qtr.setTimeout(2500);

  printHelp();
  startCalibration();
}

void loop() {
  uint32_t nowMs = millis();
  serviceSerial();
  serviceButton();

  // Non-blocking calibration window
  if (state == CALIBRATING) {
    if (nowMs - calStartMs < CAL_PREROLL_MS) return;    // 1.5 s pre-roll
    if (nowMs - calStepMs >= 10) { calStepMs = nowMs; qtr.calibrate(); }
    if (nowMs - calStartMs >= CAL_PREROLL_MS + CAL_TIME_MS) {
      Serial.println(F("[CAL] done."));
      if (validateCalibration()) { Serial.println(F("[CAL] PASSED.")); setState(READY); }
      else { Serial.println(F("[CAL] FAILED. Check height/wiring, then 'c'.")); setState(ERR); }
    }
    return;
  }

  // 5 ms control loop
  uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - lastControlUs) >= CONTROL_US) {
    float dt = (nowUs - lastControlUs) / 1e6f;
    if (dt > 0.05f) dt = 0.05f;
    lastControlUs = nowUs;
    if (state == RUNNING)         runningCycle(dt);
    else if (state == RECOVERING) recoveringCycle();
    else                          setMotors(0, 0);
  }
}
