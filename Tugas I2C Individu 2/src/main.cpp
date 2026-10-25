#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define PB_JAM   18
#define PB_MENIT 19

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;

bool lastJamState   = HIGH;
bool lastMenitState = HIGH;

unsigned long lastDebounceJam   = 0;
unsigned long lastDebounceMenit = 0;
const unsigned long debounceDelay = 200;

// dayOfTheWeek() returns 0 (Minggu) … 6 (Sabtu)
const char* NAMA_HARI[] = {"Min", "Sen", "Sel", "Rab", "Kam", "Jum", "Sab"};

void tampilWaktu(const DateTime& now) {
    // Baris 0: "2026/09/24 Sab"
    lcd.setCursor(0, 0);
    lcd.print(now.year());
    lcd.print('/');
    if (now.month()  < 10) lcd.print('0');
    lcd.print(now.month());
    lcd.print('/');
    if (now.day()    < 10) lcd.print('0');
    lcd.print(now.day());
    lcd.print(' ');
    lcd.print(NAMA_HARI[now.dayOfTheWeek()]);

    // Baris 1: "19:00:00" — titik dua berkedip tiap detik
    char sep = (now.second() % 2 == 0) ? ':' : ' ';
    lcd.setCursor(0, 1);
    if (now.hour()   < 10) lcd.print('0');
    lcd.print(now.hour());
    lcd.print(sep);
    if (now.minute() < 10) lcd.print('0');
    lcd.print(now.minute());
    lcd.print(sep);
    if (now.second() < 10) lcd.print('0');
    lcd.print(now.second());
    lcd.print("        ");
}

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    pinMode(PB_JAM,   INPUT_PULLUP);
    pinMode(PB_MENIT, INPUT_PULLUP);

    if (!rtc.begin()) {
        Serial.println("RTC tidak terdeteksi");
        lcd.print("RTC not found!");
        while (1) delay(10);
    }

    // Jika RTC belum pernah diset, sync ke waktu kompilasi
    if (!rtc.isrunning()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    lcd.print("RTC DS1307");
    lcd.setCursor(0, 1);
    lcd.print("Siap...");
    delay(1500);
    lcd.clear();
}

void loop() {
    DateTime now = rtc.now();

    // Tombol jam: tambah 1 jam
    bool jamState = digitalRead(PB_JAM);
    if (jamState == LOW && lastJamState == HIGH &&
        millis() - lastDebounceJam > debounceDelay) {
        int jamBaru = (now.hour() + 1) % 24;
        rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                            jamBaru, now.minute(), now.second()));
        Serial.printf("Jam → %02d\r\n", jamBaru);
        lastDebounceJam = millis();
    }
    lastJamState = jamState;

    // Tombol menit: tambah 1 menit
    bool menitState = digitalRead(PB_MENIT);
    if (menitState == LOW && lastMenitState == HIGH &&
        millis() - lastDebounceMenit > debounceDelay) {
        int menitBaru = now.minute() + 1;
        int jamBaru   = now.hour();
        if (menitBaru >= 60) {
            menitBaru = 0;
            jamBaru   = (jamBaru + 1) % 24;
        }
        rtc.adjust(DateTime(now.year(), now.month(), now.day(),
                            jamBaru, menitBaru, now.second()));
        Serial.printf("Menit → %02d:%02d\r\n", jamBaru, menitBaru);
        lastDebounceMenit = millis();
    }
    lastMenitState = menitState;

    tampilWaktu(rtc.now());
    delay(100);
}