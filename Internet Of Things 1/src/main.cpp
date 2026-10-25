#include <Arduino.h>
#define merah 23
#define kuning 22
#define hijau 21

void setup() {
  // put your setup code here, to run once:
  pinMode(merah, OUTPUT);
  pinMode(kuning, OUTPUT);
  pinMode(hijau, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(merah, HIGH); //nyala
  delay(1000);
  digitalWrite(merah, LOW); //mati
  digitalWrite(kuning, HIGH); //nyala
  delay(500);
  digitalWrite(kuning, LOW);
  digitalWrite(hijau, HIGH); //nyala
  delay(1000);
  digitalWrite(hijau, LOW); //mati
}