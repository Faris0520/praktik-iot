#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // alamat I2C LCD, 16 kolom x 2 baris
RTC_DS1307 rtc;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    Serial.println("RTC DS1307 tidak terdeteksi!");
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1) delay(10);
  }

  // --- Set nilai awal RTC (hanya perlu sekali) ---
  // Format: tahun, bulan, tanggal, jam, menit, detik
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.setCursor(0, 0);
  lcd.print("RTC DS1307 Demo");
  delay(1500);
  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();

  // Baris 1: tanggal (YYYY/MM/DD)
  lcd.setCursor(0, 0);
  lcd.print(now.year());
  lcd.print('/');
  if (now.month() < 10) lcd.print('0');
  lcd.print(now.month());
  lcd.print('/');
  if (now.day() < 10) lcd.print('0');
  lcd.print(now.day());

  // Baris 2: jam (HH:MM:SS)
  lcd.setCursor(0, 1);
  if (now.hour() < 10) lcd.print('0');
  lcd.print(now.hour());
  lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute());
  lcd.print(':');
  if (now.second() < 10) lcd.print('0');
  lcd.print(now.second());
  lcd.print(" "); // bersihkan sisa karakter lama

  delay(1000);
}