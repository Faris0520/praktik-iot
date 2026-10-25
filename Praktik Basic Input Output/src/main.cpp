#include <Arduino.h>
#define input 15
#define red 23
#define output 22


void setup() {
  // put your setup code here, to run once:
  pinMode(input, INPUT_PULLDOWN);
  pinMode(output, OUTPUT);
  pinMode(red,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  int inputState = digitalRead(input);
  if (inputState == 1){
    digitalWrite(output, HIGH);
    digitalWrite(red, LOW);
  }
  else if (inputState == 0){
    digitalWrite(output, LOW);
    digitalWrite(red, HIGH);
  }
}