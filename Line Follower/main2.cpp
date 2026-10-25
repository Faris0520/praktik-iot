/*
    PID nya jalan, tapi oled nya mati
*/
#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// ===== MOTOR DRIVER L298N =====
#define ENA 25
#define IN1 26
#define IN2 27
#define IN3 18
#define IN4 19
#define ENB 23

// PWM untuk Arduino-ESP32 Core 3.x
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

// ===== OLED SSD1306 I2C 128x64 =====
// Setelan umum ESP32: SDA=21, SCL=22, alamat OLED=0x3C.
// Ubah konstanta ini jika modul Anda memakai pin/alamat berbeda.
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDRESS 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

uint8_t oledBuffer[OLED_WIDTH * OLED_HEIGHT / 8];
bool oledReady = false;
bool oledDirty = true;

// ===== SENSOR BFD-1000 =====
// Urutan fisik kiri ke kanan: S5, S4, S3, S2, S1
const int sensorPins[5] = {34, 35, 36, 33, 32};
// Urutan array: S5, S4, S3, S2, S1
const int weights[5] = {-2, -1, 0, 1, 2};

#define CLP_PIN  4
#define NEAR_PIN 5

// Asumsi garis hitam menghasilkan LOW.
// Ubah menjadi HIGH jika hasil pembacaan sensor terbalik.
#define LINE_ACTIVE_LEVEL LOW

// ===== PUSH BUTTON =====
#define BTN_START      13
#define BTN_SPEED_UP   14
#define BTN_SPEED_DOWN 16
#define BTN_STOP       17

// ===== PENGATURAN ROBOT DAN PID =====
int baseSpeed = 100;
const int minSpeed = 70;
const int maxSpeed = 200;

float Kp = 35.0f;
float Ki = 2.0f;
float Kd = 0.25f;

const unsigned long PID_SAMPLE_US = 10000UL; // 10 ms
const float INTEGRAL_LIMIT = 1.5f;
const int MAX_CORRECTION = 90;

bool robotRunning = true;
bool tuningMode = false;
uint8_t selectedPID = 0; // 0=Kp, 1=Ki, 2=Kd

float lastError = 0.0f;
float integral = 0.0f;
float filteredDerivative = 0.0f;
unsigned long lastPIDTime = 0;

int rawSensors[5];
unsigned long lastPrint = 0;

const unsigned long debounceMs = 35UL;
const unsigned long longPressMs = 800UL;

// ===== STATE TOMBOL =====
enum ButtonEvent : uint8_t {
  BUTTON_NONE,
  BUTTON_SHORT,
  BUTTON_LONG
};

struct ButtonState {
  bool lastReading = HIGH;
  bool stableState = HIGH;
  bool longPressSent = false;
  unsigned long lastChange = 0;
  unsigned long pressedAt = 0;
};

ButtonState startButton;
ButtonState upButton;
ButtonState downButton;
ButtonState stopButton;

ButtonEvent readButtonEvent(int pin, ButtonState &button) {
  const unsigned long now = millis();
  const bool reading = digitalRead(pin);

  if (reading != button.lastReading) {
    button.lastReading = reading;
    button.lastChange = now;
  }

  if (now - button.lastChange >= debounceMs &&
      reading != button.stableState) {
    button.stableState = reading;

    if (reading == LOW) {
      button.pressedAt = now;
      button.longPressSent = false;
    } else {
      button.pressedAt = 0;
      if (!button.longPressSent) {
        return BUTTON_SHORT;
      }
      button.longPressSent = false;
    }
  }

  if (button.stableState == LOW && !button.longPressSent &&
      now - button.pressedAt >= longPressMs) {
    button.longPressSent = true;
    return BUTTON_LONG;
  }

  return BUTTON_NONE;
}

// ===== PWM MOTOR: API CORE 3.x =====
bool setupPWM() {
  bool leftAttached = ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
  bool rightAttached = ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);

  if (!leftAttached || !rightAttached) {
    Serial.println("Gagal mengaktifkan PWM LEDC pada pin motor.");
    return false;
  }

  return true;
}

