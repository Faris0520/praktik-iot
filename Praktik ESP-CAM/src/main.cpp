/*
  ============================================================================
  Proyek Contoh: ESP32-S3 AI Camera - Web Server + Analisis Objek dengan AI
  (Versi VSCode + PlatformIO)
  ----------------------------------------------------------------------------
  Board  : DFRobot ESP32-S3 AI Camera Module V1.0 (DFR1154)
  Sensor : OV3660 (3MP)

  Deskripsi:
    Program ini menunjukkan implementasi lengkap modul kamera pada board di
    atas, mencakup:
      1. Inisialisasi kamera bawaan board
      2. Web server sederhana di jaringan WiFi lokal
      3. Halaman web dengan pratinjau gambar (auto-refresh tiap 2 detik)
      4. Tombol "Analisis dengan AI" yang mengambil satu foto, mengirimkannya
         ke Google Gemini Vision API, lalu menampilkan hasil deskripsi objek
         yang dikenali AI

  Semua pengaturan board (memory type, PSRAM, ukuran flash, partisi, USB, dan
  dependensi library) sudah diatur lewat file platformio.ini di root proyek
  ini - tidak perlu diubah lewat menu seperti di Arduino IDE.

  Struktur proyek:
    esp32s3_ai_camera_pio/
    ├── platformio.ini        <- konfigurasi board & library
    ├── include/
    │   └── camera_pins.h     <- definisi pin kamera board DFR1154
    └── src/
        └── main.cpp          <- berkas ini

  Catatan keamanan & keterbatasan (untuk keperluan pembelajaran):
    - client.setInsecure() dipakai supaya koneksi HTTPS lebih sederhana untuk
      dipelajari. Untuk aplikasi produksi/nyata, gunakan validasi sertifikat
      TLS yang semestinya.
    - API key Gemini ditulis langsung di kode (hardcoded) agar mudah
      dipahami pemula. Untuk proyek nyata, sebaiknya simpan kredensial
      seperti ini di luar kode sumber (mis. lewat build_flags di
      platformio.ini yang di-.gitignore, bukan ditulis langsung di sini).
  ============================================================================
*/

#include <Arduino.h>          // wajib disertakan secara eksplisit di proyek PlatformIO (.cpp)
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <base64.h>
#include "esp_camera.h"
#include "camera_pins.h"

// ============================ KONFIGURASI PENGGUNA ============================
// Ganti nilai-nilai di bawah ini sesuai jaringan dan akun Anda.

const char* WIFI_SSID     = "TP-LINK_2899";
const char* WIFI_PASSWORD = "tepelink";

// Dapatkan API key gratis di https://aistudio.google.com/apikey
const char* GEMINI_API_KEY = "AQ.Ab8RN6LDMnL3MkHF-8GD5P2pSCOBqzpaHDtGiPriL2S8ivi1dQ";
const char* GEMINI_MODEL   = "gemini-3.6-flash";

// Prompt yang dikirim bersama gambar ke AI. Silakan ubah sesuai kebutuhan
// (mis. untuk deteksi jenis sampah, kematangan buah, kehadiran orang, dll).
const char* PROMPT_AI =
  "Jelaskan singkat dalam Bahasa Indonesia tolong tentukan warna dominan yang "
  "terlihat pada gambar ini. Jawab maksimal 3 kalimat.";

// ================================================================================

WebServer server(80);

// ---------------------------------------------------------------------------
// Inisialisasi kamera sesuai pin dan kemampuan board DFR1154
// ---------------------------------------------------------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Board ini punya 8MB PSRAM, manfaatkan agar resolusi & buffer lebih besar
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_XGA;   // 800 x 600
    config.jpeg_quality = 12;               // 0-63, makin kecil makin bagus
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_VGA;    // 640 x 480
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Inisialisasi kamera GAGAL, kode error: 0x%x\n", err);
    return false;
  }

  // Jika gambar hasil capture terbalik/miring di board Anda, aktifkan salah
  // satu baris berikut sesuai kebutuhan:
  // sensor_t *s = esp_camera_sensor_get();
  // s->set_vflip(s, 1);
  // s->set_hmirror(s, 1);

  return true;
}

