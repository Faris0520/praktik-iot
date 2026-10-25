/*
  camera_pins.h
  ---------------------------------------------------------------------------
  Definisi pin kamera untuk board:
    DFRobot ESP32-S3 AI Camera Module V1.0 (DFR1154) - sensor OV3660

  Pin ini SUDAH SESUAI dengan tata letak board bawaan (kamera terpasang
  langsung di modul, bukan dipasang manual dengan kabel). Jangan diubah
  kecuali Anda memakai board custom dengan tata letak berbeda.

  Referensi: DFRobot Wiki - DFR1154 ESP32-S3 AI Camera Module
  https://wiki.dfrobot.com/dfr1154/

  Berkas ini identik dengan versi Arduino IDE — cukup disalin ke folder
  include/ pada struktur proyek PlatformIO.
  ---------------------------------------------------------------------------
*/

#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// Kamera pada board ini tidak memakai pin power-down / reset terpisah
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1

// Clock & jalur kontrol I2C (SCCB) ke sensor OV3660
#define XCLK_GPIO_NUM      5
#define SIOD_GPIO_NUM      8   // SDA
#define SIOC_GPIO_NUM      9   // SCL

// Jalur data gambar (8 bit)
#define Y9_GPIO_NUM        4
#define Y8_GPIO_NUM        6
#define Y7_GPIO_NUM        7
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       17
#define Y4_GPIO_NUM       21
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       16

// Sinkronisasi frame
#define VSYNC_GPIO_NUM     1
#define HREF_GPIO_NUM      2
#define PCLK_GPIO_NUM     15

// Lampu bantu inframerah (IR fill light) bawaan board, untuk mode malam.
// Tidak dipakai pada contoh dasar ini, disediakan untuk pengembangan lanjutan.
#define IR_LED_GPIO_NUM   47

#endif // CAMERA_PINS_H
