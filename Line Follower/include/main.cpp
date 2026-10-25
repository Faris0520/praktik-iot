#include <Arduino.h>

// ===== MOTOR DRIVER L298N =====
#define ENA 25
#define IN1 26
#define IN2 27
#define IN3 18
#define IN4 19
#define ENB 23

// PWM channel untuk ESP32 Arduino Core 2.x
#define PWM_LEFT_CHANNEL  0
#define PWM_RIGHT_CHANNEL 1
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

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
#define BTN_START    13
#define BTN_SPEED_UP 14
#define BTN_SPEED_DOWN 16
#define BTN_STOP     17

// ===== PENGATURAN ROBOT =====
int baseSpeed = 100;
const int minSpeed = 70;
const int maxSpeed = 200;
const float Kp = 45.0;

bool robotRunning = true;
float lastError = 0;

int rawSensors[5];

bool lastStart = HIGH;
bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastStop = HIGH;

unsigned long timeStart = 0;
unsigned long timeUp = 0;
unsigned long timeDown = 0;
unsigned long timeStop = 0;
unsigned long lastPrint = 0;

const unsigned long debounceMs = 35;

// ===== PWM MOTOR =====
void setupPWM() {
  ledcSetup(PWM_LEFT_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_RIGHT_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(ENA, PWM_LEFT_CHANNEL);
  ledcAttachPin(ENB, PWM_RIGHT_CHANNEL);
}

// ===== KONTROL MOTOR =====
// speed positif = maju, negatif = mundur
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

// ===== BACA SENSOR =====
void readSensors() {
  for (int i = 0; i < 5; i++) {
    rawSensors[i] = digitalRead(sensorPins[i]);
  }
}

// ===== DEBOUNCE PUSH BUTTON =====
bool buttonPressed(
  int pin,
  bool &lastState,
  unsigned long &lastChange
) {
  bool reading = digitalRead(pin);

  if (reading != lastState &&
      millis() - lastChange >= debounceMs) {
    lastChange = millis();
    lastState = reading;

    if (reading == LOW) {
      return true;
    }
  }

  return false;
}

// ===== LOGIKA LINE FOLLOWER =====
void followLine() {
  int weightedSum = 0;
  int detectedCount = 0;

  // Semua sensor bernilai 1 (putih) -> berhenti
  for (int i = 0; i < 5; i++) {
    if (rawSensors[i] == LOW) {
      detectedCount++;
    }
  }

  if (detectedCount == 0) {
    stopMotor();
    return;
  }

  // Urutan array: S5, S4, S3, S2, S1
  // S2, S3, S4 bernilai 0 (hitam) -> lurus
  if (rawSensors[3] == LOW &&
      rawSensors[2] == LOW &&
      rawSensors[1] == LOW) {
    setMotor(baseSpeed, baseSpeed);
    return;
  }

  // Menghitung posisi garis untuk koreksi arah
  for (int i = 0; i < 5; i++) {
    if (rawSensors[i] == LOW) {
      weightedSum += weights[i];
    }
  }

  float error = (float)weightedSum / detectedCount;
  int correction = (int)(Kp * error);

  int leftSpeed  = baseSpeed + correction;
  int rightSpeed = baseSpeed - correction;

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

  Serial.println();
  Serial.println("=== ESP32 LINE FOLLOWER TEST ===");
  Serial.println("BTN 1: START");
  Serial.println("BTN 2: SPEED UP");
  Serial.println("BTN 3: SPEED DOWN");
  Serial.println("BTN 4: STOP");
  Serial.println("Sensor ditampilkan dari kiri (S5) ke kanan (S1).");
}

// ===== LOOP =====
void loop() {
  if (buttonPressed(BTN_START, lastStart, timeStart)) {
    robotRunning = true;
    Serial.println("Robot START");
  }

  if (buttonPressed(BTN_STOP, lastStop, timeStop)) {
    robotRunning = false;
    stopMotor();
    Serial.println("Robot STOP");
  }

  if (buttonPressed(BTN_SPEED_UP, lastUp, timeUp)) {
    baseSpeed = min(baseSpeed + 10, maxSpeed);
    Serial.printf("Kecepatan: %d\n", baseSpeed);
  }

  if (buttonPressed(BTN_SPEED_DOWN, lastDown, timeDown)) {
    baseSpeed = max(baseSpeed - 10, minSpeed);
    Serial.printf("Kecepatan: %d\n", baseSpeed);
  }

  readSensors();

  if (robotRunning) {
    followLine();
  } else {
    stopMotor();
  }

  // Tampilkan data sensor di Serial Monitor
  if (millis() - lastPrint >= 150) {
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

    Serial.print(" | Status: ");
    Serial.println(robotRunning ? "RUNNING" : "STOPPED");
  }

  delay(2);
}

// =========================== TES SENSOR
// #include <Arduino.h>

// // Lima sensor garis
// const uint8_t sensorPins[5] = {
//     32, 33, 25, 26, 27
// };

// // Sensor tambahan
// const uint8_t CLP_PIN  = 14;
// const uint8_t NEAR_PIN = 13;

// void setup() {
//     Serial.begin(115200);

//     for (int i = 0; i < 5; i++) {
//         pinMode(sensorPins[i], INPUT);
//     }

//     pinMode(CLP_PIN, INPUT);
//     pinMode(NEAR_PIN, INPUT);

//     Serial.println("\n=== TEST IR SENSOR 5 CHANNEL ===");
// }

// void loop() {
//     // Baca sensor garis
//     for (int i = 0; i < 5; i++) {
//         Serial.print("S");
//         Serial.print(i + 1);
//         Serial.print("=");
//         Serial.print(digitalRead(sensorPins[i]));
//         Serial.print("  ");
//     }

//     // Baca sensor benturan dan jarak
//     Serial.print("| CLP=");
//     Serial.print(digitalRead(CLP_PIN));

//     Serial.print(" | NEAR=");
//     Serial.println(digitalRead(NEAR_PIN));

//     delay(300);
// }

// ================================= TES MOTOR DRIVER
// #include <Arduino.h>

// // Motor A
// #define ENA 25
// #define IN1 26
// #define IN2 27

// // Motor B
// #define ENB 33
// #define IN3 18
// #define IN4 19

// #define PWM_CHANNEL_A 0
// #define PWM_CHANNEL_B 1

// #define PWM_FREQ 1000
// #define PWM_RESOLUTION 8

// void stopMotor() {
//   digitalWrite(IN1, LOW);
//   digitalWrite(IN2, LOW);
//   digitalWrite(IN3, LOW);
//   digitalWrite(IN4, LOW);

//   ledcWrite(PWM_CHANNEL_A, 0);
//   ledcWrite(PWM_CHANNEL_B, 0);
// }

// void maju(int speed) {
//   digitalWrite(IN1, LOW);
//   digitalWrite(IN2, HIGH);

//   digitalWrite(IN3, LOW);
//   digitalWrite(IN4, HIGH);

//   ledcWrite(PWM_CHANNEL_A, speed);
//   ledcWrite(PWM_CHANNEL_B, speed);
//   // digitalWrite(IN1, HIGH);
//   // digitalWrite(IN2, LOW);

//   // digitalWrite(IN3, HIGH);
//   // digitalWrite(IN4, LOW);

//   // ledcWrite(PWM_CHANNEL_A, speed);
//   // ledcWrite(PWM_CHANNEL_B, speed);
// }

// void mundur(int speed) {
//   digitalWrite(IN1, HIGH);
//   digitalWrite(IN2, LOW);

//   digitalWrite(IN3, HIGH);
//   digitalWrite(IN4, LOW);

//   ledcWrite(PWM_CHANNEL_A, speed);
//   ledcWrite(PWM_CHANNEL_B, speed);
//   // digitalWrite(IN1, LOW);
//   // digitalWrite(IN2, HIGH);

//   // digitalWrite(IN3, LOW);
//   // digitalWrite(IN4, HIGH);

//   // ledcWrite(PWM_CHANNEL_A, speed);
//   // ledcWrite(PWM_CHANNEL_B, speed);
// }

// void setup() {
//   Serial.begin(115200);

//   pinMode(IN1, OUTPUT);
//   pinMode(IN2, OUTPUT);
//   pinMode(IN3, OUTPUT);
//   pinMode(IN4, OUTPUT);

//   // Konfigurasi PWM untuk Arduino-ESP32 Core 2.x
//   ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
//   ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);

//   ledcAttachPin(ENA, PWM_CHANNEL_A);
//   ledcAttachPin(ENB, PWM_CHANNEL_B);

//   stopMotor();

//   Serial.println("Tes Motor Driver L298N");
// }

// void loop() {
//   Serial.println("Motor MAJU");
//   maju(180);
//   delay(2000);

//   Serial.println("Motor BERHENTI");
//   stopMotor();
//   delay(1000);

//   Serial.println("Motor MUNDUR");
//   mundur(180);
//   delay(2000);

//   Serial.println("Motor BERHENTI");
//   stopMotor();
//   delay(2000);
// }