// ---------------------------------------------------------------------------
// Halaman web utama (HTML + CSS + JavaScript dalam satu berkas)
// ---------------------------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"====(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-S3 AI Camera</title>
<style>
  body{font-family:Arial,sans-serif;background:#f4f4f4;text-align:center;padding:20px;margin:0;}
  h2{color:#2c3e50;}
  img{max-width:90%;border:4px solid #2c3e50;border-radius:8px;margin-top:10px;}
  button{margin-top:15px;padding:10px 22px;font-size:16px;background:#2c3e50;color:#fff;border:none;border-radius:6px;cursor:pointer;}
  button:disabled{background:#95a5a6;cursor:not-allowed;}
  #hasil{margin:20px auto 0;padding:15px;background:#fff;border-radius:8px;max-width:600px;text-align:left;box-shadow:0 1px 4px rgba(0,0,0,0.15);min-height:40px;white-space:pre-wrap;}
</style>
</head>
<body>
  <h2>ESP32-S3 AI Camera</h2>
  <p>Modul kamera OV3660 + analisis objek berbasis AI</p>
  <img id="preview" src="/capture" alt="pratinjau kamera">
  <br>
  <button id="btnAnalisis" onclick="analisisGambar()">Analisis dengan AI</button>
  <div id="hasil">Hasil analisis AI akan muncul di sini...</div>

<script>
  // Memperbarui gambar pratinjau tiap 2 detik. Ini BUKAN video streaming
  // asli (MJPEG), melainkan foto yang diperbarui berkala secara bergantian
  // supaya program ini tetap sederhana untuk dipelajari.
  setInterval(function () {
    document.getElementById('preview').src = '/capture?t=' + new Date().getTime();
  }, 2000);

  function analisisGambar() {
    var btn = document.getElementById('btnAnalisis');
    var hasil = document.getElementById('hasil');
    btn.disabled = true;
    hasil.innerText = 'Mengambil foto dan mengirim ke AI, mohon tunggu...';

    fetch('/analyze')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        hasil.innerText = data.ok ? data.result : ('Gagal: ' + data.result);
      })
      .catch(function (err) {
        hasil.innerText = 'Terjadi kesalahan: ' + err;
      })
      .finally(function () {
        btn.disabled = false;
      });
  }
</script>
</body>
</html>
)====";

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

// ---------------------------------------------------------------------------
// Endpoint: ambil satu foto dari kamera dan kirim sebagai gambar JPEG
// ---------------------------------------------------------------------------
void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Gagal mengambil gambar dari kamera");
    return;
  }
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ---------------------------------------------------------------------------
// Endpoint: ambil foto, kirim ke Gemini Vision API, kembalikan hasilnya
// sebagai JSON ke halaman web
// ---------------------------------------------------------------------------
void handleAnalyze() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "application/json",
                "{\"ok\":false,\"result\":\"Gagal mengambil gambar dari kamera\"}");
    return;
  }

  // Ubah data JPEG menjadi teks Base64 agar bisa disisipkan ke dalam JSON
  String imageBase64 = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (WiFi.status() != WL_CONNECTED) {
    server.send(500, "application/json",
                "{\"ok\":false,\"result\":\"WiFi tidak terhubung\"}");
    return;
  }

  // --- Susun isi permintaan (request body) sesuai format Gemini API ---
  JsonDocument reqDoc;
  JsonArray contents  = reqDoc["contents"].to<JsonArray>();
  JsonObject content0 = contents.add<JsonObject>();
  JsonArray parts     = content0["parts"].to<JsonArray>();

  JsonObject textPart = parts.add<JsonObject>();
  textPart["text"] = PROMPT_AI;

  JsonObject imagePart   = parts.add<JsonObject>();
  JsonObject inlineData  = imagePart["inline_data"].to<JsonObject>();
  inlineData["mime_type"] = "image/jpeg";
  inlineData["data"]      = imageBase64;

  String requestBody;
  serializeJson(reqDoc, requestBody);

  // --- Kirim ke Gemini API lewat HTTPS ---
  WiFiClientSecure client;
  client.setInsecure(); // menyederhanakan koneksi TLS untuk keperluan belajar

  HTTPClient https;
  String url = String("https://generativelanguage.googleapis.com/v1beta/models/") +
               GEMINI_MODEL + ":generateContent?key=" + GEMINI_API_KEY;

  String resultText = "";
  bool ok = false;

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");
    https.setTimeout(20000);
    client.setTimeout(20000);
    int httpCode = https.POST(requestBody);
    String response = https.getString();

    if (httpCode == 200) {

      JsonDocument resDoc;
      DeserializationError parseErr = deserializeJson(resDoc, response);

      if (!parseErr) {
        const char* text = resDoc["candidates"][0]["content"]["parts"][0]["text"];
        if (text) {
          resultText = String(text);
          ok = true;
        } else {
          resultText = "Format respons AI tidak sesuai dugaan.";
        }
      } else {
        resultText = "Gagal membaca JSON dari server AI.";
      }
    } else {
      resultText = "Permintaan ke server AI gagal, kode HTTP: " + String(httpCode);
    }
    https.end();
  } else {
    resultText = "Tidak dapat terhubung ke server AI.";
  }

  JsonDocument outDoc;
  outDoc["ok"]     = ok;
  outDoc["result"] = resultText;
  String outStr;
  serializeJson(outDoc, outStr);

  server.send(200, "application/json", outStr);
}

// ---------------------------------------------------------------------------
// Setup & loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println();

  if (!initCamera()) {
    Serial.println("Program dihentikan karena kamera gagal diinisialisasi.");
    return;
  }
  Serial.println("Kamera berhasil diinisialisasi.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Terhubung! Alamat IP board: ");
  Serial.println(WiFi.localIP());
  Serial.println("Buka alamat IP di atas pada browser (perangkat harus satu jaringan WiFi).");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/analyze", HTTP_GET, handleAnalyze);
  server.begin();
  Serial.println("Web server berjalan.");
}

void loop() {
  server.handleClient();
}
