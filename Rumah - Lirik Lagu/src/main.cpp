
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 21
#define OLED_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

String incomingText = "";

void showText(String message) {
    const double textSize = 1.6;
    const int charWidth = 6 * textSize;
    const int lineHeight = 8 * textSize;
    const int headerHeight = 12;
    const int maxChars = SCREEN_WIDTH / charWidth;
    const int maxLines = (SCREEN_HEIGHT - headerHeight) / lineHeight;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("LIRIK");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    message.trim();
    display.setTextSize(textSize);

    int y = headerHeight;
    int lines = 0;

    while (message.length() > 0 && lines < maxLines) {
        int breakAt = min(maxChars, (int)message.length());

        if (message.length() > maxChars) {
            int space = message.lastIndexOf(' ', maxChars);

            if (space > 0) {
                breakAt = space;
            }
        }

        display.setCursor(0, y);
        display.print(message.substring(0, breakAt));

        message = message.substring(breakAt);
        message.trim();

        y += lineHeight;
        lines++;
    }

    display.display();
}

void setup() {
    Serial.begin(115200);

    Wire.begin(OLED_SDA, OLED_SCL);

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
    display.setTextSize(2);
    display.setCursor(0, 16);
    display.println("Siap");
    display.setCursor(0, 36);
    display.println("USB...");
    display.display();

    Serial.println("ESP32 siap menerima teks.");
}

void loop() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (incomingText.length() > 0) {
                Serial.print("Teks diterima: ");
                Serial.println(incomingText);

                showText(incomingText);
                incomingText = "";
            }
        } else if (incomingText.length() < 240) {
            incomingText += c;
        }
    }
}