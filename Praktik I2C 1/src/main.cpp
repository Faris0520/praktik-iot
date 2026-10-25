#include <LiquidCrystal_I2C.h>

#define I2C_ADDR 0x20
#define LCD_COLUMNS 16
#define LCD_LINES 2

LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

void setup() {
  // Setting koneksi LCD
  Wire.begin(21,22);

  // Init
  lcd.init();
  lcd.backlight();

  // Print something
  lcd.setCursor(0, 0);
  lcd.print("Hello, world!");
  lcd.setCursor(0, 1);
  lcd.print("Wiwok Online IoT");
}

void loop() {
}