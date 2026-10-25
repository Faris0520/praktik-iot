/*
  ESP32 LINE FOLLOWER - PID + OLED SSD1306 128x64 (I2C)

  Target : ESP32 DevKitC, Arduino-ESP32 core 2.0.x (PlatformIO espressif32 7.0.1)
  Driver : L298N  | Sensor : BFD-1000 (5 kanal) | OLED : SSD1306 tanpa library

  PENTING (perangkat keras):
  - Lepas jumper cap ENA/ENB di modul L298N, karena PWM dikirim dari ESP32.
  - GPIO 22 (SCL) dan GPIO 23 (ENB) bersebelahan. Jangan sampai kabelnya
    bersentuhan, karena ENB yang LOW akan menahan SCL dan I2C mati.

  Tombol:
  - START (pendek)  : jalan / pilih parameter PID (saat mode tuning)
  - START (tahan)   : masuk / keluar mode tuning PID
  - UP / DOWN       : kecepatan, atau ubah nilai Kp/Ki/Kd saat mode tuning
  - STOP            : berhenti
*/
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <stdarg.h>
#include <esp_system.h>

// ===== MOTOR DRIVER L298N =====
#define ENA 25
#define IN1 26
#define IN2 27
#define IN3 18
#define IN4 19
#define ENB 23

// Core 2.x memakai nomor channel LEDC (0-15) pada ledcWrite, bukan nomor pin.
#define PWM_LEFT_CHANNEL  0
#define PWM_RIGHT_CHANNEL 1
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

// ===== OLED I2C 128x64 =====
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// Alamat dicoba otomatis: 0x3C lalu 0x3D.
#define OLED_ADDRESS_1 0x3C
#define OLED_ADDRESS_2 0x3D

// Ubah ke 1 jika OLED 1.3" (chip SH1106). OLED 0.96" (SSD1306) biarkan 0.
#define OLED_IS_SH1106 0

// 100 kHz sudah terbukti stabil di modul ini.
#define OLED_I2C_CLOCK 100000UL

#if OLED_IS_SH1106
#define OLED_COLUMN_OFFSET 2
#else
#define OLED_COLUMN_OFFSET 0
#endif

// Buffer layar dikirim sepotong demi sepotong supaya loop PID tidak tertahan.
// Satu potongan 32 byte butuh sekitar 3-4 ms di 100 kHz (satu halaman penuh
// 128 byte butuh sekitar 14 ms, lebih lama dari periode PID 10 ms).
#define OLED_CHUNK 32
#define OLED_CHUNKS_PER_PAGE (OLED_WIDTH / OLED_CHUNK)
#define OLED_CHUNK_COUNT (8 * OLED_CHUNKS_PER_PAGE)

uint8_t oledBuffer[OLED_WIDTH * OLED_HEIGHT / 8];
uint8_t oledAddress = OLED_ADDRESS_1;
bool oledReady = false;
bool oledDirty = true;
bool wireStarted = false;
int16_t oledFlushChunk = -1; // -1 = idle, 0..OLED_CHUNK_COUNT-1 = potongan berikutnya
unsigned long lastOledRetry = 0;
unsigned long lastOledRefresh = 0;

const unsigned long OLED_RETRY_MS = 2000UL;
const unsigned long OLED_REFRESH_MS = 1000UL;

// ===== SENSOR BFD-1000 =====
// Pin sesuai daftar sambungan: S1=IO32, S2=IO33, S3=VP(IO36), S4=IO35, S5=IO34.
// Posisi di robot, kiri ke kanan: S1, S2, S3, S4, S5 (S2 kiri, S4 kanan).
#define PIN_S1 32
#define PIN_S2 33
#define PIN_S3 36
#define PIN_S4 35
#define PIN_S5 34

const int sensorPins[5] = {PIN_S1, PIN_S2, PIN_S3, PIN_S4, PIN_S5};
// Polaritas terbukti di hardware: S2 hitam menghasilkan belok kiri.
// Karena itu sensor kiri berbobot positif.
const int weights[5] = {2, 1, 0, -1, -2};

