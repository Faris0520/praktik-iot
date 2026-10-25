// /*
//   ESP32 Pertama
// */

// #include <Arduino.h>

// void setup() {
//   // put your setup code here, to run once:
//   Serial.begin(115200);
//   Serial.setTimeout(5000);
//   Serial2.begin(115200); // pin default untuk Serial2 adalah RX2 = 16, TX2 = 17
//   Serial.println("Masukkan text (maksimal 16 karakter): ");
//   //mengosongkan buffer serial agar tidak ada data sisa dari percobaan sebelumnya
//   while (Serial.available() > 0) {
//     Serial.read();
//   }
//   while (Serial2.available() > 0) {
//     Serial.read();
//   }
// }

// void loop() {
//   // put your main code here, to run repeatedly:

//   if (Serial.available()) {
//     String data="";
//     //baca data input dari serial monitor sampai menemukan karakter '\n' (enter)
//     data = Serial.readStringUntil('\n');
//     data.trim(); // buang \r atau spasi/whitespace sisa di ujung
//     //Kirim data ke node 2 melalui serial2
//     Serial2.println(data);
//     //Tampilkan status data dikirim ke node 2
//     Serial.println("ESP-1 mengirim data:");
//     Serial.println(data);
//     //delay(1000); //tunggu 1 detik sebelum membaca balasan dari node 2
//     delay(500); //tunggu 0.5 detik sebelum membaca balasan dari node 2
//     //cek apakah ada balasan dari node 2
//     if (Serial2.available()) {
//       String data="";
//       //baca data input dari esp2
//       data = Serial2.readStringUntil('\n');
//       data.trim(); // buang \r atau spasi/whitespace sisa di ujung
//       //tampilkan balasan dari node 2
//       Serial.println("ESP-2 Konfirmasi data diterima:");
//       Serial.println(data);
//     }
//     //Meminta data berikutnya
//     Serial.println("Masukkan text (maksimal 16 karakter): ");
//   }
// }
/*
  ESP32 Pertama
  DHT11 -> UART2 -> ESP32 Kedua
*/

#include <Arduino.h>
#include "DHTesp.h"

// Pin DHT11
#define DHT_PIN 18

// Membuat objek DHT
DHTesp dht;

void setup() {

  // Serial Monitor
  Serial.begin(115200);

  // UART2
  // RX2 = GPIO16
  // TX2 = GPIO17
  Serial2.begin(115200);

  // Inisialisasi DHT11
  dht.setup(DHT_PIN, DHTesp::DHT11);

  Serial.println("ESP32-1 siap.");
  Serial.println("Membaca DHT11 setiap 5 detik...");
}

void loop() {

  // Membaca data dari DHT11
  TempAndHumidity data = dht.getTempAndHumidity();

  // Cek apakah pembacaan berhasil
  if (isnan(data.temperature) || isnan(data.humidity)) {
    Serial.println("Gagal membaca DHT11!");
    delay(5000);
    return;
  }

  // Membuat data yang akan dikirim
  String pesan = "T:" + String(data.temperature, 1) +
                 "C H:" + String(data.humidity, 1) + "%";

  // Tampilkan hasil pembacaan di Serial Monitor
  Serial.println("Data DHT11:");
  Serial.print("Temperature: ");
  Serial.print(data.temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(data.humidity, 1);
  Serial.println(" %");

  // Kirim data ke ESP32-2 melalui UART2
  Serial2.println(pesan);

  Serial.println("Data dikirim ke ESP32-2:");
  Serial.println(pesan);

  Serial.println("--------------------");

  // Baca sensor setiap 5 detik
  delay(5000);
}