#include "DHTesp.h"
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#define buzz 27
const int DHT_PIN = 18;
const float temp_threshold = 27.00;

DHTesp dhtSensor;
LiquidCrystal_I2C lcd(0x27, 16, 2); 

void setup() {
  Serial.begin(115200);
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  
  pinMode(buzz, OUTPUT);
  // setting koneksi LCD
  Wire.begin(21, 22); // nilai default 21, 22 I2C

  // Inisialisasi LCD
  lcd.init();

  // Menampilkan teks di baris pertama (kolom 0, baris 0)
  lcd.setCursor(0, 0);
  lcd.print("Halo, ESP32!");

  // Menampilkan teks di baris kedua (kolom 0, baris 1)
  lcd.setCursor(0, 1);
  lcd.print("LCD 16x2 I2C OK");
  delay(2000);
}

void loop() {
  // Baca data sensor
  TempAndHumidity  data = dhtSensor.getTempAndHumidity();
  // Tampilkan data sensor di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp = "+ String(data.temperature,2));
  lcd.setCursor(0, 1);
  lcd.print("Hum = "+ String(data.humidity,2));

  Serial.print("Suhu: ");
  Serial.print(data.temperature, 2);
  Serial.print(" °C, Kelembapan: ");
  Serial.print(data.humidity, 2);
  Serial.println(" %");

  if (data.temperature >= temp_threshold) {
    Serial.println("Peringatan: suhu melewati ambang batas!");
  }

  // Cek nilai sensor apakah melampaui treshold, jika ia hidupkan buzzer
  if (data.temperature >= temp_threshold){
    int i;
    for (i=0; i<100; i++){
      digitalWrite(buzz, HIGH);
      delay(10);
      digitalWrite(buzz, LOW);
      delay(10);
    }
  }
  delay(1000);
}