// ===== KONTROL MOTOR =====
// Kecepatan positif = maju, negatif = mundur.
void setMotor(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // Motor kiri
  if (leftSpeed > 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else if (leftSpeed < 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  }

  // Motor kanan
  if (rightSpeed > 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else if (rightSpeed < 0) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
  }

  // Core 3.x menerima nomor pin, bukan nomor channel.
  ledcWrite(ENA, abs(leftSpeed));
  ledcWrite(ENB, abs(rightSpeed));
}

void stopMotor() {
  setMotor(0, 0);
}

// ===== BACA SENSOR =====
void readSensors() {
  for (int i = 0; i < 5; i++) {
    rawSensors[i] = digitalRead(sensorPins[i]);
  }
}

// ===== PID TUNING DENGAN TOMBOL =====
void adjustSelectedPID(int direction) {
  if (selectedPID == 0) {
    Kp = constrain(Kp + direction * 0.5f, 0.0f, 100.0f);
  } else if (selectedPID == 1) {
    Ki = constrain(Ki + direction * 0.05f, 0.0f, 10.0f);
  } else {
    Kd = constrain(Kd + direction * 0.05f, 0.0f, 10.0f);
  }

  Serial.printf("PID: Kp=%.2f Ki=%.2f Kd=%.2f\n", Kp, Ki, Kd);
}

void handleButtons() {
  const int oldBaseSpeed = baseSpeed;
  const float oldKp = Kp;
  const float oldKi = Ki;
  const float oldKd = Kd;
  const bool oldRobotRunning = robotRunning;
  const bool oldTuningMode = tuningMode;
  const uint8_t oldSelectedPID = selectedPID;

  ButtonEvent startEvent = readButtonEvent(BTN_START, startButton);
  ButtonEvent upEvent = readButtonEvent(BTN_SPEED_UP, upButton);
  ButtonEvent downEvent = readButtonEvent(BTN_SPEED_DOWN, downButton);
  ButtonEvent stopEvent = readButtonEvent(BTN_STOP, stopButton);

  if (startEvent == BUTTON_LONG) {
    tuningMode = !tuningMode;

    if (tuningMode) {
      // Robot berhenti saat masuk menu tuning.
      robotRunning = false;
      stopMotor();
      integral = 0.0f;
      filteredDerivative = 0.0f;
      lastError = 0.0f;
      lastPIDTime = micros();
      Serial.println("Mode tuning PID. START pilih Kp/Ki/Kd.");
    } else {
      // Keluar menu tetap dalam keadaan berhenti; tekan START untuk jalan.
      robotRunning = false;
      stopMotor();
      Serial.println("Keluar mode tuning PID.");
    }
  } else if (startEvent == BUTTON_SHORT) {
    if (tuningMode) {
      selectedPID = (selectedPID + 1) % 3;
      Serial.printf("Parameter dipilih: %s\n",
                    selectedPID == 0 ? "Kp" : selectedPID == 1 ? "Ki" : "Kd");
    } else {
      robotRunning = true;
      Serial.println("Robot START");
    }
  }

  if (stopEvent != BUTTON_NONE) {
    robotRunning = false;
    stopMotor();
    Serial.println("Robot STOP");
  }

  if (upEvent != BUTTON_NONE) {
    if (tuningMode) {
      adjustSelectedPID(1);
    } else {
      baseSpeed = min(baseSpeed + 10, maxSpeed);
      Serial.printf("Kecepatan: %d\n", baseSpeed);
    }
  }

  if (downEvent != BUTTON_NONE) {
    if (tuningMode) {
      adjustSelectedPID(-1);
    } else {
      baseSpeed = max(baseSpeed - 10, minSpeed);
      Serial.printf("Kecepatan: %d\n", baseSpeed);
    }
  }

  if (baseSpeed != oldBaseSpeed || Kp != oldKp || Ki != oldKi || Kd != oldKd ||
      robotRunning != oldRobotRunning || tuningMode != oldTuningMode ||
      selectedPID != oldSelectedPID) {
    oledDirty = true;
  }
}

// ===== OLED SSD1306 TANPA LIBRARY TAMBAHAN =====
void oledCommand(uint8_t command) {
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00); // control byte: command
  Wire.write(command);
  Wire.endTransmission();
}

