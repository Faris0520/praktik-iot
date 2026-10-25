#include <Arduino.h>
#include <Wire.h>
#include <DHTesp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "angry.h"

// ==================================================
// KONFIGURASI ANIMASI
// ==================================================
#define ANIM_FRAMES_TBL angry_frames
#define ANIM_DELAYS_TBL angry_delays
#define ANIM_COUNT      ANGRY_FRAMES
#define ANIM_W          ANGRY_W
#define ANIM_H          ANGRY_H

// ==================================================
// PIN ESP32
// ==================================================

// DHT11
#define DHT_PIN 4

// OLED I2C
#define OLED_SDA 21
#define OLED_SCL 22

// Push button
#define BUTTON_PIN 18

// HC-SR501 PIR
#define PIR_PIN 27

// ==================================================
// KONFIGURASI OLED
// ==================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

// Interval pembacaan DHT11
const unsigned long DHT_INTERVAL = 2000;

// Debounce tombol
const unsigned long DEBOUNCE_MS = 40;

// ==================================================
// OBJEK
// ==================================================
DHTesp dht;

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

// ==================================================
// VARIABEL SENSOR
// ==================================================
float suhu = NAN;
float kelembapan = NAN;

bool sensorValid = false;

unsigned long lastDhtRead = 0;

// ==================================================
// VARIABEL TOMBOL
// ==================================================
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;

// ==================================================
// VARIABEL PIR
// ==================================================
bool lastPirState = LOW;

// ==================================================
// VARIABEL ANIMASI
// ==================================================
bool animPlaying = false;

uint16_t animFrame = 0;

unsigned long animFrameStartedAt = 0;

// ==================================================
// MEMBACA SENSOR DHT11
// ==================================================
void readDHT() {
    TempAndHumidity data = dht.getTempAndHumidity();

    suhu = data.temperature;
    kelembapan = data.humidity;

    sensorValid = !isnan(suhu) && !isnan(kelembapan);

    if (sensorValid) {
        Serial.print("Suhu: ");
        Serial.print(suhu, 1);
        Serial.print(" C | Kelembapan: ");
        Serial.print(kelembapan, 1);
        Serial.println(" %");
    } else {
        Serial.println("Gagal membaca DHT11!");
    }
}

// ==================================================
// MENAMPILKAN DATA SENSOR PADA OLED
// ==================================================
void drawSensorScreen() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    if (!sensorValid) {
        display.setTextSize(1);

        display.setCursor(10, 10);
        display.println("MONITOR LINGKUNGAN");

        display.drawLine(0, 22, 127, 22, SSD1306_WHITE);

        display.setCursor(10, 30);
        display.println("Sensor error!");

        display.setCursor(10, 43);
        display.println("Periksa DHT11");

        display.display();
        return;
    }

    // Judul
    display.setTextSize(1);
    display.setCursor(18, 0);
    display.println("MONITOR LINGKUNGAN");

    display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

    // Suhu
    display.setCursor(5, 20);
    display.println("SUHU");

    display.setTextSize(2);
    display.setCursor(5, 31);
    display.print(suhu, 1);
    display.print(" C");

    // Kelembapan
    display.setTextSize(1);
    display.setCursor(5, 53);
    display.print("Kelembapan: ");
    display.print(kelembapan, 1);
    display.print("%");

    display.display();
}

// ==================================================
// MENAMPILKAN SATU FRAME ANIMASI
// ==================================================
void drawAnimationFrame(uint16_t index) {
    const unsigned char* bitmap =
        (const unsigned char*)pgm_read_ptr(
            &ANIM_FRAMES_TBL[index]
        );

    display.clearDisplay();

    // Animasi diposisikan di tengah OLED
    int16_t x = (SCREEN_WIDTH - ANIM_W) / 2;
    int16_t y = (SCREEN_HEIGHT - ANIM_H) / 2;

    display.drawBitmap(
        x,
        y,
        bitmap,
        ANIM_W,
        ANIM_H,
        SSD1306_WHITE
    );

    display.display();
}

// ==================================================
// MEMULAI ANIMASI
// ==================================================
void startAnimation() {
    if (animPlaying) {
        return;
    }

    Serial.println("Animasi dimulai!");

    animPlaying = true;
    animFrame = 0;
    animFrameStartedAt = millis();

    drawAnimationFrame(animFrame);
}

