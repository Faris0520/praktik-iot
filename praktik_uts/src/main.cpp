#include "DHTesp.h"
#include "Wire.h"
#define LED_MERAH 21
#define LED_KUNING 22
#define LED_HIJAU 23
const int DHT_PIN = 27;

DHTesp dhtSensor;

void setup() {
  pinMode(LED_MERAH, OUTPUT);
  pinMode(LED_KUNING, OUTPUT);
  pinMode(LED_HIJAU, OUTPUT);
  Serial.begin(115200);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  delay(2000);
}

void loop() {
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();

  Serial.print("Suhu: ");
  Serial.print(data.temperature, 2);

  if (data.temperature < 25){
    Serial.println(" °C, Lampu Hijau Menyala");
    digitalWrite(LED_HIJAU, HIGH);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, LOW);
  } else if (data.temperature >= 25 && data.temperature <= 40){
    Serial.println(" °C, Lampu Kuning Menyala");
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, HIGH);
    digitalWrite(LED_MERAH, LOW);
  } else {
    Serial.println(" °C, Lampu Merah Menyala");
    digitalWrite(LED_HIJAU, LOW);
    digitalWrite(LED_KUNING, LOW);
    digitalWrite(LED_MERAH, HIGH);
  }
  
  delay(1000);
}