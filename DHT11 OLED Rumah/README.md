# Monitoring Suhu dan Kelembapan ESP32

Proyek sederhana untuk memantau suhu dan kelembapan udara menggunakan **ESP32, sensor DHT11, dan layar OLED GME12864 128×64 I²C**. Data yang dibaca oleh sensor akan ditampilkan pada layar OLED dan Serial Monitor secara berkala.

## Fitur

- Membaca suhu dalam derajat Celsius (°C).
- Membaca kelembapan udara dalam persen (%).
- Menampilkan hasil pembacaan sensor pada layar OLED.
- Menampilkan data sensor melalui Serial Monitor.
- Mendeteksi kegagalan pembacaan sensor.
- Memperbarui data setiap 2 detik.

## Komponen yang Dibutuhkan

| No. | Komponen | Jumlah |
|---:|---|---:|
| 1 | ESP32 DevKit V1 | 1 |
| 2 | Sensor DHT11 | 1 |
| 3 | OLED GME12864 128×64 I²C | 1 |
| 4 | Kabel jumper | Secukupnya |
| 5 | Breadboard | 1 (opsional) |

## Rangkaian dan Wiring

### 1. DHT11 ke ESP32

| Pin DHT11 | Pin ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DATA | GPIO 4 |

### 2. GME12864 OLED ke ESP32

| Pin OLED | Pin ESP32 |
|---|---|
| VCC | 3V3* |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

*Pastikan tegangan kerja modul OLED sesuai dengan 3,3 V dan periksa jenis controller yang digunakan.*

## Perangkat Lunak

Perangkat lunak yang digunakan dalam proyek ini:

- **PlatformIO IDE** untuk pengelolaan proyek dan kompilasi program.
- **Arduino Framework** sebagai framework pemrograman ESP32.
- **Visual Studio Code** sebagai editor kode (opsional).

## Library yang Digunakan

Proyek ini menggunakan beberapa library berikut:

- `DHTesp` untuk membaca suhu dan kelembapan dari sensor DHT11.
- `Adafruit GFX Library` untuk menampilkan teks dan grafis.
- `Adafruit SSD1306` untuk mengontrol layar OLED berbasis SSD1306.

### Konfigurasi PlatformIO

Tambahkan konfigurasi berikut ke file `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
    beegee-tokyo/DHT sensor library for ESPx
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
```

## Struktur Proyek

```text
esp32-dht11-oled/
├── include/
├── lib/
├── src/
│   └── main.cpp
├── test/
├── platformio.ini
└── README.md
```

## Cara Menjalankan Proyek

1. Unduh atau kloning repositori proyek.
2. Buka folder proyek menggunakan Visual Studio Code dengan ekstensi PlatformIO.
3. Hubungkan ESP32 ke komputer menggunakan kabel USB.
4. Rakit rangkaian sesuai tabel wiring.
5. Pastikan konfigurasi pada `platformio.ini` sudah benar.
6. Klik **Build** untuk mengompilasi program.
7. Klik **Upload** untuk mengunggah program ke ESP32.
8. Buka Serial Monitor dengan baud rate `115200`.
9. Amati hasil pembacaan suhu dan kelembapan pada layar OLED.

## Contoh Hasil Pembacaan

### Serial Monitor

```text
Suhu: 29.4 C | Kelembapan: 75.0 %
Suhu: 29.5 C | Kelembapan: 74.8 %
Suhu: 29.5 C | Kelembapan: 74.9 %
```

### Tampilan OLED

```text
  MONITOR LINGKUNGAN
--------------------
SUHU

29.4 C

Kelembapan: 75.0%
```

*Nilai di atas hanya contoh. Hasil aktual bergantung pada kondisi lingkungan.*

## Cara Kerja Sistem

1. Sensor DHT11 membaca suhu dan kelembapan udara di sekitarnya.
2. ESP32 mengambil data hasil pembacaan sensor.
3. ESP32 memeriksa apakah data yang diterima valid.
4. Data suhu dan kelembapan ditampilkan pada layar OLED.
5. Data yang sama dikirim ke Serial Monitor.
6. Proses pembacaan dan penampilan data diulang setiap 2 detik.

## Pemecahan Masalah

| Masalah | Solusi |
|---|---|
| OLED tidak menyala | Periksa koneksi VCC, GND, SDA, dan SCL. |
| OLED tidak terdeteksi | Periksa alamat I²C, biasanya `0x3C` atau `0x3D`. |
| Layar kosong | Pastikan controller layar sesuai dengan library yang digunakan. |
| Sensor gagal membaca data | Periksa kabel DATA pada GPIO 4 dan konfigurasi sensor DHT11. |
| Pembacaan tidak stabil | Periksa sambungan kabel dan pastikan catu daya stabil. |

## Pengembangan Selanjutnya

Beberapa fitur yang dapat ditambahkan pada proyek ini:

- Menampilkan suhu dan kelembapan minimum serta maksimum.
- Membuat grafik perubahan suhu dan kelembapan.
- Mengirim data melalui Wi-Fi untuk pemantauan jarak jauh.
- Mengintegrasikan sensor dengan MQTT atau dashboard web.
- Menambahkan peringatan ketika suhu atau kelembapan melewati batas tertentu.

## Lisensi

Proyek ini dibuat untuk keperluan pembelajaran dan pengembangan pribadi. Program dapat dimodifikasi dan dikembangkan sesuai kebutuhan.