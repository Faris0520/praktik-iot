# Spotify Lyrics Display dengan ESP32 dan OLED

Proyek ini menampilkan lirik lagu yang sedang diputar di laptop Windows pada layar OLED GME12864 128×64 yang dikendalikan oleh ESP32. Program Python pada laptop membaca metadata dari Windows Media Session, mencari lirik bertimestamp melalui LRCLIB, lalu mengirimkan baris lirik ke ESP32 melalui USB Serial.

> **Status proyek:** versi saat ini berfokus pada tampilan lirik melalui USB Serial. Integrasi sensor DHT11 belum digabungkan ke program lirik. Controller OLED diasumsikan SSD1306 dengan alamat I²C `0x3C`; sesuaikan jika modul yang digunakan berbeda.

## Fitur

- Membaca informasi media yang tersedia melalui Windows Media Session.
- Mencari lirik yang memiliki timestamp melalui LRCLIB.
- Memperbarui tampilan OLED berdasarkan posisi pemutaran lagu.
- Mengirim teks dari laptop ke ESP32 melalui USB Serial.
- Menampilkan status jika lagu atau lirik tidak terdeteksi.

## Komponen perangkat keras

| Komponen | Jumlah | Keterangan |
|---|---:|---|
| ESP32 DevKit | 1 | Pengendali layar |
| OLED GME12864 128×64 I²C | 1 | Controller diasumsikan SSD1306 |
| Kabel USB data | 1 | Pemrograman ESP32 dan komunikasi Serial |
| Kabel jumper | Secukupnya | Untuk koneksi OLED |
| DHT11 | 1 (opsional) | Belum dipakai oleh firmware lirik saat ini |

## Wiring OLED ke ESP32

| Pin OLED | Pin ESP32 |
|---|---|
| VCC | 3V3* |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

*Pastikan tegangan kerja modul sesuai sebelum menghubungkannya. Jika OLED tidak menyala, periksa controller dan alamat I²C yang digunakan.

### Koneksi DHT11 (opsional)

| Pin DHT11 | Pin ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DATA | GPIO 4 |

DHT11 tidak dibaca oleh program lirik dalam versi ini. Integrasi untuk beralih antara informasi suhu/kelembapan dan lirik dapat ditambahkan kemudian.

## Perangkat lunak

- Windows 10/11 dengan Windows Media Session aktif.
- Python untuk Windows.
- PlatformIO dengan Arduino framework untuk firmware ESP32.
- Spotify desktop atau pemutar lain yang menerbitkan metadata ke Windows Media Session.
- Koneksi internet untuk mencari lirik dari LRCLIB.

**Catatan kompatibilitas:** jika instalasi paket WinRT gagal pada Python 3.14, coba Python 3.11 atau 3.12 64-bit dan jalankan perintah instalasi menggunakan interpreter tersebut. Pastikan interpreter yang dipakai untuk memasang paket sama dengan interpreter untuk menjalankan program.

## Struktur folder

Contoh struktur proyek:

```text
spotify-lyrics-display/
├── firmware/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
├── python/
│   └── lyrics.py
└── README.md
```

Sesuaikan nama dan lokasi folder dengan susunan proyek lokal.

## Dependensi Python

Dari terminal PowerShell, masuk ke folder `python`, lalu instal dependensi:

```powershell
python -m pip install requests pyserial winrt-runtime winrt-Windows.Foundation winrt-Windows.Media.Control
```

Paket WinRT dipisahkan berdasarkan namespace. `winrt-Windows.Foundation` diperlukan untuk namespace `winrt.windows.foundation`, sementara `winrt-Windows.Media.Control` menyediakan akses ke Windows Media Session.

Untuk memastikan modul tersedia pada interpreter yang digunakan:

```powershell
python -c "import winrt.windows.foundation; from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager; print('WinRT berhasil diimpor')"
```

Jika perintah di atas gagal, pastikan paket diinstal pada interpreter yang sama. Kamu dapat memeriksa interpreter dan lokasi pip dengan:

```powershell
python --version
python -m pip --version
```

## Dependensi PlatformIO

