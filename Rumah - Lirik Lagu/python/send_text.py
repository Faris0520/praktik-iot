import time
import serial
from serial.tools import list_ports

ports = list(list_ports.comports())

if not ports:
    print("Port ESP32 tidak ditemukan.")
    raise SystemExit(1)

print("Daftar port serial:")
for i, port in enumerate(ports):
    print(f"{i}. {port.device} - {port.description}")

try:
    choice = int(input("Pilih nomor port ESP32: "))
    selected_port = ports[choice].device
except (ValueError, IndexError):
    print("Pilihan port tidak valid.")
    raise SystemExit(1)

try:
    with serial.Serial(
        selected_port,
        115200,
        timeout=1
    ) as esp:
        # ESP32 biasanya restart saat port dibuka
        time.sleep(2)

        print("\nESP32 terhubung.")
        print("Ketik teks untuk ditampilkan di OLED.")
        print("Ketik 'exit' untuk keluar.\n")

        while True:
            text = input("Teks: ")

            if text.strip().lower() == "exit":
                break

            esp.write((text + "\n").encode("utf-8"))
            print("Teks dikirim ke ESP32.")

except serial.SerialException as error:
    print(f"Gagal membuka port serial: {error}")