bool oledBegin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);

  Wire.beginTransmission(OLED_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("OLED tidak terdeteksi. Periksa alamat dan kabel I2C.");
    return false;
  }
  Serial.printf("OLED merespons pada alamat 0x%02X.\n", OLED_ADDRESS);

  const uint8_t initCommands[] = {
    0xAE,       // display off
    0xD5, 0x80, // clock divide
    0xA8, 0x3F, // multiplex 1/64
    0xD3, 0x00, // display offset
    0x40,       // start line
    0x8D, 0x14, // charge pump
    0x20, 0x02, // page addressing mode
    0xA1,       // segment remap
    0xC8,       // COM scan direction
    0xDA, 0x12, // COM pins
    0x81, 0xCF, // contrast
    0xD9, 0xF1, // precharge
    0xDB, 0x40, // VCOM detect
    0xA4,       // display follows RAM
    0xA6,       // normal display
    0xAF        // display on
  };

  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(0x00);
  Wire.write(initCommands, sizeof(initCommands));
  if (Wire.endTransmission() != 0) {
    Serial.println("Gagal menginisialisasi OLED.");
    return false;
  }

  return true;
}

const uint8_t *oledGlyph(char character) {
  static const uint8_t letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
  };

  static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
  };

  static const uint8_t blank[5] = {0,0,0,0,0};
  static const uint8_t colon[5] = {0,0x36,0x36,0,0};
  static const uint8_t period[5] = {0,0x60,0x60,0,0};
  static const uint8_t greater[5] = {0x00,0x41,0x22,0x14,0x08};
  static const uint8_t minus[5] = {0x08,0x08,0x08,0x08,0x08};

  if (character >= 'A' && character <= 'Z') {
    return letters[character - 'A'];
  }
  if (character >= 'a' && character <= 'z') {
    return letters[character - 'a'];
  }
  if (character >= '0' && character <= '9') {
    return digits[character - '0'];
  }

  switch (character) {
    case ':': return colon;
    case '.': return period;
    case '>': return greater;
    case '-': return minus;
    default: return blank;
  }
}

void oledDrawPixel(int x, int y) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
    return;
  }
  oledBuffer[x + (y / 8) * OLED_WIDTH] |= (uint8_t)(1U << (y & 7));
}

void oledDrawChar(int x, int y, char character) {
  const uint8_t *glyph = oledGlyph(character);
  for (int column = 0; column < 5; column++) {
    for (int row = 0; row < 7; row++) {
      if (glyph[column] & (1U << row)) {
        oledDrawPixel(x + column, y + row);
      }
    }
  }
}

void oledDrawText(int x, int y, const char *text) {
  while (*text && x < OLED_WIDTH) {
    oledDrawChar(x, y, *text++);
    x += 6;
  }
}

void oledShowBuffer() {
  for (uint8_t page = 0; page < 8; page++) {
    oledCommand(0xB0 + page);
    oledCommand(0x00);
    oledCommand(0x10);

    for (uint8_t column = 0; column < OLED_WIDTH; column += 28) {
      uint8_t count = min((int)28, OLED_WIDTH - column);
      Wire.beginTransmission(OLED_ADDRESS);
      Wire.write(0x40); // control byte: display data
      for (uint8_t i = 0; i < count; i++) {
        Wire.write(oledBuffer[page * OLED_WIDTH + column + i]);
      }
      Wire.endTransmission();
    }
  }
}