#define CLP_PIN  4
#define NEAR_PIN 5

// Asumsi garis hitam menghasilkan LOW. Ubah ke HIGH jika terbalik.
#define LINE_ACTIVE_LEVEL LOW

// ===== PUSH BUTTON =====
#define BTN_START      13
#define BTN_SPEED_UP   14
#define BTN_SPEED_DOWN 16
#define BTN_STOP       17

// ===== PENGATURAN ROBOT DAN PID =====
// Geometri: sensor hanya ~3 cm di depan poros roda, dan ~2.5 cm ke dalam
// dari pinggir roda. Look-ahead sangat pendek, jadi tikungan harus diputar
// di tempat (roda dalam mundur). Kecepatan tinggi tidak sempat menikung.
int baseSpeed = 80;
const int minSpeed = 60;
const int maxSpeed = 160;

float Kp = 35.0f;
float Ki = 2.0f;
float Kd = 0.25f;

const unsigned long PID_SAMPLE_US = 10000UL; // 10 ms
const float INTEGRAL_LIMIT = 1.5f;
const int MAX_CORRECTION = 140;

// Setelah garis hilang, tetap berputar ke arah terakhir sebentar.
// Dengan look-ahead 3 cm, sensor sering lepas di tengah tikungan 90 derajat.
const unsigned long LINE_LOST_PIVOT_MS = 280UL;

// Saat robot berjalan, potongan OLED hanya dikirim dalam jendela ini setelah
// tick PID terakhir, supaya transfer selesai sebelum tick berikutnya.
const unsigned long PID_SAFE_WINDOW_US = 3000UL;

// Robot tidak langsung jalan saat dinyalakan. Tekan START untuk memulai.
bool robotRunning = false;
bool tuningMode = false;
uint8_t selectedPID = 0; // 0=Kp, 1=Ki, 2=Kd

float lastError = 0.0f;
float integral = 0.0f;
float filteredDerivative = 0.0f;
unsigned long lastPIDTime = 0;
unsigned long lastLineMs = 0;

int rawSensors[5];
unsigned long lastPrint = 0;

const unsigned long debounceMs = 35UL;
const unsigned long longPressMs = 800UL;

// #region agent log
#define DBG_SESSION "3fa29d"
#define DBG_RUN "run4"

static void dbgLog(const char *hyp, const char *loc, const char *msg,
                   const char *dataFmt, ...) {
  char data[200];
  va_list args;
  va_start(args, dataFmt);
  vsnprintf(data, sizeof(data), dataFmt, args);
  va_end(args);
  Serial.printf("{\"sessionId\":\"" DBG_SESSION "\",\"runId\":\"" DBG_RUN
                "\",\"hypothesisId\":\"%s\",\"location\":\"%s\",\"message\":\"%s\""
                ",\"data\":{%s},\"timestamp\":%lu}\n",
                hyp, loc, msg, data, (unsigned long)millis());
}
// #endregion

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

