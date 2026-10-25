#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

char serialCommand[32];
uint8_t serialCommandLength = 0;

void processSettingCommand() {
  serialCommand[serialCommandLength] = '\0';

  int hour;
  int minute;
  int second;
  char extra[2];
  int parsed = sscanf(serialCommand, "SETTING %d:%d:%d %1s",
                      &hour, &minute, &second, extra);

  if (parsed != 3 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) {
    Serial.println("[GAGAL] Format salah. Gunakan: SETTING hh:mm:ss");
    Serial.println("Contoh: SETTING 12:30:45");
    return;
  }

  DateTime currentTime = rtc.now();
  rtc.adjust(DateTime(currentTime.year(), currentTime.month(),
                      currentTime.day(), hour, minute, second));

  DateTime updatedTime = rtc.now();
  Serial.printf("[SUKSES] Waktu RTC berhasil diatur menjadi: %02d:%02d:%02d\n",
                updatedTime.hour(), updatedTime.minute(),
                updatedTime.second());
}

void readSerialCommand() {
  while (Serial.available() > 0) {
    char received = static_cast<char>(Serial.read());

    if (received == '\b' || received == 0x7F) {
      if (serialCommandLength > 0) {
        serialCommandLength--;
        Serial.print("\b \b");
      }
      continue;
    }

    Serial.write(received);

    if (received == '\n' || received == '\r') {
      if (serialCommandLength == 0) continue;

      processSettingCommand();
      serialCommandLength = 0;
    } else if (serialCommandLength < sizeof(serialCommand) - 1) {
      serialCommand[serialCommandLength++] = received;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if (!rtc.begin()) {
    Serial.println("RTC DS1307 tidak terdeteksi");
    lcd.setCursor(0, 0);
    lcd.print("RTC not found");
    while (1) delay(10);
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.setCursor(0, 0);
  lcd.print("RTC DS1307 Demo");

  Serial.println("=== RTC DS1307 Serial Control ===");
  Serial.println("Contoh: SETTING 12:30:45");
  Serial.println("Kirim perintah lalu tekan Enter/Newline.");
  Serial.println("========================");

  delay(1500);
  lcd.clear();
}

void loop() {
  readSerialCommand();

  DateTime now = rtc.now();

  lcd.setCursor(0, 0);
  lcd.print(now.year());
  lcd.print('/');
  if (now.month() < 10) lcd.print('0');
  lcd.print(now.month());
  lcd.print('/');
  if (now.day() < 10) lcd.print('0');
  lcd.print(now.day());

  lcd.setCursor(0, 1);
  if (now.hour() < 10) lcd.print('0');
  lcd.print(now.hour());
  lcd.print(':');
  if (now.minute() < 10) lcd.print('0');
  lcd.print(now.minute());
  lcd.print(':');
  if (now.second() < 10) lcd.print('0');
  lcd.print(now.second());
  lcd.print("  ");

  delay(1000);
}