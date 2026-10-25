#include <Arduino.h>
#define PWM_PIN 2
#define FREQ 5000 // 5kHz
#define RES 8 // 8 bit
#define PWM_CHANNEL 0

int dutyCycle = 0;

void setup() {
  pinMode(PWM_PIN, OUTPUT);
  ledcSetup(PWM_CHANNEL, FREQ, RES);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
}

void loop() {
  for (dutyCycle = 0; dutyCycle < (1 << RES); dutyCycle++) {
    ledcWrite(PWM_CHANNEL, dutyCycle);
    delay(10);
  }
}