// ===== PWM MOTOR: API CORE 2.x =====
void setupPWM() {
  ledcSetup(PWM_LEFT_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_RIGHT_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(ENA, PWM_LEFT_CHANNEL);
  ledcAttachPin(ENB, PWM_RIGHT_CHANNEL);

  ledcWrite(PWM_LEFT_CHANNEL, 0);
  ledcWrite(PWM_RIGHT_CHANNEL, 0);
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

  ledcWrite(PWM_LEFT_CHANNEL, abs(leftSpeed));
  ledcWrite(PWM_RIGHT_CHANNEL, abs(rightSpeed));
}

void stopMotor() {
  setMotor(0, 0);
}

// Putar di tempat. Polaritas terbukti: channel kiri positif = belok kiri.
void pivotTurn(bool toLeft) {
  const int spin = constrain(baseSpeed, 70, 140);
  const int leftSpeed = toLeft ? spin : -spin;
  const int rightSpeed = toLeft ? -spin : spin;
  lastError = toLeft ? 1.0f : -1.0f;
  setMotor(leftSpeed, rightSpeed);
  // #region agent log
  static unsigned long lastTurnLog = 0;
  if (millis() - lastTurnLog >= 250UL) {
    lastTurnLog = millis();
    dbgLog("T", "main.cpp:followLine", "putar di tempat",
           "\"s\":[%d,%d,%d,%d,%d],\"arah\":\"%s\",\"L\":%d,\"R\":%d",
           rawSensors[0], rawSensors[1], rawSensors[2], rawSensors[3], rawSensors[4],
           toLeft ? "kiri" : "kanan", leftSpeed, rightSpeed);
  }
  // #endregion
}

// ===== BACA SENSOR =====
void readSensors() {
  for (int i = 0; i < 5; i++) {
    rawSensors[i] = digitalRead(sensorPins[i]);
  }
}

// ===== PID =====
void resetPIDState() {
  integral = 0.0f;
  filteredDerivative = 0.0f;
  lastError = 0.0f;
  lastPIDTime = micros();
}

void adjustSelectedPID(int direction) {
  if (selectedPID == 0) {
    Kp = constrain(Kp + direction * 0.5f, 0.0f, 100.0f);
  } else if (selectedPID == 1) {
    Ki = constrain(Ki + direction * 0.05f, 0.0f, 10.0f);
  } else {
    Kd = constrain(Kd + direction * 0.05f, 0.0f, 10.0f);
  }
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

  // #region agent log
  {
    // Level mentah tombol, dicatat hanya saat berubah (tanpa debounce).
    static int lastRaw[4] = {-1, -1, -1, -1};
    static const int pins[4] = {BTN_START, BTN_SPEED_UP, BTN_SPEED_DOWN, BTN_STOP};
    static const char *names[4] = {"START", "UP", "DOWN", "STOP"};
    for (int i = 0; i < 4; i++) {
      const int level = digitalRead(pins[i]);
      if (level != lastRaw[i]) {
        lastRaw[i] = level;
        dbgLog("H1", "main.cpp:handleButtons", "level mentah tombol berubah",
               "\"btn\":\"%s\",\"gpio\":%d,\"level\":%d", names[i], pins[i], level);
      }
    }
    if (startEvent != BUTTON_NONE || stopEvent != BUTTON_NONE ||
        upEvent != BUTTON_NONE || downEvent != BUTTON_NONE) {
      dbgLog("H2", "main.cpp:handleButtons", "event tombol terdeteksi",
             "\"start\":%d,\"up\":%d,\"down\":%d,\"stop\":%d,\"note\":\"0=none 1=short 2=long\","
             "\"runningBefore\":%d,\"tuningBefore\":%d",
             (int)startEvent, (int)upEvent, (int)downEvent, (int)stopEvent,
             oldRobotRunning ? 1 : 0, oldTuningMode ? 1 : 0);
    }
  }
  // #endregion

  if (startEvent == BUTTON_LONG) {
    // Masuk atau keluar mode tuning. Robot selalu berhenti saat berpindah mode.
    tuningMode = !tuningMode;
    robotRunning = false;
    stopMotor();
    resetPIDState();
  } else if (startEvent == BUTTON_SHORT) {
    if (tuningMode) {
      selectedPID = (selectedPID + 1) % 3;
    } else {
      robotRunning = true;
      resetPIDState();
    }
  }

  if (stopEvent != BUTTON_NONE) {
    robotRunning = false;
    stopMotor();
  }

  if (upEvent != BUTTON_NONE) {
    if (tuningMode) {
      adjustSelectedPID(1);
    } else {
      baseSpeed = min(baseSpeed + 10, maxSpeed);
    }
  }

  if (downEvent != BUTTON_NONE) {
    if (tuningMode) {
      adjustSelectedPID(-1);
    } else {
      baseSpeed = max(baseSpeed - 10, minSpeed);
    }
  }

  if (baseSpeed != oldBaseSpeed || Kp != oldKp || Ki != oldKi || Kd != oldKd ||
      robotRunning != oldRobotRunning || tuningMode != oldTuningMode ||
      selectedPID != oldSelectedPID) {
    oledDirty = true;
    // #region agent log
    dbgLog("H2", "main.cpp:handleButtons", "status berubah",
           "\"running\":%d,\"tuning\":%d,\"sel\":%u,\"speed\":%d",
           robotRunning ? 1 : 0, tuningMode ? 1 : 0, selectedPID, baseSpeed);
    // #endregion
  }
}

// ===== OLED I2C TANPA LIBRARY TAMBAHAN =====
bool oledCommand(uint8_t command) {
  Wire.beginTransmission(oledAddress);
  Wire.write(0x00); // control byte: command
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

bool oledProbe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool oledBegin() {
  if (!wireStarted) {
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(OLED_I2C_CLOCK);
    wireStarted = true;
  }

  // Cari alamat OLED secara otomatis.
  if (oledProbe(OLED_ADDRESS_1)) {
    oledAddress = OLED_ADDRESS_1;
  } else if (oledProbe(OLED_ADDRESS_2)) {
    oledAddress = OLED_ADDRESS_2;
  } else {
    Serial.println("OLED tidak terdeteksi (0x3C/0x3D). Periksa kabel SDA/SCL, VCC, GND.");
    return false;
  }

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

  Wire.beginTransmission(oledAddress);
  Wire.write(0x00);
  Wire.write(initCommands, sizeof(initCommands));
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("Gagal menginisialisasi OLED, error=%u\n", error);
    return false;
  }

  Serial.printf("OLED siap pada alamat 0x%02X.\n", oledAddress);
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
  static const uint8_t slash[5] = {0x20,0x10,0x08,0x04,0x02};

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
    case '/': return slash;
    default: return blank;
  }
}