void updateOLED() {
  if (!oledReady) {
    return;
  }

  memset(oledBuffer, 0, sizeof(oledBuffer));

  oledDrawText(0, 0, tuningMode ? "PID TUNING" : "PID CONTROL");

  char line[20];
  snprintf(line, sizeof(line), "%cKP:%.2f", selectedPID == 0 ? '>' : ' ', Kp);
  oledDrawText(0, 12, line);
  snprintf(line, sizeof(line), "%cKI:%.2f", selectedPID == 1 ? '>' : ' ', Ki);
  oledDrawText(0, 22, line);
  snprintf(line, sizeof(line), "%cKD:%.2f", selectedPID == 2 ? '>' : ' ', Kd);
  oledDrawText(0, 32, line);

  snprintf(line, sizeof(line), "SPD:%d", baseSpeed);
  oledDrawText(0, 44, line);
  oledDrawText(78, 44, robotRunning ? "RUN" : "STOP");

  oledDrawText(0, 56, tuningMode ? "START SEL UP/DN" : "HOLD START TUNE");

  oledShowBuffer();
  oledDirty = false;
}

// ===== LOGIKA LINE FOLLOWER PID =====
void followLine() {
  unsigned long now = micros();

  // Jalankan PID setiap 10 ms.
  if (now - lastPIDTime < PID_SAMPLE_US) {
    return;
  }

  float dt = (now - lastPIDTime) / 1000000.0f;
  lastPIDTime = now;

  int weightedSum = 0;
  int detectedCount = 0;

  for (int i = 0; i < 5; i++) {
    if (rawSensors[i] == LINE_ACTIVE_LEVEL) {
      weightedSum += weights[i];
      detectedCount++;
    }
  }

  // Semua sensor putih: berhenti dan reset komponen PID.
  if (detectedCount == 0) {
    stopMotor();
    integral = 0.0f;
    filteredDerivative = 0.0f;
    lastError = 0.0f;
    return;
  }

  float error = (float)weightedSum / detectedCount;

  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

  float derivative = (error - lastError) / dt;
  const float alpha = 0.7f;
  filteredDerivative = alpha * filteredDerivative
                     + (1.0f - alpha) * derivative;

  float output = Kp * error + Ki * integral + Kd * filteredDerivative;
  lastError = error;

  int correction = constrain((int)output, -MAX_CORRECTION, MAX_CORRECTION);
  int leftSpeed = constrain(baseSpeed + correction, 0, 255);
  int rightSpeed = constrain(baseSpeed - correction, 0, 255);

  setMotor(leftSpeed, rightSpeed);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  setupPWM();
  stopMotor();

  for (int i = 0; i < 5; i++) {
    pinMode(sensorPins[i], INPUT);
  }

  pinMode(CLP_PIN, INPUT);
  pinMode(NEAR_PIN, INPUT);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_SPEED_UP, INPUT_PULLUP);
  pinMode(BTN_SPEED_DOWN, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);

  lastPIDTime = micros();
  oledReady = oledBegin();
  updateOLED();

  Serial.println();
  Serial.println("=== ESP32 LINE FOLLOWER PID ===");
  Serial.println("Tekan START: mulai; STOP: berhenti.");
  Serial.println("Tahan START 0.8 detik: masuk/keluar menu PID.");
  Serial.println("Di menu PID: START pilih parameter, UP/DOWN ubah nilai.");
}

// ===== LOOP =====
void loop() {
  handleButtons();
  readSensors();

  if (robotRunning && !tuningMode) {
    followLine();
  } else {
    stopMotor();
  }

  if (millis() - lastPrint >= 150UL) {
    lastPrint = millis();

    Serial.print("S5 S4 S3 S2 S1: ");
    for (int i = 0; i < 5; i++) {
      Serial.print(rawSensors[i]);
      Serial.print(" ");
    }

    Serial.print("| CLP: ");
    Serial.print(digitalRead(CLP_PIN));
    Serial.print(" | NEAR: ");
    Serial.print(digitalRead(NEAR_PIN));
    Serial.print(" | Speed: ");
    Serial.print(baseSpeed);
    Serial.print(" | Kp/Ki/Kd: ");
    Serial.print(Kp, 2);
    Serial.print("/");
    Serial.print(Ki, 2);
    Serial.print("/");
    Serial.print(Kd, 2);
    Serial.print(" | Status: ");
    Serial.println(tuningMode ? "PID TUNING" : robotRunning ? "RUNNING" : "STOPPED");
  }

  if (oledReady && oledDirty) {
    updateOLED();
  }

  delay(2);
}