// ==================================================
// MEMPERBARUI ANIMASI TANPA DELAY
// ==================================================
void updateAnimation() {
    if (!animPlaying) {
        return;
    }

    unsigned long now = millis();

    // Durasi frame saat ini dari angry.h
    uint16_t frameDelay = pgm_read_word(
        &ANIM_DELAYS_TBL[animFrame]
    );

    if (now - animFrameStartedAt < frameDelay) {
        return;
    }

    // Lanjut ke frame berikutnya
    animFrame++;

    // Semua frame selesai
    if (animFrame >= ANIM_COUNT) {
        animPlaying = false;

        Serial.println("Animasi selesai.");

        // Kembali ke tampilan sensor
        drawSensorScreen();
        return;
    }

    // Tampilkan frame berikutnya
    drawAnimationFrame(animFrame);

    animFrameStartedAt = now;
}

// ==================================================
// SETUP
// ==================================================
void setup() {
    Serial.begin(115200);

    // Inisialisasi pin
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(PIR_PIN, INPUT);

    // Inisialisasi kondisi awal tombol
    lastButtonReading = digitalRead(BUTTON_PIN);
    stableButtonState = lastButtonReading;
    lastDebounceTime = millis();

    // Hindari memicu animasi hanya karena
    // output PIR sudah HIGH saat ESP32 menyala
    lastPirState = (digitalRead(PIR_PIN) == HIGH);

    // Inisialisasi DHT11
    dht.setup(DHT_PIN, DHTesp::DHT11);

    // Inisialisasi I2C
    Wire.begin(OLED_SDA, OLED_SCL);

    // Inisialisasi OLED
    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            OLED_ADDRESS
        )) {
        Serial.println("OLED tidak terdeteksi!");

        while (true) {
            delay(1000);
        }
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();

    // Agar DHT11 langsung dibaca pada loop pertama
    lastDhtRead = millis() - DHT_INTERVAL;

    Serial.println("Sistem siap.");
    Serial.println("Tekan tombol atau gerakkan objek di depan PIR.");
}

// ==================================================
// LOOP
// ==================================================
void loop() {
    unsigned long now = millis();

    // ----------------------------------------------
    // 1. DETEKSI TOMBOL DENGAN DEBOUNCE
    // ----------------------------------------------
    bool rawButtonState = digitalRead(BUTTON_PIN);
    bool buttonPressedEvent = false;

    if (rawButtonState != lastButtonReading) {
        lastButtonReading = rawButtonState;
        lastDebounceTime = now;
    }

    if (now - lastDebounceTime >= DEBOUNCE_MS) {
        if (rawButtonState != stableButtonState) {
            stableButtonState = rawButtonState;

            // INPUT_PULLUP:
            // LOW berarti tombol sedang ditekan
            if (stableButtonState == LOW) {
                buttonPressedEvent = true;
            }
        }
    }

    // ----------------------------------------------
    // 2. DETEKSI GERAKAN DARI HC-SR501
    // ----------------------------------------------
    bool pirState = (digitalRead(PIR_PIN) == HIGH);

    // Trigger hanya saat PIR berubah LOW -> HIGH
    bool motionDetected = pirState && !lastPirState;

    lastPirState = pirState;

    // ----------------------------------------------
    // 3. PEMBACAAN DHT11 SETIAP 2 DETIK
    // ----------------------------------------------
    if (now - lastDhtRead >= DHT_INTERVAL) {
        lastDhtRead = now;

        readDHT();

        // Jangan menimpa animasi dengan tampilan sensor
        if (!animPlaying) {
            drawSensorScreen();
        }
    }

    // ----------------------------------------------
    // 4. TOMBOL ATAU PIR MEMICU ANIMASI
    // ----------------------------------------------
    if (!animPlaying &&
        (buttonPressedEvent || motionDetected)) {

        if (buttonPressedEvent) {
            Serial.println("Tombol ditekan.");
        }

        if (motionDetected) {
            Serial.println("Gerakan terdeteksi oleh PIR.");
        }

        startAnimation();
    }

    // ----------------------------------------------
    // 5. JALANKAN ANIMASI
    // ----------------------------------------------
    updateAnimation();
}