// on=true menyalakan piksel, on=false memadamkan (untuk teks terbalik).
void oledDrawPixel(int x, int y, bool on = true) {
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
    return;
  }
  const uint8_t mask = (uint8_t)(1U << (y & 7));
  uint8_t &cell = oledBuffer[x + (y / 8) * OLED_WIDTH];
  if (on) {
    cell |= mask;
  } else {
    cell &= (uint8_t)~mask;
  }
}

void oledDrawChar(int x, int y, char character, bool on = true, int scale = 1) {
  const uint8_t *glyph = oledGlyph(character);
  for (int column = 0; column < 5; column++) {
    for (int row = 0; row < 7; row++) {
      if (glyph[column] & (1U << row)) {
        for (int sx = 0; sx < scale; sx++) {
          for (int sy = 0; sy < scale; sy++) {
            oledDrawPixel(x + column * scale + sx, y + row * scale + sy, on);
          }
        }
      }
    }
  }
}

// Lebar teks dalam piksel (spasi antar huruf 1 kolom, tanpa spasi di ujung).
int oledTextWidth(const char *text, int scale = 1) {
  const int length = (int)strlen(text);
  return length > 0 ? length * 6 * scale - scale : 0;
}

void oledDrawText(int x, int y, const char *text, bool on = true, int scale = 1) {
  while (*text && x < OLED_WIDTH) {
    oledDrawChar(x, y, *text++, on, scale);
    x += 6 * scale;
  }
}

// Teks di tengah area [areaX, areaX + areaW).
void oledDrawTextCentered(int areaX, int areaW, int y, const char *text,
                          bool on = true, int scale = 1) {
  oledDrawText(areaX + (areaW - oledTextWidth(text, scale)) / 2, y, text, on, scale);
}

void oledFillRect(int x, int y, int w, int h, bool on = true) {
  for (int row = 0; row < h; row++) {
    for (int column = 0; column < w; column++) {
      oledDrawPixel(x + column, y + row, on);
    }
  }
}

void oledDrawRect(int x, int y, int w, int h) {
  for (int column = 0; column < w; column++) {
    oledDrawPixel(x + column, y);
    oledDrawPixel(x + column, y + h - 1);
  }
  for (int row = 0; row < h; row++) {
    oledDrawPixel(x, y + row);
    oledDrawPixel(x + w - 1, y + row);
  }
}

