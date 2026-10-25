#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

// Pin SPI (VSPI)
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define SS_PIN 5

// Pin LED
#define LED_HIJAU 26
#define LED_MERAH 27

// UID kartu yang diizinkan
// const String UID_TERDAFTAR[] = {"01 02 03 04", "A1 B2 C3 D4", "11 22 33 44",};
// const String LIST_UID_TERDAFTAR = sizeof(UID_TERDAFTAR) / sizeof(UID_TERDAFTAR[0]);

const String UID_TERDAFTAR = "01 02 03 04";
const String LIST_UID_TERDAFTAR[3] =  {"01 02 03 04", "A1 B2 C3 D4", "11 22 33 44"};

// Objek driver dan pembaca RFID
MFRC522DriverPinSimple ss_pin(SS_PIN);
SPIClass &spiClass = SPI;
const SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE0);
MFRC522DriverSPI driver{ss_pin, spiClass, spiSettings};
MFRC522 rfid{driver};

void setup() {
  Serial.begin(115200);

  pinMode(LED_HIJAU, OUTPUT);
  pinMode(LED_MERAH, OUTPUT);

  // Atur pin SPI sebelum modul RFID diinisialisasi
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  MFRC522Debug::PCD_DumpVersionToSerial(rfid, Serial);

  Serial.println("Tempelkan kartu RFID...");
}

void loop() {
  // Tunggu sampai ada kartu baru dan UID berhasil dibaca
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Ubah UID menjadi teks, contoh: "A1 B2 C3 D4"
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10)
      uid += "0";

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1)
      uid += " ";
  }

  uid.toUpperCase();

  // Tampilkan UID
  Serial.print("UID kartu: ");
  Serial.println(uid);
  int card_state = 0;

  for (int i = 0; i < 3; i++){
    // Periksa hak akses
    if (uid == LIST_UID_TERDAFTAR[i]) {
      Serial.println("Akses DITERIMA");
      digitalWrite(LED_HIJAU, HIGH);
      card_state = 1;
      break;
    }
  }
  if (card_state == 0){
    Serial.println("Akses DITOLAK");
    digitalWrite(LED_MERAH, HIGH);
  }


  delay(1500);

  digitalWrite(LED_HIJAU, LOW);
  digitalWrite(LED_MERAH, LOW);

  // Mengakhiri komunikasi dengan kartu
  rfid.PICC_HaltA();
  
}