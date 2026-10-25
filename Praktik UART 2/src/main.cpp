// /*
//   ESP32 Kedua
// */

// #include <Arduino.h>
// #include "LiquidCrystal_I2C.h"

// LiquidCrystal_I2C lcd(0x27, 16, 2);

// void setup() {
//   // put your setup code here, to run once:
//   Serial2.begin(115200); // pin default untuk Serial2 adalah RX2 = 16, TX2 = 17
//   Serial2.setTimeout(5000);
//   while (Serial2.available() > 0) {
//     Serial.read();
//   }

//   //setting koneksi LCD
//   Wire.begin(21, 22); // nilai default 21,22 I2C
//   // Inisialisasi LCD
//   lcd.init();
//   // Menyalakan lampu latar (backlight)
//   lcd.backlight();
//   // Menampilkan teks di baris pertama (kolom 0, baris 0)
//   lcd.setCursor(0, 0);
//   lcd.print("Halo, ESP32!");
//   // Menampilkan teks di baris kedua (kolom 0, baris 1)
//   lcd.setCursor(0, 1);
//   lcd.print("Siap Menerima Data");
// }

// void loop() {
//   String data="";
//   if (Serial2.available()) {
//     data = Serial2.readStringUntil('\n');
//     data.trim();
//     lcd.clear();
//     //menampilkan data di LCD
//     // Menampilkan teks di baris pertama (kolom 0, baris 0)
//     lcd.setCursor(0, 0);
//     lcd.print("Data diterima:");
//     // Menampilkan teks di baris kedua (kolom 0, baris 1)
//     lcd.setCursor(0, 1);
//     lcd.print(data);// buang \r atau spasi/whitespace sisa di ujung
//     //kirim konfirmasi ke esp-1 data diterima
//     Serial2.println(data);
//   }
// }
/*
  ESP32 Kedua
  UART2 -> LCD I2C
*/

#include <Arduino.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"

// LCD 16x2
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {

  // UART2
  // RX2 = GPIO16
  // TX2 = GPIO17
  Serial2.begin(115200);

  // Serial Monitor untuk debugging
  Serial.begin(115200);

  // I2C
  // SDA = GPIO21
  // SCL = GPIO22
  Wire.begin(21, 22);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // Tampilan awal
  lcd.setCursor(0, 0);
  lcd.print("ESP32-2 Ready");

  lcd.setCursor(0, 1);
  lcd.print("Waiting data...");

  Serial.println("ESP32-2 siap.");
}

void loop() {

  if (Serial2.available()) {

    // Baca data sampai newline
    String data = Serial2.readStringUntil('\n');

    // Buang whitespace
    data.trim();

    // Tampilkan data yang diterima ke Serial Monitor
    Serial.println("Data diterima dari ESP32-1:");
    Serial.println(data);

    // Contoh data:
    // T:29.0C H:75.0%

    // Pisahkan temperature dan humidity
    int posisiH = data.indexOf("H:");

    if (posisiH != -1) {

      // Ambil bagian temperature
      String temperature = data.substring(2, posisiH);

      // Ambil bagian humidity
      String humidity = data.substring(posisiH + 2);

      // Hapus karakter C dari temperature
      temperature.replace("C", "");

      // Hapus karakter % dari humidity
      humidity.replace("%", "");

      // Bersihkan LCD
      lcd.clear();

      // Tampilkan temperature
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(temperature);
      lcd.print(" C");

      // Tampilkan humidity
      lcd.setCursor(0, 1);
      lcd.print("Hum : ");
      lcd.print(humidity);
      lcd.print(" %");
    }
  }
}