// Menggambar ke buffer di RAM saja (tanpa I2C), jadi murah dipanggil kapan pun.
//
//  y 0-9    : bar judul (terbalik)
//  y 13-41  : baris KP/KI/KD (kiri) dan kotak status besar (kanan)
//  y 44-52  : kecepatan + bar indikator
//  y 55-63  : footer, START di kiri bawah dan STOP di kanan bawah
//             (sama dengan posisi tombol fisik di robot)
void oledRender() {
  memset(oledBuffer, 0, sizeof(oledBuffer));

  char text[16];

  // --- Judul ---
  oledFillRect(0, 0, OLED_WIDTH, 10, true);
  oledDrawTextCentered(0, OLED_WIDTH, 1,
                       tuningMode ? "PID TUNING" : "PID CONTROL", false);

  // --- Parameter PID: label rata kiri, angka rata kanan agar titik desimal sejajar ---
  const char *labels[3] = {"KP", "KI", "KD"};
  const float values[3] = {Kp, Ki, Kd};
  for (int i = 0; i < 3; i++) {
    const int y = 14 + i * 10;
    const bool selected = tuningMode && selectedPID == i;

    if (selected) {
      oledFillRect(0, y - 1, 71, 9, true); // sorot baris yang sedang dipilih
    }
    snprintf(text, sizeof(text), "%.2f", values[i]);
    oledDrawText(4, y, labels[i], !selected);
    oledDrawText(67 - oledTextWidth(text), y, text, !selected);
  }

  // --- Kotak status: RUN (terisi), STOP, atau TUNE ---
  const int boxX = 76;
  const int boxY = 13;
  const int boxW = 52;
  const int boxH = 29;
  const bool running = robotRunning && !tuningMode;
  const char *status = tuningMode ? "TUNE" : (running ? "RUN" : "STOP");

  if (running) {
    oledFillRect(boxX, boxY, boxW, boxH, true);
  } else {
    oledDrawRect(boxX, boxY, boxW, boxH);
  }
  oledDrawTextCentered(boxX, boxW, boxY + (boxH - 14) / 2, status, !running, 2);

  // --- Kecepatan: label + angka, lalu bar sesuai rentang minSpeed..maxSpeed ---
  snprintf(text, sizeof(text), "SPD %d", baseSpeed);
  oledDrawText(0, 45, text);

  const int barX = 48;
  const int barW = OLED_WIDTH - barX;
  oledDrawRect(barX, 44, barW, 9);
  const int inner = barW - 4;
  const int filled = constrain(
      (int)((long)(baseSpeed - minSpeed) * inner / (maxSpeed - minSpeed)),
      0, inner);
  oledFillRect(barX + 2, 46, filled, 5, true);

  // --- Footer: label tombol sesuai posisi fisiknya ---
  const int footerY = 55;
  oledFillRect(0, footerY, 36, 9, true);
  oledDrawTextCentered(0, 36, footerY + 1, tuningMode ? "PILIH" : "START", false);

  oledFillRect(OLED_WIDTH - 36, footerY, 36, 9, true);
  oledDrawTextCentered(OLED_WIDTH - 36, 36, footerY + 1, "STOP", false);

  oledDrawTextCentered(36, OLED_WIDTH - 72, footerY + 1,
                       tuningMode ? "UP/DN" : "HOLD:PID");
}

// Mengirim satu potongan OLED_CHUNK byte ke layar.
bool oledSendChunk(uint8_t chunk) {
  const uint8_t page = chunk / OLED_CHUNKS_PER_PAGE;
  const uint8_t column = (chunk % OLED_CHUNKS_PER_PAGE) * OLED_CHUNK;

  // Alamat halaman dan kolom hanya diset di awal tiap halaman, karena pointer
  // kolom SSD1306 maju sendiri setelah setiap byte data.
  if (column == 0) {
    if (!oledCommand(0xB0 + page)) return false;
    if (!oledCommand(0x00 | (OLED_COLUMN_OFFSET & 0x0F))) return false;
    if (!oledCommand(0x10 | (OLED_COLUMN_OFFSET >> 4))) return false;
  }

  Wire.beginTransmission(oledAddress);
  Wire.write(0x40); // control byte: display data
  Wire.write(&oledBuffer[page * OLED_WIDTH + column], OLED_CHUNK);
  return Wire.endTransmission() == 0;
}