Contoh isi `platformio.ini` untuk firmware OLED:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
```

Firmware perlu menjalankan program penerima teks Serial pada ESP32. Contoh kode penerima yang dibahas dalam proyek ini menggunakan baud rate `115200`, SDA `GPIO 21`, SCL `GPIO 22`, resolusi `128×64`, dan alamat OLED `0x3C`.

## Cara menjalankan

### 1. Unggah firmware ke ESP32

1. Buka folder firmware di PlatformIO.
2. Pastikan pin dan alamat I²C sesuai dengan modul OLED.
3. Bangun dan unggah program penerima teks ke ESP32.
4. Tutup Serial Monitor sebelum menjalankan program Python karena satu port COM umumnya tidak dapat dipakai oleh dua program sekaligus.

### 2. Jalankan program Python

1. Hubungkan ESP32 ke laptop menggunakan kabel USB data.
2. Pastikan Spotify sedang memutar lagu.
3. Buka terminal di folder `python`.
4. Jalankan program:

   ```powershell
   python lyrics.py
   ```

5. Pilih nomor port COM ESP32, misalnya `COM7`.
6. Program akan mencoba membaca media aktif, mencari lirik bertimestamp, lalu mengirim baris lirik ke OLED.
7. Hentikan program dengan `Ctrl+C`.

## Cara kerja

1. Windows Media Session menyediakan metadata dari media yang sedang diputar.
2. Program Python membaca judul, artis, durasi, dan posisi pemutaran jika tersedia.
3. Program mencari lirik bertimestamp melalui endpoint LRCLIB.
4. Baris yang sesuai dengan posisi lagu dikirim melalui USB Serial.
5. ESP32 menerima teks dan memperbarui OLED.

## Pemecahan masalah

| Masalah | Kemungkinan penyebab dan solusi |
|---|---|
| `ModuleNotFoundError: No module named 'winrt.windows.foundation'` | Instal `winrt-Windows.Foundation` menggunakan interpreter Python yang sama dengan yang menjalankan `lyrics.py`. |
| Instalasi `winsdk` gagal saat membangun wheel | Proyek ini menggunakan paket PyWinRT modular; jangan memasang `winsdk` lama untuk kode berbasis `winrt.windows...`. |
| Port COM tidak muncul | Periksa kabel USB data, driver USB-to-UART, dan koneksi ESP32. |
| `Access is denied` atau port sedang digunakan | Tutup Serial Monitor, terminal serial lain, atau aplikasi lain yang memakai COM tersebut. |
| Program tidak menemukan media aktif | Pastikan pemutar menampilkan metadata ke kontrol media Windows. Dukungan dapat berbeda antara aplikasi Spotify desktop dan Spotify di browser. |
| `429 Client Error: Too Many Requests` | LRCLIB membatasi request. Program menunggu header `Retry-After` dan menyimpan hasil pencarian lagu yang sama. Jangan restart program berulang kali saat limit masih aktif. |
| Lirik tidak ditemukan | Tidak semua lagu tersedia di LRCLIB, dan tidak semua hasil memiliki timestamp. |
| Lirik tidak sinkron | Posisi pemutaran dari Windows dapat terlambat diperbarui, atau timestamp sumber lirik kurang akurat. |
| OLED kosong | Periksa VCC, GND, SDA, SCL, alamat `0x3C`/`0x3D`, dan controller layar. Jika menggunakan SH1106, firmware harus disesuaikan. |
| Karakter tidak tampil benar | OLED berbasis font bawaan mungkin tidak mendukung semua karakter Unicode; pertimbangkan normalisasi teks pada program Python. |

## Batasan

- Proyek bergantung pada metadata yang diterbitkan ke Windows Media Session; tidak semua pemutar atau browser berperilaku sama.
- Lirik hanya dapat ditampilkan jika sumber menyediakan lirik yang cocok dan bertimestamp.
- Sinkronisasi adalah perkiraan berdasarkan posisi pemutaran media dan timestamp lirik.
- OLED 128×64 memiliki ruang terbatas, sehingga lirik ditampilkan per baris atau beberapa baris pendek.
- DHT11 belum terintegrasi dengan tampilan lirik dalam versi ini.

## Privasi, hak cipta, dan ketentuan layanan

Proyek ini mengambil metadata media dari Windows dan meminta lirik dari LRCLIB. Gunakan lirik sesuai hak cipta dan ketentuan layanan sumbernya. Jika proyek dikembangkan atau didistribusikan lebih lanjut dengan menggunakan platform Spotify, periksa kebijakan pengembang Spotify yang berlaku, khususnya aturan mengenai sinkronisasi konten Spotify dengan media visual.

- Dokumentasi LRCLIB: https://lrclib.net/docs
- Kebijakan Pengembang Spotify: https://developer.spotify.com/policy
- Dokumentasi Spotify Web API: https://developer.spotify.com/documentation/web-api

## Pengembangan selanjutnya

- Menggabungkan DHT11 agar layar bisa menampilkan suhu dan kelembapan saat tidak ada lagu.
- Menambahkan pembungkusan kata dan pengaturan ukuran teks untuk lirik panjang.
- Memperbaiki pencocokan judul dan artis untuk versi remix atau judul alternatif.
- Menambahkan indikator status koneksi dan status pencarian lirik.

## Lisensi

Belum ada lisensi yang ditentukan. Tambahkan file `LICENSE` jika proyek akan dipublikasikan dan dibagikan secara terbuka.
