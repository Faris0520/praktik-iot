#include <Wire.h>
#include <DHTesp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin sensor DHT11
#define DHT_PIN 4

// Pin I2C ESP32
#define OLED_SDA 21
#define OLED_SCL 22

// Konfigurasi OLED 128x64
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Alamat I2C OLED umumnya 0x3C
#define OLED_ADDRESS 0x3C

DHTesp dht;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor DHT11
  dht.setup(DHT_PIN, DHTesp::DHT11);

  // Inisialisasi I2C
  Wire.begin(OLED_SDA, OLED_SCL);

  // Inisialisasi OLED
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS)) {
    Serial.println("OLED tidak terdeteksi!");
    while (true) {
      delay(1000);
    }
  }

  // Tampilan awal
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(15, 15);
  display.println("ESP32 MONITOR");
  display.setCursor(25, 30);
  display.println("DHT11 SENSOR");
  display.display();

  delay(2000);
}

void loop() {
  // Membaca data sensor
  TempAndHumidity data = dht.getTempAndHumidity();

  float suhu = data.temperature;
  float kelembapan = data.humidity;

  // Periksa apakah pembacaan valid
  if (isnan(suhu) || isnan(kelembapan)) {
    Serial.println("Gagal membaca DHT11!");

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.println("Sensor error!");
    display.setCursor(10, 35);
    display.println("Periksa wiring");
    display.display();

    delay(2000);
    return;
  }

  // Tampilkan pada Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(suhu, 1);
  Serial.print(" C | Kelembapan: ");
  Serial.print(kelembapan, 1);
  Serial.println(" %");

  // Tampilkan pada OLED
  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  // Judul
  display.setTextSize(1);
  display.setCursor(18, 0);
  display.println("MONITOR LINGKUNGAN");

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  // Suhu
  display.setTextSize(1);
  display.setCursor(5, 20);
  display.println("SUHU");

  display.setTextSize(2);
  display.setCursor(5, 31);
  display.print(suhu, 1);
  display.print(" C");

  // Kelembapan
  display.setTextSize(1);
  display.setCursor(5, 49);
  display.print("Kelembapan: ");

  display.print(kelembapan, 1);
  display.print("%");

  display.display();

  // Interval pembacaan
  delay(2000);
}