// Dipanggil tiap iterasi loop. Mengurus init ulang, refresh berkala, dan
// mengirim buffer sepotong demi sepotong supaya PID tidak tertahan.
void oledService() {
  const unsigned long now = millis();

  if (!oledReady) {
    // Jangan probe saat robot berjalan: probe yang gagal bisa menahan loop PID.
    if (!robotRunning && now - lastOledRetry >= OLED_RETRY_MS) {
      lastOledRetry = now;
      oledReady = oledBegin();
      if (oledReady) {
        oledDirty = true;
        oledFlushChunk = -1;
        lastOledRefresh = now;
      }
    }
    return;
  }

  // Refresh berkala hanya saat robot diam, untuk memulihkan layar yang glitch.
  if (!robotRunning && now - lastOledRefresh >= OLED_REFRESH_MS) {
    lastOledRefresh = now;
    oledDirty = true;
  }

  if (oledDirty && oledFlushChunk < 0) {
    oledRender();
    oledDirty = false;
    oledFlushChunk = 0;
  }

  if (oledFlushChunk < 0) {
    return;
  }

  // Saat robot berjalan, kirim hanya di awal jendela PID.
  if (robotRunning && micros() - lastPIDTime > PID_SAFE_WINDOW_US) {
    return;
  }

  if (!oledSendChunk((uint8_t)oledFlushChunk)) {
    // Error I2C (kabel kendor atau noise motor). Coba init ulang nanti.
    oledReady = false;
    oledFlushChunk = -1;
    Serial.println("Gagal mengirim data ke OLED. Akan dicoba inisialisasi ulang.");
    return;
  }

  oledFlushChunk++;
  if (oledFlushChunk >= OLED_CHUNK_COUNT) {
    oledFlushChunk = -1;
  }
}

// ===== LOGIKA LINE FOLLOWER PID =====
void followLine() {
  const unsigned long now = micros();

  // Jalankan PID setiap 10 ms.
  if (now - lastPIDTime < PID_SAMPLE_US) {
    return;
  }

  const float dt = (now - lastPIDTime) / 1000000.0f;
  lastPIDTime = now;

  int weightedSum = 0;
  int detectedCount = 0;

  for (int i = 0; i < 5; i++) {
    if (rawSensors[i] == LINE_ACTIVE_LEVEL) {
      weightedSum += weights[i];
      detectedCount++;
    }
  }

  const bool leftBlack = (rawSensors[0] == LINE_ACTIVE_LEVEL) ||
                         (rawSensors[1] == LINE_ACTIVE_LEVEL);
  const bool rightBlack = (rawSensors[3] == LINE_ACTIVE_LEVEL) ||
                          (rawSensors[4] == LINE_ACTIVE_LEVEL);

  if (detectedCount == 0) {
    integral = 0.0f;
    filteredDerivative = 0.0f;
    if (lastError != 0.0f && millis() - lastLineMs < LINE_LOST_PIVOT_MS) {
      pivotTurn(lastError > 0.0f);
    } else {
      stopMotor();
      lastError = 0.0f;
      // #region agent log
      static unsigned long lastLostLog = 0;
      if (millis() - lastLostLog >= 500UL) {
        lastLostLog = millis();
        dbgLog("H3", "main.cpp:followLine", "robot RUN tapi tidak ada sensor di garis, motor distop",
               "\"s\":[%d,%d,%d,%d,%d],\"activeLevel\":%d",
               rawSensors[0], rawSensors[1], rawSensors[2], rawSensors[3], rawSensors[4],
               (int)LINE_ACTIVE_LEVEL);
      }
      // #endregion
    }
    return;
  }

  lastLineMs = millis();

  // Garis hanya di kiri (S1/S2) atau hanya di kanan (S4/S5): putar di tempat.
  // Zona buta ~2.5 cm antara sensor terluar dan roda, jadi belok harus cepat.
  if (leftBlack != rightBlack) {
    pivotTurn(leftBlack);
    return;
  }

  const float error = (float)weightedSum / detectedCount;

  integral += error * dt;
  integral = constrain(integral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

  const float derivative = (error - lastError) / dt;
  const float alpha = 0.7f;
  filteredDerivative = alpha * filteredDerivative
                     + (1.0f - alpha) * derivative;

  const float output = Kp * error + Ki * integral + Kd * filteredDerivative;
  lastError = error;

  const int correction = constrain((int)output, -MAX_CORRECTION, MAX_CORRECTION);
  // Izinkan roda dalam mundur. Dengan look-ahead 3 cm, batas 0 membuat
  // radius belok lebih besar dari jarak sensor, jadi tikungan terlewat.
  const int leftSpeed = constrain(baseSpeed + correction, -255, 255);
  const int rightSpeed = constrain(baseSpeed - correction, -255, 255);

  setMotor(leftSpeed, rightSpeed);

  // #region agent log
  static unsigned long lastRunLog = 0;
  if (millis() - lastRunLog >= 250UL) {
    lastRunLog = millis();
    dbgLog("H3H5", "main.cpp:followLine", "PID menggerakkan motor",
           "\"s\":[%d,%d,%d,%d,%d],\"count\":%d,\"err\":%.2f,\"corr\":%d,"
           "\"L\":%d,\"R\":%d,\"dutyL\":%u,\"dutyR\":%u,\"in1\":%d,\"in2\":%d,\"in3\":%d,\"in4\":%d",
           rawSensors[0], rawSensors[1], rawSensors[2], rawSensors[3], rawSensors[4],
           detectedCount, error, correction, leftSpeed, rightSpeed,
           ledcRead(PWM_LEFT_CHANNEL), ledcRead(PWM_RIGHT_CHANNEL),
           digitalRead(IN1), digitalRead(IN2), digitalRead(IN3), digitalRead(IN4));
  }
  // #endregion
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(300); // beri waktu daya stabil dan Serial Monitor siap

  // #region agent log
  {
    const esp_reset_reason_t reason = esp_reset_reason();
    const char *name =
        reason == ESP_RST_POWERON  ? "POWERON" :
        reason == ESP_RST_EXT      ? "EXT_RESET_PIN" :
        reason == ESP_RST_SW       ? "SOFTWARE" :
        reason == ESP_RST_PANIC    ? "PANIC_CRASH" :
        reason == ESP_RST_INT_WDT  ? "INT_WATCHDOG" :
        reason == ESP_RST_TASK_WDT ? "TASK_WATCHDOG" :
        reason == ESP_RST_WDT      ? "WATCHDOG" :
        reason == ESP_RST_DEEPSLEEP? "DEEPSLEEP" :
        reason == ESP_RST_BROWNOUT ? "BROWNOUT" : "LAINNYA";
    dbgLog("H4", "main.cpp:setup", "alasan reset terakhir",
           "\"reason\":\"%s\",\"code\":%d", name, (int)reason);
  }
  // #endregion

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

  resetPIDState();

  delay(100); // beri waktu OLED menyala sebelum diinisialisasi
  oledReady = oledBegin();
  lastOledRetry = millis();
  lastOledRefresh = millis();
  oledDirty = true;

  Serial.println();
  Serial.println("=== ESP32 LINE FOLLOWER PID + OLED ===");
  Serial.println("START: jalan | STOP: berhenti");
  Serial.println("Tahan START 0.8 detik: masuk/keluar mode tuning PID");
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

  oledService();

  if (millis() - lastPrint >= 500UL) {
    lastPrint = millis();
    Serial.printf("S1..S5: %d%d%d%d%d | CLP:%d NEAR:%d | SPD:%d | Kp/Ki/Kd: %.2f/%.2f/%.2f | OLED:%s | %s\n",
                  rawSensors[0], rawSensors[1], rawSensors[2],
                  rawSensors[3], rawSensors[4],
                  digitalRead(CLP_PIN), digitalRead(NEAR_PIN),
                  baseSpeed, Kp, Ki, Kd,
                  oledReady ? "OK" : "OFF",
                  tuningMode ? "TUNING" : robotRunning ? "RUNNING" : "STOPPED");
  }

  delay(1);
}
